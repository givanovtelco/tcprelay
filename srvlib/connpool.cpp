#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <unistd.h>

#include "connpool.h"

struct pool
{
	int _fd;
	int _sfd;
	epoll_event _ev;
	epoll_event *_events;
};

ConnPool::ConnPool(const remote& remote, int max)
	: _remote(remote), _evsize(max)
{
	_pool = new pool;
	_pool->_events = new epoll_event[max];
}

ConnPool::~ConnPool()
{
	int lsize = _evsize;
	for (int i = 0; i < lsize; i++)
		del_event(_pool->_events[i].data.fd);
	delete _pool->_events;
	delete _pool;
}

int ConnPool::init()
{
	if ((_pool->_fd = epoll_create1(0)) < 0)
		return -1;

	sigset_t sigset;
	if (sigemptyset(&sigset))
		return -1;
	if (sigaddset(&sigset, SIGUSR1))
		return -1;
	if (sigprocmask(SIG_BLOCK, &sigset, nullptr))
		return -1;
	if ((_pool->_sfd = signalfd(-1, &sigset, 0)) < 0)
		return -1;

	if (add_event(_pool->_sfd))
		return -1;

	int lsize = _evsize;

	for (int i = 0; i < lsize; i++)
	{
		int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (fd < 0)
		{
			perror("Socket");
			return -1;
		}

		if (config_fd(fd))
			return -1;

		if (connect(fd))
			return -1;

		if (add_event(fd))
			return -1;
	}
	return 0;
}

void ConnPool::run(std::function<void (int)>& cb)
{
	while(1)
	{
		int idx, num;
		num = epoll_wait(_pool->_fd, _pool->_events, _evsize, -1);

		for (int i = 0; i < num; i++)
		{
			epoll_event ev = _pool->_events[i];

			int fd = ev.data.fd;

			if (ev.events & EPOLLERR ||
					ev.events & EPOLLHUP ||
					(!ev.events & EPOLLIN) )
			{
				shutdown(fd, SHUT_RDWR);
				continue;
			}

			if (fd == _pool->_sfd)
				return; // exit signal from the main thread.

			cb(fd);
		}
	}
	return;
}

int ConnPool::connect(int fd)
{
	struct sockaddr_in caddr;
	memset(&caddr, 0, sizeof(sockaddr_in));

	caddr.sin_family = AF_INET;
	caddr.sin_port = htons(_remote._port);
	caddr.sin_addr.s_addr = inet_addr(_remote._addr);
	return ::connect(fd, (struct sockaddr *)&caddr, sizeof(struct sockaddr));
}

int ConnPool::config_fd(int fd)
{
	int alive = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &alive, sizeof(alive)))
		return -1;
	int idle = 1;
	if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)))
		return -1;

	return 0;
}

int ConnPool::add_event(int fd)
{
	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = EPOLLIN | EPOLLET;
	return epoll_ctl(_pool->_fd, EPOLL_CTL_ADD, fd, &ev);
}

int ConnPool::del_event(int fd)
{
	return epoll_ctl(_pool->_fd, EPOLL_CTL_DEL, fd, &_pool->_ev);
}


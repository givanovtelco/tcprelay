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
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <unistd.h>
#include <array>
#include <cassert>

#include "relay.h"

#define MAX_EVENTS 2048
#define MAX_BUFF 2048
struct events
{
	int _fd;
	int _sfd;
	epoll_event _ev;
	std::array<epoll_event, MAX_EVENTS> _maxev;
};

EventQueue::EventQueue(const relay& params)
:_params(params)
{
	_evs = new events;
}

int EventQueue::init()
{
	if ((_evs->_fd = epoll_create1(0)) < 0)
		return -1;

	sigset_t sigset;
	if (sigemptyset(&sigset))
		return -1;
	if (sigaddset(&sigset, SIGUSR1))
		return -1;
	if (sigprocmask(SIG_BLOCK, &sigset, nullptr))
		return -1;
	if ((_evs->_sfd = signalfd(-1, &sigset, 0)) < 0)
		return -1;

	evdata userdata;
	userdata._fd = _evs->_sfd;
	userdata._state = LISTENER;
	userdata._fn = nullptr;

	if (add_event(_evs->_sfd, userdata))
		return -1;

	if (init_sockets())
		return -1;

	return 0;
}

int EventQueue::init_sockets()
{
	std::vector<int> srvfds;
	if (spawn_listeners(_params._srvports, srvfds))
		return -1;

	std::vector<evdata> cbs;

	for (const auto &elem : srvfds)
	{
		evdata event;
		event._fd = elem;
		event._state = LISTENER;
		event._fn = [this](int arg) -> int {
			return this->accept_upstream(arg);
		};
		cbs.push_back(event);
	}

	if (conf_listeners(srvfds, cbs))
		return -1;
	return 0;
}

EventQueue::~EventQueue()
{
	stop();
	delete _evs;
}

int EventQueue::conf_listeners(const std::vector<int>& sockets, const std::vector<evdata>& cbs)
{

	assert(sockets.size() == cbs.size());
	int sz = sockets.size();
	for (int i = 0; i < sz; i++)
	{

	}
	return 0;
}

int EventQueue::spawn_listeners(const std::vector<uint16_t>& ports, std::vector<int>& sockets)
{

	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = inet_addr(_params._addr);

	int sz = ports.size();

	for (int i = 0; i < sz; i++)
	{
		int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
		if (sock < 0)
			return -1;

		int opt = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

		saddr.sin_port = htons(ports[i]);
		if (bind(sock, (struct sockaddr *)&saddr, sizeof(saddr)))
			return -1;

		if (listen(sock, 1024))
			return -1;

		sockets.push_back(sock);
	}

	return 0;
}

void EventQueue::dispatch()
{
	int idx, num;
	num = epoll_wait(_evs->_fd, _evs->_maxev.begin(), MAX_EVENTS, -1);

	for (int i = 0; i < num; i++)
	{
		epoll_event ev = _evs->_maxev[i];
		evdata *user = (evdata*)ev.data.ptr;

		if (!user)
			continue;

		int fd = user->_fd;

		if (ev.events & EPOLLERR ||
				ev.events & EPOLLHUP ||
				(!ev.events & EPOLLIN) )
		{
			shutdown(user->_fd, SHUT_RDWR);
			continue;
		} 

		if (fd == _evs->_sfd)
			return; // exit signal from the main thread.

		user->_fn(fd);
	}
}

int EventQueue::add_event(int fd, const evdata& data)
{
	struct epoll_event ev;
	memmove(ev.data.ptr, (evdata *)&data, sizeof(data));
	ev.events = EPOLLIN | EPOLLET;
	return epoll_ctl(_evs->_fd, EPOLL_CTL_ADD, fd, &ev);
}

int EventQueue::destroy_event(int fd_event)
{
	return epoll_ctl(_evs->_fd, EPOLL_CTL_DEL, fd_event, &_evs->_ev);
}

void EventQueue::stop()
{
	for (auto &elem : _evs->_maxev)
		destroy_event(elem.data.fd);
	close(_evs->_fd);
	close(_evs->_sfd);
}

int EventQueue::make_nonblocking(int fd)
{
	int flags, s;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return -1;

	flags |= O_NONBLOCK;
	s = fcntl(fd, F_SETFL, flags);
	if (s == -1)
		return -1;

	return 0;
}

int EventQueue::accept_upstream(int fd)
{
	struct sockaddr in_addr;
	socklen_t in_len;
	int infd;

	in_len = sizeof(in_addr);
	infd = accept(fd, &in_addr, &in_len);

	if (infd < 0)
		return -1;

	int zero = 1;
	setsockopt(infd, SOL_SOCKET, SO_ZEROCOPY, &zero, sizeof(zero));

	int alive = 1;
	setsockopt(infd, SOL_SOCKET, SO_KEEPALIVE, &alive, sizeof(alive));
	int idle = 1;
	setsockopt(infd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
	int interval = 5;
	setsockopt(infd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));

	struct evdata ev;
	ev._fd = infd;
	ev._state = PRODUCER;
	ev._fn = [this](int arg) -> int { return this->forward_downstream(arg); };

	if (_mutils.add_upstream(infd))
		return -1;

	if (add_event(infd, ev))
		return -1;

	return 0;
}

int EventQueue::accept_downstream(int fd)
{
	struct sockaddr in_addr;
	socklen_t in_len;
	int infd;

	in_len = sizeof(in_addr);
	infd = accept(fd, &in_addr, &in_len);

	if (infd < 0)
		return -1;

	int zero = 1;
	setsockopt(infd, SOL_SOCKET, SO_ZEROCOPY, &zero, sizeof(zero));

	struct evdata ev;
	ev._fd = infd;
	ev._state = PRODUCER;
	ev._fn = [this](int arg) -> int { return this->forward_upstream(arg); };

	if (_mutils.add_downstream(infd))
		return -1;

	if (add_event(infd, ev))
		return -1;

	return 0;
}

int EventQueue::forward_upstream(int fd)
{
	int upfd = -1;
	if ((upfd  = _mutils.add_upstream(fd)) == -1)
		return -1;

	if (do_forward(fd, upfd))
		return -1;

	return 0;
}

int EventQueue::forward_downstream(int fd)
{
	int downfd = -1;
	if ((downfd  = _mutils.add_upstream(fd)) == -1)
		return -1;

	if (do_forward(fd, downfd))
		return -1;

	return 0;
}

int EventQueue::do_forward(int src, int dst)
{
	char buf[MAX_BUFF];
	struct cmsghdr *cmsg;
	struct msghdr msg;
	struct iovec vec[1];
	struct sockaddr_in sin;
	memset(&msg,   0, sizeof(msg));
	memset(vec,    0, sizeof(vec));
	memset(buf, 0, sizeof(buf));

	vec[0].iov_base = buf;
	vec[0].iov_len  = sizeof(buf);
	msg.msg_name = &sin;
	msg.msg_namelen = sizeof(sin);
	msg.msg_iov     = vec;
	msg.msg_iovlen  = 1;
	msg.msg_control = 0;
	msg.msg_controllen = 0;

	int len = recvmsg(src, &msg, 0);

	if (len == -1)
		return -1;

	if (sendmsg(dst, &msg, MSG_ZEROCOPY) == -1)
		return -1;

	return 0;
}

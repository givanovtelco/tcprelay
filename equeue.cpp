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
#include <syslog.h>

#include <array>
#include <cassert>
#include <memory>

#include "relay.h"
#include "threadpool.h"

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
	_tpool = new ThreadPool(std::thread::hardware_concurrency() + 1);
    _tpool->init();

}

int EventQueue::init()
{
	if ((_evs->_fd = epoll_create1(0)) < 0)
	{
		syslog(LOG_ERR, "%s", "Epoll create error");
		return -1;
	}
	sigset_t sigset;
	if (sigemptyset(&sigset))
		return -1;
	if (sigaddset(&sigset, SIGUSR1))
		return -1;
	if (sigprocmask(SIG_BLOCK, &sigset, nullptr))
		return -1;
	if ((_evs->_sfd = signalfd(-1, &sigset, 0)) < 0)
	{
		syslog(LOG_ERR, "%s", "Signal fd create error");
		return -1;
	}
	evdata *userdata = new evdata;
	userdata->_fd = _evs->_sfd;
	userdata->_state = LISTENER;
	userdata->_fn = nullptr;

	if (add_event(_evs->_sfd, userdata))
	{
		syslog(LOG_ERR, "%s", "Error adding event");
		return -1;
	}
	if (init_sockets(_params._srvports))
	{
		syslog(LOG_ERR, "%s %s", "Init socket error", strerror(errno));
		return -1;
	}
	if (init_config())
	{
		syslog(LOG_ERR, "%s %s", "Configuration socket create error", strerror(errno));
		return -1;
	}

	return 0;
}

int EventQueue::init_config()
{
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, _params._cfg, SUN_PATH);

	if ( (_cfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		return -1;

	if (bind(_cfd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
		return -1;

	if (listen(_cfd, 1) == -1)
		return -1;

	evdata *event = new evdata;
	event->_fd = _cfd;
	event->_state = LISTENER;
	event->_fn = [this](int arg) -> int {
		return this->cfg_accept(arg);
	};

	add_event(_cfd, event);

	return 0;
}

int EventQueue::init_sockets(const std::vector<uint16_t>& ports)
{
	std::vector<int> srvfds;
	if (spawn_listeners(ports, srvfds))
		return -1;

	std::vector<evdata*> cbs;

	for (const auto &elem : srvfds)
	{
		evdata *event = new evdata;
		event->_fd = elem;
		event->_state = LISTENER;
		event->_fn = [this](int arg) -> int {
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

int EventQueue::conf_listeners(const std::vector<int>& sockets, const std::vector<evdata*>& cbs)
{
	assert(sockets.size() == cbs.size());
	int sz = sockets.size();
	for (int i = 0; i < sz; i++)
		add_event(sockets[i], cbs[i]);

	return 0;
}

int EventQueue::spawn_client(uint16_t port)
{
	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = inet_addr(_params._addr);
	saddr.sin_port = htons(port);

	int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if (sock < 0)
		return -1;
	int opt = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
	if (bind(sock, (struct sockaddr *)&saddr, sizeof(saddr)))
	{
		syslog(LOG_ERR, "%s\n", strerror(errno));
		return -1;
	}
	if (listen(sock, 1024))
		return -1;

	evdata *event = new evdata;
	event->_fd = sock;
	event->_state = LISTENER;
	event->_fn = [this](int arg) -> int {
		return this->accept_downstream(arg);
	};

	if (add_event(sock, event))
		return -1;

	return 0;
}

int EventQueue::spawn_listeners(const std::vector<uint16_t>& ports, std::vector<int>& sockets)
{

	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(sockaddr_in));
	saddr.sin_family = AF_INET;
	inet_aton(_params._addr, &saddr.sin_addr);

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
		{
			syslog(LOG_ERR, "%s", strerror(errno));
			return -1;
		}

		if (listen(sock, 1024))
			return -1;

		sockets.push_back(sock);
	}

	return 0;
}

void EventQueue::run()
{
	while (1)
	{
		int idx, num;
		num = epoll_wait(_evs->_fd, _evs->_maxev.begin(), MAX_EVENTS, -1);

		for (int i = 0; i < num; i++)
		{
			epoll_event ev = _evs->_maxev[i];
			evdata *user = (evdata*)ev.data.ptr;

			int fd;
			if (user)
				fd = user->_fd;
			else
				fd = ev.data.fd;

			if (ev.events & EPOLLRDHUP ||
					(!ev.events & EPOLLIN) )
			{
				// TODO: unmap connection
				close(fd);
				del_event(fd);
				continue;
			}

			if (fd == _evs->_sfd)
				return; // exit signal from the main thread.

			// TODO: pull out the error in order to make decision what to do
			int ret = user->_fn(fd);
		}
	}
}


int EventQueue::add_event(int fd,  evdata* data)
{
	make_nonblocking(fd);
	struct epoll_event ev;
	memset(&ev, 0, sizeof(epoll_event));

	if (data)
		ev.data.ptr = data;
	else
		ev.data.fd = fd;

	ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	return epoll_ctl(_evs->_fd, EPOLL_CTL_ADD, fd, &ev);
}

int EventQueue::del_event(int fd)
{
	return epoll_ctl(_evs->_fd, EPOLL_CTL_DEL, fd, &_evs->_ev);
}

void EventQueue::stop()
{
	for (auto &elem : _evs->_maxev) {
		del_event(elem.data.fd);
		if (elem.data.ptr)
			delete static_cast<evdata*>(elem.data.ptr);
	}
	close(_evs->_fd);
	close(_evs->_sfd);
	unlink(_params._cfg);
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

int EventQueue::cfg_execute(int fd)
{
	char buf[1024];

	memset(buf, 0, sizeof(buf));
	int bytes = recv(fd, buf, sizeof(buf), 0);
	if (bytes < 0 )
		return -1;
	if (bytes == 0)
		return -1;

	int port;
	if ((port = _cutils.parse_cmd(buf, sizeof(buf))) == -1)
		return -1;

	if (cfg_helper(port, fd))
		return -1;
	return 0;
}

int EventQueue::cfg_helper(int port, int fd)
{
	std::vector<uint16_t>  ports;
	std::vector<int>  sockets;
	std::vector<evdata *> cbs;

	ports.push_back(port);

	if (init_sockets(ports))
		return -1;
	char resp[5];
	if (spawn_listeners(ports, sockets))
	{
		snprintf(resp, 4, "%s", "NOK");
		send(fd, resp, sizeof(resp), 0);
		return -1;
	}
	evdata *event = new evdata;
	event->_fd = sockets[0];
	event->_state = LISTENER;
	event->_fn = [this](int arg) -> int {
		return this->accept_upstream(arg);
	};

	cbs.push_back(event);

	if (conf_listeners(sockets, cbs))
	{
		snprintf(resp, 4, "%s", "NOK");
		send(fd, resp, sizeof(resp), 0);
		return -1;
	}

	snprintf(resp, 3, "%s", "OK");
	send(fd, resp, sizeof(resp), 0);

	return 0;
}

int EventQueue::cfg_accept(int fd)
{
	int cfd = accept(fd, NULL, NULL);
	if (cfd < 0)
		return -1;
	evdata *ev =  new evdata;
	ev->_fd = cfd;
	ev->_state = PRODUCER;
	ev->_fn = [this](int arg) -> int { return this->cfg_execute(arg); };

	if (add_event(cfd, ev))
		return -1;

	return 0;
}

int EventQueue::accept_upstream(int fd)
{
	struct sockaddr in_addr;
	struct sockaddr_in sinfo;
	socklen_t len = sizeof(sockaddr_in);

	socklen_t in_len;
	int infd;

	in_len = sizeof(in_addr);
	infd = accept(fd, &in_addr, &in_len);

	if (infd < 0)
		return -1;

	memset(&sinfo, 0, sizeof(sockaddr_in));

	uint16_t port;

	if (getsockname(fd, (struct sockaddr *)&sinfo, &len) == -1)
		return -1;
	port = htons(sinfo.sin_port);
	syslog(LOG_INFO, "%s %s", "Upstream accepted from: ", inet_ntoa(sinfo.sin_addr));

	int zero = 1;
	setsockopt(infd, SOL_SOCKET, SO_ZEROCOPY, &zero, sizeof(zero));

	int alive = 1;
	setsockopt(infd, SOL_SOCKET, SO_KEEPALIVE, &alive, sizeof(alive));
	int idle = 1;
	setsockopt(infd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
	int interval = 5;
	setsockopt(infd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));

	evdata *ev =  new evdata;
	ev->_fd = infd;
	ev->_state = PRODUCER;
	ev->_fn = [this](int arg) -> int { return this->forward_downstream(arg); };

	if (_mutils.add_upstream(infd))
		return -1;

	if (add_event(infd, ev))
		return -1;

	if (_mutils.find_dacceptor(infd))
		spawn_client(port - _params._prefix);

	return 0;
}

int EventQueue::accept_downstream(int fd)
{
	struct sockaddr in_addr;
	struct sockaddr_in sinfo;
	socklen_t len = sizeof(sockaddr_in);

	socklen_t in_len;
	int infd;

	in_len = sizeof(in_addr);
	infd = accept(fd, &in_addr, &in_len);

	if (infd < 0)
		return -1;

	int zero = 1;
	setsockopt(infd, SOL_SOCKET, SO_ZEROCOPY, &zero, sizeof(zero));

	if (getsockname(fd, (struct sockaddr *)&sinfo, &len) == -1)
		return -1;
	syslog(LOG_INFO, "%s %s", "Downstream accepted from: ", inet_ntoa(sinfo.sin_addr));

	evdata *ev = new evdata;
	ev->_fd = infd;
	ev->_state = PRODUCER;
	ev->_fn = [this](int arg) -> int { return this->forward_upstream(arg); };

	if (_mutils.add_downstream(infd))
		return -1;

	if (add_event(infd, ev))
		return -1;

	return 0;
}

int EventQueue::forward_upstream(int fd)
{
	auto res = _tpool->push([this](int fd) mutable {
		int upfd = -1;
		if ((upfd  = _mutils.find_upstream(fd)) == -1)
			return -1;
		if (do_forward(fd, upfd))
			return -1;
		return 0;
	}, fd);

	return res.get();
}

int EventQueue::forward_downstream(int fd)
{
	auto res = _tpool->push([this](int fd) mutable {
		int downfd = -1;
		if ((downfd  = _mutils.find_downstream(fd)) == -1)
			return -1;

		if (do_forward(fd, downfd))
			return -1;

		return 0;
	}, fd);
	return res.get();
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
	if (len == 0)
		return -1;

	if (sendmsg(dst, &msg, MSG_ZEROCOPY) == -1)
		return -1;

	return 0;
}

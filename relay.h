#ifndef RELAY_H__
#define RELAY_H__

#include <vector>
#include <unordered_map>
#include <functional>

#define ADDRLEN 46

struct events;

enum error
{
	UPSTREAM_DOWN,
	DOWNSTREAM_DOWN,
	UNKNOWN_LISTENER,
	INTERNAL_ERROR,
};

enum evstate
{
      LISTENER = 1 << 0,
      PRODUCER = 1 << 1,

};

struct evdata
{
	int _fd;
	evstate _state;
	std::function<int (int)> _fn;
};

struct connmap
{
	std::vector<int> _upstream;
	std::vector<int> _downstream;
};

struct bdkeys
{
	std::unordered_map<int, int> _upkey;
	std::unordered_map<int, int> _downkey;
};

struct listeners
{
	std::vector<int> _srv;
	std::vector<int> _cli;
};

struct relay
{
	char _addr[ADDRLEN];
	std::vector<uint16_t> _clports;
	std::vector<uint16_t> _srvports;
};

class MapUtils
{
public:
	MapUtils();
	int find_upstream(int fd);
	int find_downstream(int fd);
	int find_dacceptor(int fd);
	int find_uacceptor(int fd);
	int add_downstream(int fd);
	int add_upstream(int fd);
private:
	listeners _lscoks;
	connmap _map;
	bdkeys _keys;
};

class EventQueue
{
public:
	EventQueue(const relay& params);
	~EventQueue();
	int init();
	// infinite loop
	void dispatch();
	void stop();
	int add_event(int fd, const evdata& udata);
	int destroy_event(int fd_event);
	int make_nonblocking(int fd);
private:
	int init_sockets();
	int spawn_listeners(const std::vector<uint16_t>& ports,std::vector<int>& sockets);
	int conf_listeners(const std::vector<int>& sockets, const std::vector<evdata>& cbs);
	int accept_upstream(int fd);
	int accept_downstream(int fd);
	int forward_upstream(int fd);
	int forward_downstream(int fd);
private:
	events *_evs;
	relay _params;
	MapUtils _mutils;
};
#endif
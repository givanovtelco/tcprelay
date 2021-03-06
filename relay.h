#ifndef RELAY_H__
#define RELAY_H__

#include <vector>
#include <unordered_map>
#include <functional>
#include <queue>
#include <cstdarg>

#include <limits.h>

#define ADDRLEN 46
#define SUN_PATH 108 /* based on sys/un.h structure */

struct events;
class ThreadPool;

enum evstate
{
	LISTENER = 1 << 0,
	PRODUCER = 1 << 1,
};

/**
 * Event data structure.
 */
struct evdata
{
	int _fd;
	evstate _state;
	std::function<int (int)> _fn;
};

/**
 * The actual stream mapping
 */
struct connmap
{
	std::vector<int> _upstream;
	std::vector<int> _downstream;
};

/**
 * Bi-directional keys pointing to the stream connections.
 */
struct bdkeys
{
	std::unordered_map<int, int> _upkey;
	std::unordered_map<int, int> _downkey;
};

/**
 * Listener sockets.
 */
struct listeners
{
	std::vector<int> _srv;
	std::vector<int> _cli;
};

/**
 * Listener keys.
 */
struct lkeys
{
	std::unordered_map<int, int> _lup;
	std::unordered_map<int, int> _ldown;
};

/**
 * this struct represents initial params.
 */
struct relay
{
	std::vector<uint16_t> _clports;
	std::vector<uint16_t> _srvports;
	char _addr[ADDRLEN];
	char _cfg[SUN_PATH];
	uint16_t _prefix;
};

/**
 * This class represents mapping between stream connections and listeners.
 */
class MapUtils
{
public:
	MapUtils();
	int find_upstream(int fd);
	int find_downstream(int fd);
	int find_dacceptor(int fd);
	int find_uacceptor(int fd);
	int add_uacceptor(int fd);
	int add_dacceptor(int fd);
	int add_downstream(int fd);
	int add_upstream(int fd);
	void del_upstream(int fd);
	void del_downstream(int fd);
private:
	listeners _lsocks;
	connmap _map;
	bdkeys _keys;
	lkeys _lkeys;
};

/**
 * This class is used to parse request from unix domain socket.
 */
class CfgUtils
{
public:
	CfgUtils() = default;
	int parse_cmd(const char *src, int slen);
};

/*
 * This class represents epoll handling.
 */

class EventQueue
{
public:
	EventQueue(const relay& params);
	~EventQueue();
	int init();
	// infinite loop
	void run();
	void stop();
	int add_event(int fd, evdata* udata);
	int del_event(int fd);
	int make_nonblocking(int fd);
private:
	int init_config();
	int init_sockets(const std::vector<uint16_t>& ports);
	int spawn_listeners(const std::vector<uint16_t>& ports, std::vector<int>& sockets);
	int conf_listeners(const std::vector<int>& sockets, const std::vector<evdata*>& cbs);
	int spawn_client(uint16_t port);
private:
	int accept_upstream(int fd);
	int accept_downstream(int fd);
	int forward_upstream(int fd);
	int forward_downstream(int fd);
	int do_forward(int src, int dst);
	int cfg_execute(int fd);
	int cfg_helper(int port, int fd);
	int cfg_accept(int fd);
private:
	int _cfd;
	events *_evs;
	ThreadPool *_tpool;
	relay _params;
	MapUtils _mutils;
	CfgUtils _cutils;
};
#endif

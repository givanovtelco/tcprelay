#ifndef CONNPOOL_H__
#define CONNPOOL_H__

#include <functional>
#include <stdint.h>

#define ADDRLEN 46

/**
 * Represents tcprelay relation.
 */
struct remote
{
	int _fd;
	uint16_t _port;
	char _addr[ADDRLEN];
};

struct pool;

/**
 * Connection pool is epoll implementation as event driven mechanism
 */
class ConnPool
{
public:
	ConnPool(const remote& remote, int max);
	~ConnPool();
	int init();
	void run(std::function<void (int)>& cb);
private:
	int connect(int fd);
	int config_fd(int fd);
	int add_event(int fd);
	int del_event(int fd);
private:
	remote _remote;
	pool *_pool;
	int _evsize;
};


#endif

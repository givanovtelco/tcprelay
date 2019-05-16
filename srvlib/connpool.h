#ifndef CONNPOOL_H__
#define CONNPOOL_H__

#include <stdint.h>

#define ADDRLEN 46

struct remote
{
	int _fd;
	uint16_t _port;
	char _addr[ADDRLEN];
};

class ConnPool
{
public:
	ConnPool(int min, int max);
private:
};


#endif

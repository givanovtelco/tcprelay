#include "relay.h"


MapUtils::MapUtils()
{
}

int MapUtils::find_upstream(int fd)
{
	auto it = _keys._downkey.find(fd);
	if (it != _keys._downkey.end())
		return _map._upstream[it->second];
	return -1;
}

int MapUtils::find_downstream(int fd)
{
	auto it = _keys._upkey.find(fd);
	if (it != _keys._upkey.end())
		return _map._downstream[it->second];
	return -1;
}

int MapUtils::find_dacceptor(int fd)
{
	return 0;
}

int MapUtils::find_uacceptor(int fd)
{
	return 0;
}

int MapUtils::add_downstream(int fd)
{
	return 0;
}

int MapUtils::add_upstream(int fd)
{
	return 0;
}



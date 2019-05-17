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


int MapUtils::add_downstream(int fd)
{
	int idx = _map._downstream.size();
	_map._downstream.push_back(fd);
	auto ret = _keys._downkey.insert(std::make_pair(fd, idx));
	if (ret.second)
		return 0;
	return -1;
}

int MapUtils::add_upstream(int fd)
{
	int idx = _map._upstream.size();
	_map._upstream.push_back(fd);
	auto ret = _keys._upkey.insert(std::make_pair(fd, idx));
	if (ret.second)
		return 0;
	return -1;
}



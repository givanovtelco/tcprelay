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

int MapUtils::find_dacceptor(int fd)
{
	auto it = _lkeys._ldown.find(fd);
	if (it != _lkeys._ldown.end())
		return _lsocks._cli[it->second];
	return -1;
}

int MapUtils::find_uacceptor(int fd)
{
	auto it = _lkeys._lup.find(fd);
	if (it != _lkeys._lup.end())
		return _lsocks._srv[it->second];
	return -1;
}

int MapUtils::add_uacceptor(int fd)
{
	int idx = _lsocks._srv.size();
	_lsocks._srv.push_back(fd);
	auto ret = _lkeys._lup.insert(std::make_pair(fd, idx));
	if (ret.second)
		return 0;
	return -1;
}

int MapUtils::add_dacceptor(int fd)
{
	int idx = _lsocks._cli.size();
	_lsocks._cli.push_back(fd);
	auto ret = _lkeys._ldown.insert(std::make_pair(fd, idx));
	if (ret.second)
		return 0;
	return -1;
}

void MapUtils::del_upstream(int fd)
{
	auto it = _keys._upkey.find(fd);
	if (it != _keys._upkey.end())
		_map._upstream[it->second] = -1;
}

void MapUtils::del_downstream(int fd)
{
	auto it = _keys._downkey.find(fd);
	if (it != _keys._downkey.end())
		_map._downstream[it->second] = -1;
}

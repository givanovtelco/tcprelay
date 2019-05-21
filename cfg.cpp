#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include "relay.h"

int CfgUtils::parse_cmd(const char *src, int slen)
{

	if (!src)
		return -1;

	char cmd[3];
	int port = -1;
	if (sscanf(src, "%s %d", cmd, &port) != 2)
	{
		syslog(LOG_ERR, "%s", "Invalid arguments");
		return -1;
	}
	if (strcmp(cmd, "add"))
	{
		syslog(LOG_ERR, "%s", "Invalid command");
		return -1;
	}
	return port;
}

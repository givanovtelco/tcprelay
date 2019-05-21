#include <string.h>
#include <stdlib.h>

#include "relay.h"

int CfgUtils::parse_cmd(const char *src, int slen, int *dst)
{

	if (!src || !dst)
		return -1;

	char cmd[3];
	if (sscanf(src, "%s %d", cmd, dst) != 2)
		return -1;

	if (strcmp(cmd, "add"))
		return -1;

	return 0;
}

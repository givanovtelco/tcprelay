#include <string.h>
#include <stdlib.h>
#include "relay.h"

int CfgUtils::parse_cmd(const char *src, int slen, int *dst, int dlen)
{

	if (!src || !dst)
		return -1;

	char cmd[3];
	char ports[dlen];
	memset(ports, 0, sizeof(ports));

	if (sscanf(src, "%s %s", cmd, ports) != 2)
		return -1;

	if (strcmp(cmd, "add"))
		return -1;

	char sep[2] = ",";
	char *tok = strtok(ports, sep);

	int idx = 0;
	while(tok != NULL)
	{
		if (idx < dlen)
		{
			dst[idx] = atoi(tok);
			tok = strtok(NULL, sep);
			idx++;
		}
	}

	dst[idx++] = '\0';

	return 0;
}

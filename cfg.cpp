#include <string.h>
#include "relay.h"

int CfgUtils::parse_cmd(const char *src, int slen, char *dst, int dlen)
{

	if (!src || !dst)
		return -1;

	char cmd[3];
	char ports[dlen];

	if (sscanf(src, "%s,%s", cmd, ports) != 2)
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
			dst[idx] = *tok;
			tok = strtok(NULL, sep);
			idx++;
		}
	}

	return 0;
}

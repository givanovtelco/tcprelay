#include <string.h>
#include "relay.h"

int CfgUtils::parse_cmd(const char *src, int slen, char *dst, int dlen)
{

	if (!src || !dst)
		return -1;

	char cmd[3];
	int pos = 0, length = 3;

	while (pos < length)
	{
		cmd[pos] = src[pos+length-1];
		pos++;
	}

	if (strncmp(cmd, "ADD", sizeof(cmd)))
		return -1;

	for (int idx = 2, j = 0; idx < slen; idx++)
	{
		if (!isspace(src[idx]))
			continue;
		if (!isdigit(src[idx]))
			continue;
		dst[j] = src[idx];
		j++;
	}

	return 0;
}

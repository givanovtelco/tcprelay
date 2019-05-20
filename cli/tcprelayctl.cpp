#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

static void help()
{
	printf("Usage: ./tcprelayctl [options]\n");
	printf(
			"\t--help\n"
			"\t\tshows this screen\n");

	printf(
			"\t--ports=20,22,80\n"
			"\t\tapp ports separated with comma\n");

	printf(
			"\t--sock=/full/path/to/cfg.sock\n"
			"\t\tconfig socket path file\n");

	printf(
			"\t--command=add\n"
			"\t\tspecifyng server operation (currently only add is supported)\n");

	exit(1);
}

int main(int argc, char *argv[])
{
	struct sockaddr_un addr;
	const char *sockpath = NULL;
	const char *ports = NULL;
	const char *cmd = "add";

	int fd;
	int flags, s;

	for (int idx = 1; idx < argc; idx++)
	{
		if (!strcmp(argv[idx], "--help"))
			help();

		int len = strlen("--ports=");
		if (!strncmp(argv[idx], "--ports=", len))
			ports = argv[idx] + len;

		len = strlen("--command=");
		if (!strncmp(argv[idx], "--command=", len))
			cmd = argv[idx] + len;

		len = strlen("--sock=");
		if (!strncmp(argv[idx], "--sock=", len))
			sockpath = argv[idx] + len;
	}

	if (!sockpath || !ports)
		help();

	if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	{
		perror("socket error");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockpath, sizeof(addr.sun_path));

	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return -1;

	flags |= O_NONBLOCK;
	s = fcntl(fd, F_SETFL, flags);
	if (s == -1)
		return -1;

	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
	{
		perror("connect error");
		exit(-1);
	}

	char buf[1024];
	snprintf(buf, sizeof(buf), "%s %s", cmd, ports);

	int bytes = send(fd, buf, sizeof(buf), MSG_DONTWAIT);

	if (bytes < 0)
	{
		fprintf(stderr, "%s\n", strerror(errno));
		return -1;
	}

	close(fd);

	return 0;
}




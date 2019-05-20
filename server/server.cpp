#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "connpool.h"

#define BUFFER_SIZE 1024

static void help()
{
    printf("Usage: ./echoserver [options]\n");
    printf(
    		"\t--help\n"
    		"\t\tshows this screen\n");

    printf(
    		"\t--address=192.168.0.1\n"
    		"\t\tremote address\n");

    printf(
    		"\t--port=1080\n"
    		"\t\tremote port\n");

    printf(
    		"\t--maxconn=\n"
    		"\t\tnumber of connections\n");

    exit(1);
}


int main(int argc, char *argv[])
{

	uint16_t port = 0;
	int maxconn = 1024;
	const char *address = NULL;

	for (int idx = 1; idx < argc; idx++)
	{
		if (!strcmp(argv[idx], "--help"))
			help();

		int len = strlen("--address=");
		if (!strncmp(argv[idx], "--address=", len))
			address = argv[idx] + len;

		len = strlen("--port=");
		if (!strncmp(argv[idx], "--port=", len))
			port = atoi(argv[idx] + len);

		len = strlen("--maxconn=");
		if (!strncmp(argv[idx], "--maxconn=", len))
			maxconn = atoi(argv[idx] + len);
	}

	if (!port || !address)
		help();
	remote rem;
	rem._port = port;
	strncpy(rem._addr, address, sizeof(rem._addr));

	ConnPool pool(rem, maxconn);
	if (!pool.init())
		return -1;

	std::function<void (int)> fn = [](int fd) {

		char buf[BUFFER_SIZE];
		memset(buf, 0, sizeof(buf));
		int read = recv(fd, buf, BUFFER_SIZE, MSG_DONTWAIT);

		if (read < 0)
		{
			perror("Error reading");
			return;
		}

		int res = send(fd, buf, read, MSG_DONTWAIT);
		if (res < 0)
		{
			perror("Error sending");
			return;
		}
	};

	pool.run(fn);
	return 0;
}


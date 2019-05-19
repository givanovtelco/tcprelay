#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <signal.h>

#include "relay.h"

static void sig_handler(int signum)
{
	switch(signum)
	{
	case SIGINT:
	case SIGHUP:
	{
		// sending signal to the event queue.
		kill(getpid(), SIGUSR1);
	}
	break;
	default:
		break;
	}
}

static void help() 
{
	printf("Usage: ./tcprelay [options]\n");
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
			"\t--prefix=1024\n"
			"\t\tprefix for server ports\n");

	printf(
			"\t--maxconn=1024\n"
			"\t\tmax accepted connections per port\n");

	printf(
			"\t--address=192.168.0.1\n"
			"\t\tlistening address\n");

	exit(1);
}


int main(int argc, char *argv[])
{
	/* Default configuration */
	uint16_t prefix = 1024;
	uint32_t maxconn = 1024;
	const char *ports = NULL;
	const char *addr = NULL;
	const char *sockpath = "/var/run/tcprelay.sock";

	relay rlparams;
	memset(&rlparams, 0, sizeof(rlparams));



	for (int idx = 1; idx < argc; idx++)
	{
		if (!strcmp(argv[idx], "--help"))
			help();

		int len = strlen("--ports=");
		if (!strncmp(argv[idx], "--ports=", len))
			ports = argv[idx] + len;

		len = strlen("--prefix=");
		if (!strncmp(argv[idx], "--prefix=", len))
			prefix = atoi(argv[idx] + len);

		len = strlen("--sock=");
		if (!strncmp(argv[idx], "--sock=", len))
			sockpath = argv[idx] + len;

		len = strlen("--maxconn=");
		if (!strncmp(argv[idx], "--maxconn=", len))
			maxconn = atoi(argv[idx] + len);

		len = strlen("--address=");
		if (!strncmp(argv[idx], "--address=", len))
			strncpy(rlparams._addr, argv[idx] + len, ADDRLEN);
	}

	struct sockaddr_in sa;
	if (!inet_pton(AF_INET, rlparams._addr, &(sa.sin_addr)))
		help();

//	pid_t pid;
//
//	pid = fork();
//
//	if (pid < 0)
//		exit(EXIT_FAILURE);
//
//	if (pid > 0)
//		exit(EXIT_SUCCESS);
//
//	if (setsid() < 0)
//		exit(EXIT_FAILURE);

	struct sigaction l_sa;

	l_sa.sa_handler = &sig_handler;
	// Restart the system call, if at all possible
	l_sa.sa_flags = SA_RESTART;

	// Block every signal during the handler
	sigfillset(&l_sa.sa_mask);

	if (sigaction(SIGHUP, &l_sa, NULL) == -1) {
		perror("Error: cannot handle SIGHUP.\n");
	}

	if (sigaction(SIGINT, &l_sa, NULL) == -1) {
		perror("Error: cannot handle SIGINT.\n");
	}

	// ignore SIGPIPE
	signal(SIGPIPE, SIG_IGN);

//	pid = fork();
//
//	if (pid < 0)
//		exit(EXIT_FAILURE);
//
//	if (pid > 0)
//		exit(EXIT_SUCCESS);

	/* Set new file permissions */
	umask(0);

	/* TODO: chroot daemon. Change the working directory to the root directory */
	/* or another appropriated directory */
	chdir("/");

	/* Close all open file descriptors */
	for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--)
	{
		close(x);
	}


	const char sep[2] = ",";
	char *tok;
	tok = strtok((char*)ports, sep);

	while(tok != NULL)
	{
		uint16_t clport = atoi(tok);
		rlparams._clports.push_back(clport);
		rlparams._srvports.push_back(clport + prefix);
		tok = strtok(NULL, sep);
	}

	strncpy(rlparams._cfg, sockpath, SUN_PATH);

	EventQueue queue(rlparams);
	if (queue.init())
		return -1;

	queue.dispatch();

	return 0;
}

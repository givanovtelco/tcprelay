#include <stdio.h>
#include <stdlib.h>

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
    		"\t--connnum=1024:2048\n"
    		"\t\tnumber of connections min:max\n");

    exit(1);
}

int main(char argc, char *argv[])
{
	int cmin = 1024, cmax = 2048;

}





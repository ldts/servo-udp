#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_SERVERS	2
#define BUFSIZE		1024
#define MAX_DUTY	100
#define MIN_DUTY	0

struct {
	char *hostname;
	int port;
	int duty;
	struct sockaddr_in serveraddr;
	struct hostent *host;
} server[MAX_SERVERS];

int servers;


void error(char *msg)
{
	perror(msg);
	exit(0);
}

static inline void clear_screen(void)
{
	const char* cmd = "\e[1;1H\e[2J";
	int ret;

	write(2, cmd, strlen(cmd));
}

static inline void print_config(void)
{
	int i;

	printf("Use the UP/DOWN cursor keys to move servo \n");
	for (i = 0; i < servers ; i++) {
		printf("Server %d:\n", i);
		printf(" ip	    : %s\n", server[i].hostname);
		printf(" port       : %d\n", server[i].port);
		printf(" duty cycle : %d\n", server[i].duty);
	}
}

int kbhit(void)
{
	struct termios oldt, newt;
	int ch;
	int oldf;

	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

	ch = getchar();

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	fcntl(STDIN_FILENO, F_SETFL, oldf);

	if (ch != EOF) {
		/* arrow/cursor keys use three characters - only the last one matters  */
		getchar();
		return 1;
	}

	return 0;
}

static void process(int sockfd)
{
	char buf[BUFSIZE];
	int serverlen;
	int id = 0;
	int duty, n;
	char c;

	bzero(buf, BUFSIZE);

	c = getchar();
	if (c == 'C' || c == 'D')
		id = 1;

	duty = server[id].duty;

	switch(c) {
	case 'A':
	case 'C':
		duty = duty < MAX_DUTY ? duty + 1 : duty;
		break;
	case 'B':
	case 'D':
		duty = duty > MIN_DUTY ? duty - 1 : duty;
		break;
	default:
		return;
	}

	snprintf(buf, sizeof(buf), "%d", duty);
	serverlen = sizeof(server[id].serveraddr);

	n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*) &server[id].serveraddr, serverlen);
	if (n < 0)
		error("ERROR in sendto");

	server[id].duty = duty;

}

int main(int argc, char **argv)
{
	int sockfd;
	int i;

	if (argc != 3 && argc != 5) {
		fprintf(stderr,"usage: %s <hostname> <port> (err: argc=%d)\n", argv[0], argc);
		return -1;
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	servers = argc == 5 ? 2 : 1;

	for (i = 0; i < servers; i++ ) {
		server[i].hostname = argv[1 + i * MAX_SERVERS];
		server[i].port = atoi(argv[2 + i * MAX_SERVERS]);
		server[i].host = gethostbyname(server[i].hostname);
		if (server[i].host == NULL) {
			fprintf(stderr, "ERROR, no such host as %s\n", server[i].hostname);
			exit(0);
		}
		bzero((char *) &server[i].serveraddr, sizeof(server[i].serveraddr));
		bcopy((char *) server[i].host->h_addr, (char *) &server[i].serveraddr.sin_addr.s_addr, server[i].host->h_length);
		server[i].serveraddr.sin_port = htons(server[i].port);
		server[i].serveraddr.sin_family = AF_INET;
		server[i].duty = MIN_DUTY;
	}

	for (;;) {
		clear_screen();
		print_config();
		while (!kbhit())
			usleep(10);
		process(sockfd);
	}

	return 0;
}

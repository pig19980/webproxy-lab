#include "csapp.h"

int main(int argc, char **argv) {
	int clientfd;
	char buf[MAXLINE];
	rio_t rio;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
		exit(0);
	}

	clientfd = Open_clientfd(argv[1], argv[2]);
	printf("Connected to server (%s, %s)\n", argv[1], argv[2]);
	Rio_readinitb(&rio, clientfd);

	while (Fgets(buf, MAXLINE, stdin) != NULL) {
		Rio_writen(clientfd, buf, strlen(buf));
		Rio_readlineb(&rio, buf, MAXLINE);
		Fputs(buf, stdout);
	}
	// while (Fgets(buf, MAXLINE, stdin) != NULL) {
	// 	send(clientfd, buf, strlen(buf), 0);
	// 	recv(clientfd, buf, MAXLINE, 0);
	// 	Fputs(buf, stdout);
	// }
	Close(clientfd);
	exit(0);
}
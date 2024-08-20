#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define CACHE_NUM 10

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
	"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
	"Firefox/10.0.3\r\n";

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *hostname, char *port, char *newuri);
void *task_thread(void *vargp);

typedef struct cache_info {
	char request[MAXLINE];
	int length;
	void *response_value;
} cache_info;

static cache_info cache_infos[CACHE_NUM];
static sem_t mutexs[CACHE_NUM];
int cidx;

void skiphandler(int sig) {
	return;
}

int main(int argc, char **argv) {
	int listenfd, *connfdp;
	char hostname[MAXLINE], port[MAXLINE];
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	pthread_t tid;

	/* Check command line args */
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}

	listenfd = Open_listenfd(argv[1]);
	Signal(SIGPIPE, skiphandler);
	for (int i = 0; i < CACHE_NUM; ++i) {
		Sem_init(&mutexs[i], 0, 1);
	}

	while (1) {
		clientlen = sizeof(clientaddr);
		connfdp = Malloc(sizeof(int));
		*connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
		printf("Accepted connection from (%s, %s)\n", hostname, port);
		Pthread_create(&tid, NULL, task_thread, connfdp);
	}

	return 0;
}

/*
	Get request from client and send to server.
	Send response from server to client.
*/
void doit(int connfd) {
	int parse_ret, clientfd, sendN, gotN;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char hostname[MAXLINE], port[MAXLINE], newuri[MAXLINE];
	char sendbuf[3 * MAXLINE], gotbuf[MAXLINE];
	rio_t connrio, clientrio;

	Rio_readinitb(&connrio, connfd);
	Rio_readlineb(&connrio, buf, MAXLINE);
	printf("Request headers:\n");
	printf("%s", buf);

	int cur_cidx = cidx % 10;
	++cidx;
	P(&mutexs[cur_cidx]);
	strcpy(cache_infos[cur_cidx].request, buf);
	if (cache_infos[cur_cidx].response_value) {
		free(cache_infos[cur_cidx].response_value);
	}
	cache_infos[cur_cidx].response_value = malloc(MAX_OBJECT_SIZE);
	cache_infos[cur_cidx].length = 0;
	sscanf(buf, "%s %s %s", method, uri, version);

	// just read and not use got header part
	read_requesthdrs(&connrio);

	parse_ret = parse_uri(uri, hostname, port, newuri);
	if (!parse_ret) {
		printf("Request is not formal\n");
		return;
	}
	clientfd = open_clientfd(hostname, port);
	if (clientfd < 0) {
		printf("(%s: %s) not available\n", hostname, port);
		for (int i = 0; i < CACHE_NUM; ++i) {
			if (!strcasecmp(buf, cache_infos[i].request)) {
				P(&mutexs[i]);
				Rio_writen(connfd, cache_infos[i].response_value, cache_infos[i].length);
				V(&mutexs[i]);
				return;
			}
		}
		return;
	}
	printf("Connected to server (%s, %s)\n", hostname, port);

	// send request to server
	sendN = 0;
	sendN += sprintf(sendbuf + sendN, "GET %s HTTP/1.0\r\n", newuri);
	sendN += sprintf(sendbuf + sendN, "Host: %s\r\n", hostname);
	sendN += sprintf(sendbuf + sendN, "Connection: close\r\n");
	sendN += sprintf(sendbuf + sendN, "Proxy-Connection: close\r\n");
	sendN += sprintf(sendbuf + sendN, "%s\r\n", user_agent_hdr);
	Rio_writen(clientfd, sendbuf, sendN);

	// got response from server and send to client
	Rio_readinitb(&clientrio, clientfd);
	while ((gotN = rio_readnb(&clientrio, gotbuf, MAXLINE)) > 0) {
		Rio_writen(connfd, gotbuf, gotN);
		memcpy(cache_infos[cur_cidx].response_value + cache_infos[cur_cidx].length, gotbuf, gotN);
		cache_infos[cur_cidx].length += gotN;
	}
	V(&mutexs[cur_cidx]);
	fputs(clientrio.rio_buf, stdout);
	Close(clientfd);
}

/*
	Read request until read "\r\n\r\n".
	Just for use ignore rest header part.
*/
void read_requesthdrs(rio_t *rp) {
	char buf[MAXLINE];

	Rio_readlineb(rp, buf, MAXLINE);
	while (strcmp(buf, "\r\n")) {
		Rio_readlineb(rp, buf, MAXLINE);
		printf("%s", buf);
	}
	return;
}

/*
	Parse uri http://{hostname}:{port}/{newuri}
	into hostname, port, newuri.
	Default port is 80 and default newuri is /
*/
int parse_uri(char *uri, char *hostname, char *port, char *newuri) {
	char *host_ptr, *uri_ptr, *port_ptr;
	host_ptr = strstr(uri, "http:");
	if (!host_ptr) {
		host_ptr = uri;
	} else {
		host_ptr += 5;
		while (*host_ptr != '\0' && *host_ptr == '/') {
			host_ptr++;
		}
		if (*host_ptr == '\0') {
			return 0;
		}
	}

	uri_ptr = index(host_ptr, '/');
	port_ptr = index(host_ptr, ':');
	if (!uri_ptr && !port_ptr && port_ptr > uri_ptr + 1) {
		return -1;
	}

	if (uri_ptr) {
		strcpy(newuri, uri_ptr);
		*uri_ptr = '\0';
	} else {
		strcpy(newuri, "/");
	}
	if (port_ptr) {
		strcpy(port, port_ptr + 1);
		*port_ptr = '\0';
	} else {
		strcpy(port, "80");
	}
	strcpy(hostname, host_ptr);
	return 1;
}

void *task_thread(void *vargp) {
	int connfd = *((int *)vargp);
	Pthread_detach(pthread_self());
	Free(vargp);
	doit(connfd);
	Close(connfd);
	return NULL;
}
/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(char *method, int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(char *method, int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *method, char *errnum, char *shortmsg, char *longmsg);

void skiphandler(int sig) {
	return;
}
void sigchild_handler(int sig) {
	while (waitpid(-1, 0, WNOHANG) > 0)
		;
	return;
}

int main(int argc, char **argv) {
	int listenfd, connfd;
	char hostname[MAXLINE], port[MAXLINE];
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;

	/* Check command line args */
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}

	listenfd = Open_listenfd(argv[1]);
	Signal(SIGPIPE, skiphandler);
	Signal(SIGCLD, sigchild_handler);
	while (1) {
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr,
						&clientlen); // line:netp:tiny:accept
		Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
		printf("Accepted connection from (%s, %s)\n", hostname, port);
		if (Fork() == 0) {
			Close(listenfd);
			doit(connfd);  // line:netp:tiny:doit
			Close(connfd); // line:netp:tiny:close
			exit(0);
		}
		Close(connfd);
	}
}

void doit(int fd) {
	int is_static;
	struct stat sbuf;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];
	rio_t rio;

	Rio_readinitb(&rio, fd);
	Rio_readlineb(&rio, buf, MAXLINE);
	printf("Request headers:\n");
	printf("%s", buf);
	sscanf(buf, "%s %s %s", method, uri, version);
	if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
		clienterror(fd, method, method, "501", "Not implemented", "Tiny does not implement this method");
		return;
	}
	read_requesthdrs(&rio);

	is_static = parse_uri(uri, filename, cgiargs);
	if (stat(filename, &sbuf) < 0) {
		clienterror(fd, filename, method, "404", "Not found", "Tiny couldn't find this file");
		return;
	}

	if (is_static) {
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
			clienterror(fd, filename, method, "403", "Forbidden", "Tiny couldn't read this file");
			return;
		}
		serve_static(method, fd, filename, sbuf.st_size);
	} else {
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
			clienterror(fd, filename, method, "403", "Forbidden", "Tiny couldn't run the CGI program");
			return;
		}
		serve_dynamic(method, fd, filename, cgiargs);
	}
}

void read_requesthdrs(rio_t *rp) {
	char buf[MAXLINE];

	Rio_readlineb(rp, buf, MAXLINE);
	while (strcmp(buf, "\r\n")) {
		Rio_readlineb(rp, buf, MAXLINE);
		printf("%s", buf);
	}
	return;
}

int parse_uri(char *uri, char *filename, char *cgiargs) {
	char *ptr;

	if (!strstr(uri, "cgi-bin")) {
		strcpy(cgiargs, "");
		strcpy(filename, ".");
		strcat(filename, uri);
		if (uri[strlen(uri) - 1] == '/') {
			strcat(filename, "home.html");
		}
		return 1;
	} else {
		ptr = index(uri, '?');
		if (ptr) {
			strcpy(cgiargs, ptr + 1);
			*ptr = '\0';
		} else {
			strcpy(cgiargs, "");
		}
		strcpy(filename, ".");
		strcat(filename, uri);
		return 0;
	}
}

void serve_static(char *method, int fd, char *filename, int filesize) {
	int srcfd, buf_len = 0;
	char *srcp, filetype[MAXLINE], buf[2 * MAXBUF];

	get_filetype(filename, filetype);
	buf_len += sprintf(buf + buf_len, "HTTP/1.0 200 OK\r\n");
	buf_len += sprintf(buf + buf_len, "Server: Tiny Web Server\r\n");
	buf_len += sprintf(buf + buf_len, "Connection: close\r\n");
	buf_len += sprintf(buf + buf_len, "Content-length: %d\r\n", filesize);
	buf_len += sprintf(buf + buf_len, "Content-type: %s\r\n\r\n", filetype);
	Rio_writen(fd, buf, strlen(buf));
	printf("Response headers:\n");
	printf("%s", buf);

	if (!strcasecmp(method, "HEAD")) {
		return;
	}

	srcfd = Open(filename, O_RDONLY, 0);
	// srcp = Mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
	// Close(srcfd);
	// Rio_writen(fd, srcp, filesize);
	// Munmap(srcp, filesize);
	rio_t rio;
	Rio_readinitb(&rio, srcfd);
	srcp = Malloc(filesize);
	Rio_readnb(&rio, srcp, filesize);
	Close(srcfd);
	Rio_writen(fd, srcp, filesize);
	free(srcp);
}

void get_filetype(char *filename, char *filetype) {
	if (strstr(filename, ".html")) {
		strcpy(filetype, "text/html");
	} else if (strstr(filename, ".gif")) {
		strcpy(filetype, "image/gif");
	} else if (strstr(filename, ".png")) {
		strcpy(filetype, "image/png");
	} else if (strstr(filename, ".jpg")) {
		strcpy(filetype, "image/gpeg");
	} else if (strstr(filename, ".mp4")) {
		strcpy(filetype, "video/mp4");
	} else if (strstr(filename, ".ico")) {
		strcpy(filetype, "image/vnd.microsoft.icon");
	} else {
		strcpy(filetype, "text/plain");
	}
}

void serve_dynamic(char *method, int fd, char *filename, char *cgiargs) {
	char buf[MAXLINE], *emptylist[] = {NULL};

	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Server: Tiny Web Server\r\n");
	Rio_writen(fd, buf, strlen(buf));

	if (Fork() == 0) {
		setenv("QUERY_STRING", cgiargs, 1);
		setenv("METHOD", method, 1);
		Dup2(fd, STDOUT_FILENO);
		Execve(filename, emptylist, environ);
	}
}

void clienterror(int fd, char *cause, char *method, char *errnum, char *shortmsg, char *longmsg) {
	char buf[MAXLINE], body[MAXBUF];
	int body_len = 0, buf_len;
	body_len += sprintf(body + body_len, "<html><title>Tiny Error</title>");
	body_len += sprintf(body + body_len, "<body bgcolor=\"ffff\">\r\n");
	body_len += sprintf(body + body_len, "%s: %s\r\n", errnum, shortmsg);
	body_len += sprintf(body + body_len, "<p>%s: %s\r\n", longmsg, cause);
	body_len += sprintf(body + body_len, "<hr><em>The Tiny Web server</em>\r\n");

	buf_len = sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen(fd, buf, buf_len);
	buf_len = sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen(fd, buf, buf_len);
	buf_len = sprintf(buf, "Content-length: %d\r\n\r\n", body_len);
	Rio_writen(fd, buf, buf_len);
	if (strcasecmp(method, "HEAD")) {
		Rio_writen(fd, body, body_len);
	}
}
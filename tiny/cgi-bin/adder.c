/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
	char *buf, *p;
	char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
	int n1 = 0, n2 = 0, cont_len = 0;

	if ((buf = getenv("QUERY_STRING")) != NULL) {
		p = strchr(buf, '&');
		*p = '\0';
		strcpy(arg1, buf);
		strcpy(arg2, p + 1);
		n1 = atoi(arg1);
		n2 = atoi(arg2);
		// sscanf(buf, "%d&%d", &n1, &n2);
	}

	cont_len += sprintf(content + cont_len, "Welcome to add.com: ");
	cont_len += sprintf(content + cont_len, "THE Internet addition portal.\r\n<p>");
	cont_len += sprintf(content + cont_len, "The answer is: %d + %d = %d\r\n<p>", n1, n2, n1 + n2);
	cont_len += sprintf(content + cont_len, "Thanks for visiting!\r\n");

	printf("Connection: close\r\n");
	printf("Content-length: %d\r\n", cont_len);
	printf("Content-type: text/html\r\n\r\n");
	printf("%s", content);
	fflush(stdout);

	exit(0);
}
/* $end adder */

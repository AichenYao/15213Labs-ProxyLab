/*
 * Starter code for proxy lab.
 * Feel free to modify this code in whatever way you wish.
 */

/* Some useful includes to help you get started */

#include "csapp.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>

/*
 * Debug macros, which can be enabled by adding -DDEBUG in the Makefile
 * Use these if you find them useful, or delete them if not
 */
#ifdef DEBUG
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_assert(...)
#define dbg_printf(...)
#endif

/*
 * Max cache and object sizes
 * You might want to move these to the file containing your cache implementation
 */
#define MAX_CACHE_SIZE (1024 * 1024)
#define MAX_OBJECT_SIZE (100 * 1024)

/*
 * String to use for the User-Agent header.
 * Don't forget to terminate with \r\n
 */
// static const char *header_user_agent = "Mozilla/5.0"
//                                        " (X11; Linux x86_64; rv:3.10.0)"
//                                        " Gecko/20201123 Firefox/63.0.1";

/*  Cite Cs:APP textbook 11.6  */
void clientrror(int client_fd, char *cause, char*errnum, char *shortmsg, 
char *longmsg);

void parse_uri(char *uri, char *hostname, char *path, int port);

void build_header(rio_t *client_rio, char *server_header, char *hostname);

void doit(int client_fd) {
    int server_fd;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char server_header[MAXLINE]; //deal with 4.2 in writeup about headers
    char hostname[MAXLINE], path[MAXLINE];
    rio_t client_rio, server_rio;
    int port;
    //associate client's fd with its rio structure
    rio_readinitb(&client_rio, client_fd);
    rio_readlineb(&client_rio, buf, MAXLINE); //read the command line into buf
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {
        //cannot implement other types of requests
        clienterror(client_fd, method, "501", "Not implemented", 
        "Tiny does not implement this method");
        return;
    }
    parse_uri(uri, hostname, path, port);
    //the header that will be sent to the server
    char port_str[8];
    //establish a connection with a server running on host hostname and 
    //listening fo connection requests on port number port
    server_fd = open_clientfd(hostname, port_str);
    if (server_fd < 0) {
        sio_printf("connection failed\n");
        return;
    }
    rio_readinitb(&server_rio, server_fd);
    sprintf(server_header, "GET %s HTTp/1.0\r\n", path);
    build_header(&client_rio, server_header, hostname);
    rio_writen(server_fd, server_header, strlen(server_header));
    //receive content from the server and deliver to the client
    size_t size = 0;
    while((size = rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
        rio_writen(client_fd, buf, size);
        //copy to the client's fd
    }
    close(server_fd);
    return;
}
/*  Cite Cs:APP textbook 11.6  */
//Q: clientaddr
//Q: Shall I check for errors for each "non-wrapped" functions and exit if -1?
int main(int argc, char **argv) {
    int listenfd, client_fd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr clientaddr;
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    signal(SIGPIPE, SIG_IGN); 
    //simply ignore SIGPIPE because we don'w want to terminate the whole program
    listenfd = open_listenfd(argv[1]);
    while (true) {
        clientlen = sizeof(clientaddr);
        client_fd = accept(listenfd, &clientaddr, &clientlen);
        getnameinfo(&clientaddr, clientlen, hostname, MAXLINE, port, 
        MAXLINE, 0);
        sio_printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(client_fd);
        close(client_fd);
    }
    return 0;
}

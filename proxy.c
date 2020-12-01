/*
 * Starter code for proxy lab.
 * Feel free to modify this code in whatever way you wish.
 */

/* Some useful includes to help you get started */

#include "csapp.h"
#include "http_parser.h"
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
char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=" "ffffff" ">\r\n", body);
    sprintf(body, "%s%s: $s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(client_fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(client_fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(client_fd, buf, strlen(buf));
    rio_writen(client_fd, buf, strlen(body));
}


void doit(int client_fd) {
    int server_fd;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t client_rio, server_rio;
    int port;
    //associate client's fd with its rio structure
    rio_readinitb(&client_rio, client_fd);
    int n = rio_readlineb(&client_rio, buf, MAXLINE);
    if (n == -1) {
        return;
    }
    sscanf(buf, "%s %s %s", method, uri, version);
    parser_t *new_parser = parser_new();
    parser_state new_parser_state = parser_parse_line(new_parser, buf);
    if (new_parser_state == ERROR) {
        return;
    }
    if (new_parser_state == REQUEST) {
        if (strcasecmp(method, "GET")) {
            //cannot implement other types of requests
            clienterror(client_fd, method, "501", "Not implemented", 
            "Tiny does not implement this method");
        }
        const char hostname[MAXLINE];
        const char path[MAXLINE];
        char port[MAXLINE];
        int ret_num1 = parser_retrieve(new_parser, HOST, &hostname);
        if (ret_num1 == -1) {
            printf("cannot parse HOST\n");
            exit(-1);
        }
        int ret_num2 = parser_retrieve(new_parser, PATH, &path);
        if (ret_num2 == -1) {
            printf("cannot parse PATH\n");
            exit(-1);
        }
        int ret_num3 = parser_retrieve(new_parser, PORT, &port);
        if (ret_num3 == -1) {
            printf("cannot parse PORT\n");
            exit(-1);
        }
        if (ret_num3 == -2) {
            port[0] = '8';
            port[1] = '0';
        }
        server_fd = open_clientfd(hostname, port);
        if (server_fd < 0) {
            printf("connection failed\n");
            return;
        }
        rio_readinitb(&server_rio, server_fd);
        char *server_header[MAXLINE];
        sprintf(server_header, "GET %s HTTp/1.0\r\n", path);
        rio_writen(server_fd, server_header, strlen(server_header));
    }
    if (new_parser_state == HEADER) {
        header_t *host_header = parser_lookup_header(new_parser, "Host");
        header_t *user_agent_header = parser_lookup_header(new_parser, 
        "User-Agent");
        header_t *conn_header = parser_lookup_header(new_parser, "Connection");
        header_t *pro_header = parser_lookup_header(new_parser, 
        "Proxy-Connection");
        header_t *new_header;
        while ((new_header = parser_retrieve_next_header(new_parser)) 
        != NULL) {
            //do something
        }
    }
    parser_free(new_parser);
    //the header that will be sent to the server
    // char port_str[8];
    // //establish a connection with a server running on host hostname and 
    // //listening fo connection requests on port number port
    // server_fd = open_clientfd(hostname, port_str);
    // if (server_fd < 0) {
    //     sio_printf("connection failed\n");
    //     return;
    // }
    // rio_readinitb(&server_rio, server_fd);
    // sprintf(server_header, "GET %s HTTp/1.0\r\n", path);
    // build_header(&client_rio, server_header, hostname);
    // rio_writen(server_fd, server_header, strlen(server_header));
    // //receive content from the server and deliver to the client
    // size_t size = 0;
    // while((size = rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
    //     rio_writen(client_fd, buf, size);
    //     //copy to the client's fd
    // }
    // close(server_fd);
    return;
}
/*  Cite Cs:APP textbook 11.6  */
//Q: clientaddr
//Q: Shall I check for errors for each "non-wrapped" functions and exit if -1?
int main(int argc, char **argv) {
    int listen_fd, client_fd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr clientaddr;
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    signal(SIGPIPE, SIG_IGN); 
    //simply ignore SIGPIPE because we don'w want to terminate the whole program
    listen_fd = open_listenfd(argv[1]);
    if(listen_fd == -1) {
        printf("cannot establish a listening descriptor\n");
        exit(1);
    }
    while (true) {
        clientlen = sizeof(clientaddr);
        client_fd = accept(listen_fd, &clientaddr, &clientlen);
        if (client_fd == -1) {
            printf("cannot establish a connected descriptor\n");
            continue;
        }
        int n = getnameinfo(&clientaddr, clientlen, hostname, MAXLINE, port, 
        MAXLINE, 0);
        if (n != 0) {
            printf("cannot extract info from the socket address structure\n");
            continue;
        }
        sio_printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(client_fd);
        close(client_fd);
    }
    return 0;
}

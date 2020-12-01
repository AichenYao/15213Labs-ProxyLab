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
static const char *header_user_agent = "Mozilla/5.0"
                                       " (X11; Linux x86_64; rv:3.10.0)"
                                       " Gecko/20201123 Firefox/63.0.1";

/* Display an error message to the users */
void clienterror(int fd, const char *errnum, const char *shortmsg,
                 const char *longmsg) {
    char buf[MAXLINE];
    char body[MAXBUF];
    size_t buflen;
    size_t bodylen;

    /* Build the HTTP response body */
    bodylen = snprintf(body, MAXBUF,
            "<!DOCTYPE html>\r\n" \
            "<html>\r\n" \
            "<head><title>Tiny Error</title></head>\r\n" \
            "<body bgcolor=\"ffffff\">\r\n" \
            "<h1>%s: %s</h1>\r\n" \
            "<p>%s</p>\r\n" \
            "<hr /><em>The Tiny Web server</em>\r\n" \
            "</body></html>\r\n", \
            errnum, shortmsg, longmsg);
    if (bodylen >= MAXBUF) {
        return; // Overflow!
    }

    /* Build the HTTP response headers */
    buflen = snprintf(buf, MAXLINE,
            "HTTP/1.0 %s %s\r\n" \
            "Content-Type: text/html\r\n" \
            "Content-Length: %zu\r\n\r\n", \
            errnum, shortmsg, bodylen);
    if (buflen >= MAXLINE) {
        return; // Overflow!
    }

    /* Write the headers */
    if (rio_writen(fd, buf, buflen) < 0) {
        fprintf(stderr, "Error writing error response headers to client\n");
        return;
    }

    /* Write the body */
    if (rio_writen(fd, body, bodylen) < 0) {
        fprintf(stderr, "Error writing error response body to client\n");
        return;
    }
}



void doit(int client_fd) {
    int server_fd = 0;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t client_rio, server_rio;
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
        parser_free(new_parser);
        return;
    }
    if (new_parser_state == REQUEST) {
        if (strcasecmp(method, "GET")) {
            //cannot implement other types of requests
            clienterror(client_fd, method, "501 Not implemented", 
            "Tiny does not implement this method");
        }
        const char *hostname;
        const char *path;
        const char *port;
        int ret_num1 = parser_retrieve(new_parser, HOST, &hostname);
        if (ret_num1 == -1 || ret_num1 == -2) {
            printf("cannot parse HOST\n");
            exit(-1);
        }
        int ret_num2 = parser_retrieve(new_parser, PATH, &path);
        if (ret_num2 == -1 || ret_num2 == -2) {
            printf("cannot parse PATH\n");
            exit(-1);
        }
        int ret_num3 = parser_retrieve(new_parser, PORT, &port);
        if (ret_num3 == -1 || ret_num3 == -2) {
            printf("cannot parse PORT\n");
            exit(-1);
        }
        server_fd = open_clientfd(hostname, port);
        if (server_fd < 0) {
            printf("connection failed\n");
            return;
        }
        rio_readinitb(&server_rio, server_fd);
        char server_header[MAXLINE];
        sprintf(server_header, "GET %s HTTp/1.0\r\n", path);
        rio_writen(server_fd, server_header, strlen(server_header));
    }
    if (new_parser_state == HEADER) {
        const char *hostname;
        const char *port;
        int ret_num1 = parser_retrieve(new_parser, HOST, &hostname);
        if (ret_num1 == -1) {
            printf("cannot parse HOST\n");
            exit(-1);
        }
        int ret_num3 = parser_retrieve(new_parser, PORT, &port);
        if (ret_num3 == -1) {
            printf("cannot parse PORT\n");
            exit(-1);
        }
        server_fd = open_clientfd(hostname, port);
        if (server_fd < 0) {
            printf("connection failed\n");
            return;
        }
        rio_readinitb(&server_rio, server_fd);

        //Host Header
        header_t *host_header_struct = parser_lookup_header(new_parser, "Host");
        char host_header[MAXLINE];
        sprintf(host_header, "%s: %s", "Host", (host_header_struct)->value);
        rio_writen(server_fd, host_header, strlen(host_header));

        //User-Agent Header
        char ua_header[MAXLINE];
        sprintf(ua_header, "%s: %s", "User-Agent", header_user_agent);
        rio_writen(server_fd, ua_header, strlen(ua_header));

        //Connection Header
        char conn_header[MAXLINE];
        sprintf(conn_header, "%s: %s", "Connection", 
        "close\r\n");
        rio_writen(server_fd, conn_header, strlen(conn_header));

        //Proxy-Connection Header
        char pro_header[MAXLINE];
        sprintf(pro_header, "%s: %s", "Proxy-Connection",  "close\r\n");
        rio_writen(server_fd, pro_header, strlen(pro_header));

        //forward any other header that is not one of the four above
        header_t *new_header_struct;
        while ((new_header_struct = parser_retrieve_next_header(new_parser)) 
        != NULL) {
            char new_header[MAXLINE];
            sprintf(new_header, "%s: %s", (new_header_struct)->name, 
            (new_header_struct)->value);
            rio_writen(server_fd, new_header, strlen(new_header));
        }
    }
    close(server_fd);
    parser_free(new_parser);
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

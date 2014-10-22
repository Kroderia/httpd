//
//  main.c
//  httpd
//
//  Created by Kroderia on 10/22/14.
//  Copyright (c) 2014 Kroderia. All rights reserved.
//

#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

enum err_type {
    SOCKET_ERROR = 1,
    BIND_ERROR,
    GETSOCKS_ERROR,
    LISTEN_ERROR,
};

#define SOCK_BACKLOG 5
#define URL_MAX_LENGTH 1024
#define REQUEST_MAX_LENGTH 4096
#define METHOD_MAX_LENGTH 16
#define PATH_MAX_LENGTH URL_MAX_LENGTH * 2

#define DOCS_PATH "htdocs"
#define DEFAULT_PAGE "index.html"

int init_sock(u_short *port);
void exit_sock(int sockfd);

void accept_request(int sockfd);
int is_excutable(const char* path, struct stat st);

void return_404(int sockfd);
void return_501(int sockfd);

void hdlr_signal(int signo);

void log_msg(const char* msg);

#define skip_space(ptr, ori, len) while ((isspace(*(ptr++))) && ((ptr - ori) < len)); ptr--
#define read_string(to, ori, from, len) while ((! isspace(*from)) && ((to - ori) < len)) \
    *(to++) = *(from++)

/*
 * References:
 *      http://beej.us/net2/html/syscalls.html
 *      http://man7.org/linux/man-pages/man2/getsockname.2.html
 *
 * Others:
 *      PF stands for Protocol Family
 *      AF stands for Address Family
 *      In source, there is #define	PF_INET AF_INET
 *      Thus they in fact are the same.
 *      But it is better to use PF in protocol and AF in address.
 *      Like PF in socket() and Af in sockaddr_in.
 *
 *      SOCK_STREAM bases on TCP, and HTTP bases on TCP
 *      SOCK_DGRAM bases on UDP
 *
 */
int init_sock(u_short *port) {
    int sockfd = 0;
    
    if (-1 == (sockfd = socket(PF_INET, SOCK_STREAM, 0)))
        exit(-SOCKET_ERROR);
    
    struct sockaddr_in addr;
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(*port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (-1 == bind(sockfd, (struct sockaddr*) &addr, sizeof(addr)))
        exit(-BIND_ERROR);
    
    if (0 == *port) {
        socklen_t addrlen = sizeof(addr);
        if (-1 == getsockname(sockfd, (struct sockaddr*) &addr, &addrlen))
            exit(-GETSOCKS_ERROR);
        *port = ntohs(addr.sin_port);
    }
    
    if (-1 == (listen(sockfd, SOCK_BACKLOG)))
        exit(-LISTEN_ERROR);
    
    return sockfd;
}

void exit_sock(int sockfd) {
    close(sockfd);
}

void return_cgi(int sockfd, const char* path, const char* method, const char* query) {
}

void return_header(int sockfd, const char* path) {
    char buf[1024];
    
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");    // Determind the mime by filename
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(sockfd, buf, strlen(buf), 0);
}

void return_content(int sockfd, FILE* file) {
    char buf[1024];
    
    fgets(buf, sizeof(buf), file);
    do {
        send(sockfd, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), file);
    } while (! feof(file));
}

void return_file(int sockfd, const char* path) {
    FILE *file =  NULL;
    
    if ((file = fopen(path, "r")) == NULL)
        return return_404(sockfd);
    
    return_header(sockfd, path);
    return_content(sockfd, file);
    
    fclose(file);
    close(sockfd);
}

/*
 * References:
 *      http://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html
 */

void accept_request(int sockfd) {
    char buf[REQUEST_MAX_LENGTH] = { 0 };
    char *b_ptr = buf;

    char method[METHOD_MAX_LENGTH] = { 0 };
    char *t_ptr = NULL;
    
    char url[URL_MAX_LENGTH] = { 0 };
    char *query = url;

    if (0 >= read(sockfd, buf, sizeof(buf))) {
        close(sockfd);
        return log_msg("Empty request");
    }
    
    t_ptr = method;
    read_string(t_ptr, method, b_ptr, sizeof(method));

    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
        return_501(sockfd);

    skip_space(b_ptr, buf, sizeof(buf));

    t_ptr = url;
    read_string(t_ptr, url, b_ptr, sizeof(method));
    
    while ((*query != '?') && (*query != '\0'))
        query++;
    if (*query == '?')
        *(query++) = '\0';
    
    char path[PATH_MAX_LENGTH] = { 0 };

    sprintf(path, "%s%s", DOCS_PATH, url);
    if (path[strlen(path) - 1] == '/')
        strcat(path, DEFAULT_PAGE);
    
    struct stat st;
    if (stat(path, &st) == -1)
        return return_404(sockfd);
    
    if ((st.st_mode & S_IFMT) == S_IFDIR) {
        strcat(path, "/");
        strcat(path, DEFAULT_PAGE);
    }
    
    if (is_excutable(path, st))
        return_cgi(sockfd, path, method, query);
    else
        return_file(sockfd, path);
}

int is_excutable(const char* path, struct stat st) {
    // Not accurate
    return (st.st_mode & S_IXUSR) ||
           (st.st_mode & S_IXGRP) ||
           (st.st_mode & S_IXOTH);
}

void return_404(int sockfd) {
    char buf[1024];
    
    sprintf(buf, "HTTP/1.0 404 Page not found\r\n");
    send(sockfd, buf, strlen(buf), 0);
    
    close(sockfd);
}

void return_501(int sockfd)
{
    char buf[1024];
    
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(sockfd, buf, strlen(buf), 0);
    
    close(sockfd);
}

void hdlr_signal(int signo) {
}

void log_msg(const char* msg) {
    printf("%s\n", msg);
}

int main(int argc, const char * argv[]) {
    int s_sock = 0;
    u_short s_port = 12345;
    int c_sock = 0;
    struct sockaddr_in c_addr;
    int c_addrlen = sizeof(c_addr);
    
    s_sock = init_sock(&s_port);
    signal(SIGPIPE, hdlr_signal);
    
    while (1) {
        c_sock = accept(s_sock, (struct sockaddr*) &c_addr, &c_addrlen);
        if (-1 == c_sock)
            log_msg("Client socket error");
        else
            accept_request(c_sock);
    }

    exit_sock(s_sock);
    
    return 0;
}

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
#include <stdlib.h>
#include <unistd.h>

enum err_type {
    SOCKET_ERROR = 1,
    BIND_ERROR,
    GETSOCKS_ERROR,
    LISTEN_ERROR,
};

#define SOCK_BACKLOG 5

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

int init_sock(u_short *port);
void exit_sock(int sockfd);
void log_msg(const char* msg);
void accept_request(int sockfd);
void method_unimplement(int sockfd);
void method_implement(int sockfd);

void hdlr_signal(int signo);

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

void log_msg(const char* msg) {
    printf("%s\n", msg);
}

/*
 * References:
 *      http://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html
 *
 *
 */

void accept_request(int sockfd) {
    char buf[1024];
    char* b_ptr = buf;
    char method[16];
    char* m_ptr = method;
    
    int len = read(sockfd, buf, sizeof(buf));

    while ((! isspace(*b_ptr)) && ((m_ptr - method) < sizeof(method) - 1))
        *(m_ptr++) = *(b_ptr++);
    
    *m_ptr = '\0';
    log_msg(method);
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
        return method_unimplement(sockfd);
    else
        return method_implement(sockfd);
}

void method_unimplement(int sockfd)
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

void method_implement(int sockfd)
{
    char buf[1024];
    
    sprintf(buf, "HTTP/1.0 200\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Implemented\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method supported.\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(sockfd, buf, strlen(buf), 0);
    
    close(sockfd);
}

void hdlr_signal(int signo) {
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

#include "subflow.h"
#include "output_log.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BACKLOG 128

static inline void generate_addr(struct sockaddr_in* addr, const char* ip, int port)
{
    bzero(addr, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = inet_addr(ip);
}

static inline void parse_addr(struct sockaddr_in addr, char (*ip)[], int* port)
{
    inet_ntop(AF_INET, &(addr.sin_addr), *ip, INET_ADDRSTRLEN);
    *port = (int) ntohs(addr.sin_port);
}

static int socket_init()
{
    return socket(AF_INET, SOCK_STREAM, 0);
}

int client_init(subflow* sf, const char* remote_ip, int remote_port)
{
    if ((sf->fd = socket_init()) == -1)
    {
        log_error("[SUBFLOW] client_init error: %s:%d\n", sf->remote_ip, sf->remote_port);
        perror("socket_init");
        return -1;
    }
    generate_addr(&(sf->remote_addr), remote_ip, remote_port);
    parse_addr(sf->remote_addr, &(sf->remote_ip), &(sf->remote_port));
    return 0;
}

int server_init(subflow_server* server, const char* listen_ip, int listen_port)
{
    if ((server->fd = socket_init()) == -1)
    {
        log_error("[SUBFLOW] server_init error: %s:%d\n", server->listen_ip, server->listen_port);
        perror("socket_init");
        return -1;
    }
    generate_addr(&(server->listen_addr), listen_ip, listen_port);
    parse_addr(server->listen_addr, &(server->listen_ip), &(server->listen_port));
    return 0;
}

int client_connect(const subflow* sf)
{
    if (connect(sf->fd, (struct sockaddr*) &(sf->remote_addr), sizeof(struct sockaddr)))
    {
        log_error("[SUBFLOW] connect error: %s:%d\n", sf->remote_ip, sf->remote_port);
        perror("connect");
        return -1;
    }

    return 0;
}

int server_bind(const subflow_server* server)
{
    if (bind(server->fd, (struct sockaddr*) &server->listen_addr, sizeof(struct sockaddr)) == -1)
    {
        log_error("[SUBFLOW] bind error: %s:%d\n", server->listen_ip, server->listen_port);
        perror("bind");
        return -1;
    }

    return 0;
}

int server_listen(const subflow_server* server)
{
    if (listen(server->fd, BACKLOG) == -1)
    {
        log_error("[SUBFLOW] listen error: %s:%d\n", server->listen_ip, server->listen_port);
        perror("listen");
        return -1;
    }

    return 0;
}

int server_accept(subflow* sf, const subflow_server* server)
{
    int sin_size = sizeof(struct sockaddr_in);
    if ((sf->fd = accept(server->fd, (struct sockaddr*) &(sf->remote_addr),
                         (socklen_t*) &sin_size)) == -1)
    {
        log_error("[SUBFLOW] accept error: %s:%d\n", server->listen_ip, server->listen_port);
        perror("accept");
        return -1;
    }

    parse_addr(sf->remote_addr, &(sf->remote_ip), &(sf->remote_port));
    return 0;
}

int subflow_read(const subflow* sf, char* buf, int len)
{
    int total_byte = 0;
    int recv_byte = 0;

    while (1)
    {
        recv_byte = recv(sf->fd, buf + total_byte, len - total_byte, 0);

        if (recv_byte < 0)
        {
            log_error("[SUBFLOW] recv error: %s:%d\n", sf->remote_ip, sf->remote_port);
            perror("recv");
            return -1;
        }
        if (recv_byte == 0) break;

        total_byte += recv_byte;
        if (total_byte >= len) break;
    }

    assert(total_byte <= len);

    return total_byte;
}

int subflow_write(const subflow* sf, const char* buf, int len)
{
    int total_byte = 0;
    int send_byte = 0;

    while (1)
    {
        send_byte = send(sf->fd, buf + total_byte, len - total_byte, 0);

        if (send_byte < 0)
        {
            log_error("[SUBFLOW] send error: %s:%d\n", sf->remote_ip, sf->remote_port);
            perror("send");
            return -1;
        }
        if (send_byte == 0) break;

        total_byte += send_byte;
        if (total_byte >= len) break;
    }

    assert(total_byte <= len);

    return total_byte;
}

int subflow_shutdown(const subflow* sf, int how)
{
    return shutdown(sf->fd, how);
}

int subflow_close(const subflow* sf)
{
    return close(sf->fd);
}

int server_close(const subflow_server* server)
{
    return close(server->fd);
}

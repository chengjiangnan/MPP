#ifndef __SUBFLOW_H__
#define __SUBFLOW_H__ 1

#include <sys/socket.h>
#include <arpa/inet.h>

typedef struct subflow
{
    int fd;
    struct sockaddr_in remote_addr;
    char remote_ip[INET_ADDRSTRLEN];
    int remote_port;
} subflow;

typedef struct subflow_server
{
    int fd;
    struct sockaddr_in listen_addr;
    char listen_ip[INET_ADDRSTRLEN];
    int listen_port;
} subflow_server;

/*
 * initialize a subflow with remote ip address and port number
 * return 0 if success; -1 otherwise
 */
int client_init(subflow* sf, const char* remote_ip, int remote_port);

/*
 * initialize a server with listen ip address and port number
 * return 0 if success; -1 otherwises 
 */
int server_init(subflow_server* server, const char* listen_ip, int listen_port);

/*
 * connect subflow to the remote side
 * return 0 if success; -1 otherwise
 */
int client_connect(const subflow* sf);

/*
 * bind server to its listen ip address and port number
 * return 0 if success; -1 otherwise
 */
int server_bind(const subflow_server* server);

/*
 * server listens on the ip address and port number
 * return 0 if success; -1 otherwise
 */
int server_listen(const subflow_server* server);

/*
 * accept subflow from the remote side
 * return 0 if success; -1 otherwise
 */
int server_accept(subflow* sf, const subflow_server* server);

/*
 * read data from subflow, len should > 0 
 * returns the number of bytes that read
 * must succeed, unless the other side closes the connection
 * return the number of bytes read if success; -1 otherwise
 */
int subflow_read(const subflow* sf, char* buf, int len);

/*
 * write data to subflow, len should > 0 
 * returns the number of bytes that write
 * must succeed, unless the other side closes the connection
 */
int subflow_write(const subflow* sf, const char* buf, int len);

/*
 * shutdown subflow
 * return 0 if success; -1 otherwise
 */
int subflow_shutdown(const subflow* sf, int how);

/*
 * close subflow
 * return 0 if success; -1 otherwise
 */
int subflow_close(const subflow* sf);

/*
 * close server
 * return 0 if success; -1 otherwise
 */
int server_close(const subflow_server* server);

#endif /*__SUBFLOW_H__*/

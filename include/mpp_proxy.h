#ifndef __MPP_PROXY_H__
#define __MPP_PROXY_H__ 1

#include "mpp_source.h"
#include "mpp_sink.h"
#include "mpp_proxy_conn.h"

typedef struct mpp_proxy
{
    mpp_sink sink;
} mpp_proxy;

typedef struct proxy_t_args
{
    int sink_sf_num;
    subflow* sink_sf;

    int* listen_write_ratio;
    int* backend_write_ratio;
} proxy_t_args;
/*
 * initialize a mpp proxy with multiple listen (ip address, port number) pairs
 * and multiple backend (ip address, port number) pairs
 * return 0 if success; -1 otherwise
 */
int mpp_proxy_init(mpp_proxy* proxy,
                   const char* listen_ip[], const int listen_port[], int listen_sf_num,
                   const char* backend_ip[], const int backend_port[], int backend_sf_num);

/*
 * for nested mpp_sink, bind to sockets
 * return 0 if success; -1 otherwise
 */
int mpp_proxy_bind(mpp_proxy* proxy);

/*
 * for nested mpp_sink, listen on sockets
 * return 0 if success; -1 otherwise
 */
int mpp_proxy_listen(mpp_proxy* proxy);

/*
 * accept subflows from the listen side
 * return a subflow pointer if success; NULL otherwise
 */
subflow* mpp_proxy_accept(mpp_proxy* proxy);

/*
 * an mpp_proxy_thread corresponds to one mpp_proxy_conn
 */
void* mpp_proxy_thread(void* args);

/*
 * clean mpp_proxy
 */
void mpp_proxy_clean(mpp_proxy* proxy);

#endif /*__MPP_PROXY_H__*/

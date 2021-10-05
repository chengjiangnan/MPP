#ifndef __MPP_GATEWAY_H__
#define __MPP_GATEWAY_H__ 1

#include "mpp_source.h"
#include "mpp_sink.h"

typedef struct ingress_t_args
{
    subflow* sf;

    int mpp_sf_num;
    int* mpp_port;
    char** mpp_ip;
} ingress_t_args;

typedef struct egress_t_args
{
    mpp_conn* conn;

    int tcp_port;
    char* tcp_ip;
} egress_t_args;

void* mpp_ingress_thread(void* _args);

void* mpp_egress_thread(void* _args);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Below are functions used only by egress/ingress that supports topology update
///////////////////////////////////////////////////////////////////////////////////////////////////

int mpp_ingress_start(subflow* sf, char** mpp_ip, int* mpp_port, int* write_ratio, int mpp_sf_num,
                      mpp_source** src_ptr, mpp_conn** conn_ptr, pthread_t** t_ptr);

void mpp_ingress_clean(subflow* sf, mpp_source* src, mpp_conn* conn, pthread_t* t);

int mpp_egress_start(mpp_conn* conn, int* write_ratio, char* tcp_ip, int tcp_port,
                     subflow** sf_ptr, pthread_t** t_ptr);

void mpp_egress_clean(subflow* sf, mpp_conn* conn, pthread_t* t);

#endif /*__MPP_GATEWAY_H__*/

#include "mpp_gateway.h"
#include "mpp_source.h"
#include "mpp_sink.h"
#include "mpp_conn.h"
#include "output_log.h"
#include "subflow.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define BUF_SIZE 50000

typedef struct tcp_mpp_args
{
    subflow* sf;
    mpp_conn* conn;
} tcp_mpp_args;

static void* tcp_to_mpp_t(void* _args)
{
    tcp_mpp_args* args = (tcp_mpp_args*) _args;
    subflow* sf = args->sf;
    mpp_conn* conn = args->conn;

    char buf[BUF_SIZE];
    int curr_size = 0;

    while (1)
    {
        
        if ((curr_size = subflow_read(sf, buf, BUF_SIZE)) == -1)
        {
            fprintf(stderr, "subflow_read error\n");
            goto close;
        }

        log_debug("[tcp_to_mpp_t] read %d bytes from tcp\n", curr_size);

        if (curr_size == 0) goto close;

        if (mpp_conn_write(conn, buf, curr_size) < curr_size)
        {
            fprintf(stderr, "mpp_conn_write error\n");
            goto close;
        }

        if (curr_size < BUF_SIZE) goto close;
    }

close:
    subflow_shutdown(sf, SHUT_RD);
    mpp_conn_shutdown(conn, SHUT_WR);
    return NULL;
}

static void* mpp_to_tcp_t(void* _args)
{
    tcp_mpp_args* args = (tcp_mpp_args*) _args;
    subflow* sf = args->sf;
    mpp_conn* conn = args->conn;

    char buf[BUF_SIZE];
    int curr_size = 0;

    while (1)
    {
        
        if ((curr_size = mpp_conn_read(conn, buf, BUF_SIZE)) == -1)
        {
            fprintf(stderr, "mpp_conn_read error\n");
            goto close;
        }

        log_debug("[mpp_to_tcp_t] read %d bytes from mpp\n", curr_size);

        if (curr_size == 0) goto close;

        if (subflow_write(sf, buf, curr_size) < curr_size)
        {
            fprintf(stderr, "subflow_write error\n");
            goto close;
        }

        if (curr_size < BUF_SIZE) goto close;
    }

close:
    subflow_shutdown(sf, SHUT_WR);
    mpp_conn_shutdown(conn, SHUT_RD);
    return NULL;
}

static void* bind_tcp_mpp(void* args)
{
    pthread_t t[2];

    pthread_create(&t[0], NULL, tcp_to_mpp_t, args);
    pthread_create(&t[1], NULL, mpp_to_tcp_t, args);

    pthread_join(t[0], NULL);
    pthread_join(t[1], NULL);

    free(args);

    return NULL;
}

void* mpp_ingress_thread(void* _args)
{
    return NULL;
}

void* mpp_egress_thread(void* _args)
{
    return NULL;
}

int mpp_ingress_start(subflow* sf, char** mpp_ip, int* mpp_port, int* write_ratio, int mpp_sf_num,
                      mpp_source** src_ptr, mpp_conn** conn_ptr, pthread_t** t_ptr)
{
    mpp_source* src = (mpp_source*) malloc(sizeof(mpp_source));
    if(!src) goto close_sf;
    mpp_conn* conn;

    if (mpp_source_init(src, (const char**) mpp_ip, (const int*) mpp_port, mpp_sf_num) == -1)
    {
        fprintf(stderr, "mpp_source_init error\n");
        goto close_src;
    }

    log_info("[MPP_INGRESS_START] init successfully\n");

    if ((conn = mpp_source_connect(src)) == NULL)
    {
        fprintf(stderr, "mpp_source_connect error\n");
        goto close_src;
    }

    log_info("[MPP_INGRESS_START] connect successfully\n");

    if (mpp_conn_start(conn) == -1)
    {
        fprintf(stderr, "mpp_conn_start error\n");
        goto close_conn;
    }

    log_info("[MPP_INGRESS_START] start successfully\n");

    tcp_mpp_args* args = (tcp_mpp_args*) malloc(sizeof(tcp_mpp_args));
    args->sf = sf;
    args->conn = conn;

    pthread_t* t = (pthread_t*) malloc(2 * sizeof(pthread_t));

    pthread_create(&t[0], NULL, tcp_to_mpp_t, (void*) args);
    pthread_create(&t[1], NULL, mpp_to_tcp_t, (void*) args);

    *src_ptr = src;
    *conn_ptr = conn;
    *t_ptr = t;
    return 0;

close_conn:
    mpp_conn_stop(conn);
close_src:
    mpp_source_clean(src);
    free(src);
close_sf:
    subflow_close(sf);
    free(sf);
    return -1;
}

void mpp_ingress_clean(subflow* sf, mpp_source* src, mpp_conn* conn, pthread_t* t)
{
    pthread_join(t[0], NULL);
    pthread_join(t[1], NULL);

    mpp_conn_stop(conn);
    mpp_source_clean(src);
    subflow_close(sf);
    free(sf);
}

int mpp_egress_start(mpp_conn* conn, int* write_ratio, char* tcp_ip, int tcp_port,
                     subflow** sf_ptr, pthread_t** t_ptr)
{

    subflow* sf = (subflow*) malloc(sizeof(subflow));
    if(!sf) goto close_conn;

    if (mpp_conn_start(conn) == -1)
    {
        fprintf(stderr, "mpp_conn_start error\n");
        goto close_conn;
    }
    
    if (client_init(sf, tcp_ip, tcp_port) == -1)
    {
        fprintf(stderr, "client_init error\n");
        goto close_conn;
    };

    log_info("[MPP_EGRESS_THREAD] init successfully\n");

    if(client_connect(sf) == -1)
    {
        fprintf(stderr, "client_connect error\n");
        goto close_sf;
    }

    log_info("[MPP_EGRESS_THREAD] connect successfully\n");

    tcp_mpp_args* args = (tcp_mpp_args*) malloc(sizeof(tcp_mpp_args));
    args->sf = sf;
    args->conn = conn;

    pthread_t* t = (pthread_t*) malloc(2 * sizeof(pthread_t));

    pthread_create(&t[0], NULL, tcp_to_mpp_t, (void*) args);
    pthread_create(&t[1], NULL, mpp_to_tcp_t, (void*) args);

    *sf_ptr = sf;
    *t_ptr = t;
    return 0;

close_sf:
    subflow_close(sf);
close_conn:
    mpp_conn_stop(conn);
    return -1;
}

void mpp_egress_clean(subflow* sf, mpp_conn* conn, pthread_t* t)
{
    pthread_join(t[0], NULL);
    pthread_join(t[1], NULL);

    subflow_close(sf);
    mpp_conn_stop(conn);
}

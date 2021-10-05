#ifndef __MPP_PROXY_CONN_H__
#define __MPP_PROXY_CONN_H__ 1

#include "mpp_header.h"
#include "subflow.h"

#define PROXY_BUF_SEC_NUM 500
#define PROXY_BUF_SEC_SIZE DEFAULT_MPP_TOTAL_LEN

// direction of proxy
// SINK corresponds to listen side, SRC corresponds to backend side
typedef enum { SINK_SRC = 0, SRC_SINK } proxy_dir;

typedef struct mpp_proxy_conn mpp_proxy_conn;

typedef struct proxy_thread_args
{
    mpp_proxy_conn* conn;
    subflow* sf;
    proxy_dir dir;
    int* token;
} proxy_thread_args;

struct mpp_proxy_conn
{
    int sink_sf_num;
    subflow* sink_sf;

    int src_sf_num;
    subflow* src_sf;

    pthread_cond_t buf_read_cond[2];
    pthread_cond_t buf_write_cond[2];
    pthread_mutex_t buf_info_block[2];
    int buf_head[2]; // next place to write
    int buf_tail[2]; // next place to read
    int valid_data_num[2];
    int valid_space_num[2];

    pthread_mutex_t buf_sec_lock[2][PROXY_BUF_SEC_NUM];
    int pkt_size[2][PROXY_BUF_SEC_NUM];
    char buf[2][PROXY_BUF_SEC_NUM * PROXY_BUF_SEC_SIZE]; // circular buffer

    pthread_mutex_t active_thread_num_lock[2];
    int active_write_t_num[2];
    int active_read_t_num[2];

    pthread_mutex_t stat_lock;
    char status; 
    // 0: normal; 1: error

    pthread_t* sink_src_read_t;
    pthread_t* sink_src_write_t;
    pthread_t* src_sink_read_t;
    pthread_t* src_sink_write_t;

    /*
     * below are data structures related to tokens
     */
    pthread_mutex_t token_lock[2];
    pthread_cond_t token_cond[2];
    int* write_token[2];
};

/*
 * create mpp_proxy_conn data structure
 * return 0 if success; -1 otherwise
 */
int mpp_proxy_conn_init(mpp_proxy_conn* conn);

/*
 * clean mpp_proxy_conn data structure
 * return 0 if success; -1 otherwise
 */
int mpp_proxy_conn_clean(mpp_proxy_conn* conn);

/*
 * start mpp_proxy_conn
 * return 0 if success; -1 otherwise
 */
int mpp_proxy_conn_start(mpp_proxy_conn* conn);

/*
 * stop mpp_proxy_conn
 * return 0 if success; -1 otherwise
 */
int mpp_proxy_conn_stop(mpp_proxy_conn* conn);

/*
 * run mpp_proxy_conn until stop
 * return 0 if success; -1 otherwise
 */
int mpp_proxy_conn_run(mpp_proxy_conn* conn);

#endif /*__MPP__PROXY_CONN_H__*/

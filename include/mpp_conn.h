#ifndef __MPP_CONN_H__
#define __MPP_CONN_H__ 1

#include "subflow.h"
#include "mpp_header.h"

// put raw MPP pkt directly into read buffer
#define READ_BUF_RAW_PKT 1

#define WRITE_BUF_SEC_NUM 20000
#define WRITE_BUF_SEC_SIZE DEFAULT_MPP_TOTAL_LEN

#define READ_BUF_SEC_NUM 20000
#ifdef READ_BUF_RAW_PKT
    #define READ_BUF_SEC_SIZE DEFAULT_MPP_TOTAL_LEN
#else
    #define READ_BUF_SEC_SIZE DEFAULT_MPP_PAYLOAD_LEN
#endif

typedef struct mpp_conn mpp_conn;

typedef struct thread_args
{
    mpp_conn* conn;
    subflow* sf;
    int* token;
} thread_args;

struct mpp_conn
{
    int sf_num;
    subflow* sf;

    /*
     * below are data structures related to write buffer
     */
    pthread_cond_t write_master_cond;

    pthread_cond_t write_seq_cond;
    pthread_mutex_t write_seq_lock;
    int next_write_seq; // next_write_seq that mpp_source_write should use
    int min_write_seq; // minimum seq_num that not start to be written to remote yet
    int min_write_seq_finish; // minimum seq_num that not totally written to remote yet 

    pthread_mutex_t write_buf_lock[WRITE_BUF_SEC_NUM];
    int write_buf_pkt_size[WRITE_BUF_SEC_NUM]; // 0: no data; > 0: packet size
    char write_buf[WRITE_BUF_SEC_NUM * WRITE_BUF_SEC_SIZE];

    pthread_t* write_thread;
    thread_args* write_t_args;

    /*
     * below are data structures related to read buffer
     */
    pthread_cond_t read_master_cond;

    pthread_cond_t read_seq_cond;
    pthread_mutex_t read_seq_lock;
    int min_read_seq; // this is the minimum seq_num that not read from remote yet
#ifdef READ_BUF_RAW_PKT
    int next_read_seq; // next_read_seq that mpp_sink_read should use
#endif

#ifdef READ_BUF_RAW_PKT
    pthread_mutex_t read_buf_lock[READ_BUF_SEC_NUM];
    int read_buf_first_byte[READ_BUF_SEC_NUM]; // -1 means no data in this section
    mpp_header_readable read_buf_hdr_readable[READ_BUF_SEC_NUM];
    char read_buf[READ_BUF_SEC_NUM * READ_BUF_SEC_SIZE];
#else
    // read buf needs to be allocated on the heap for reallocation reason
    int min_read_seq_null; // 1 for true, 0 for false; stick together with read_seq_lock

    int read_buf_size;

    int* read_buf_data_loc;
    pthread_mutex_t* read_buf_lock;
    int* read_buf_first_byte; // -1 means no data in this section
    mpp_header_readable* read_buf_hdr_readable;
    char* read_buf;

    pthread_cond_t read_buf_info_cond;
    pthread_mutex_t read_buf_info_lock;
    int active_read_buf_thread;
    int read_buf_status; // 0: normal; 1: updating
#endif

    pthread_t* read_thread;
    thread_args* read_t_args;

    /*
     * below are data structures related to thread status
     */
    pthread_mutex_t thread_stat_lock;
    int active_write_thread_num;
    int active_read_thread_num;
    // 0: normal; 1: error; 2 force write thread quit; 4 force read thread quit
    // 8: update_topo in process;
    char thread_status;

    /*
     * below are data structures related to tokens
     */
    pthread_mutex_t token_lock;
    pthread_cond_t token_cond;
    int* write_token;
};

/*
 * start mpp conn daemon
 * return 0 if success; -1 otherwise
 */
int mpp_conn_start(mpp_conn* conn);

/*
 * shutdown mpp conn
 * return 0 if success; -1 otherwise
 */
int mpp_conn_shutdown(mpp_conn* conn, int how);

/*
 * stop mpp conn daemon
 * return 0 if success; -1 otherwise
 */
int mpp_conn_stop(mpp_conn* conn);

/*
 * write data to mpp_conn, len should > 0 
 * returns the number of bytes that write
 * must succeed, unless error happens
 */
int mpp_conn_write(mpp_conn* conn, const char* buf, int len);

/*
 * read data from mpp_conn, len should > 0 
 * returns the number of bytes that read
 * must succeed, unless the other side closes the connection
 * return the number of bytes read if success; -1 otherwise
 */
int mpp_conn_read(mpp_conn* conn, char* buf, int len);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Below are functions used for updating topologies 
///////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * change conn sf to others
 * return 0 if success; -1 otherwise
 */
int mpp_conn_change_sfs(mpp_conn* conn, subflow* new_sf, int new_sf_num);

/*
 * start changing topology
 */
void mpp_conn_change_topo_start(mpp_conn* conn);

#endif /*__MPP_CONN_H__*/

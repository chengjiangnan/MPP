#include "mpp_proxy_conn.h"
#include "mpp_header.h"
#include "utils.h"
#include "output_log.h"
#include "subflow.h"

#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static void thread_error(mpp_proxy_conn* conn)
{
    pthread_mutex_lock(&conn->stat_lock);
    conn->status |= 1;
    pthread_mutex_unlock(&conn->stat_lock);
}

static int check_thread_status(mpp_proxy_conn* conn)
{
    pthread_mutex_lock(&conn->stat_lock);
    int status = conn->status;
    pthread_mutex_unlock(&conn->stat_lock);
    return status;
}

static void decrease_active_write_t_num(mpp_proxy_conn* conn, proxy_dir dir)
{
    log_info("decrease_active_write_t on dir [%d] write_t %d\n", dir, conn->active_write_t_num[dir]);
    pthread_mutex_lock(&conn->active_thread_num_lock[dir]);
    --(conn->active_write_t_num[dir]);
    if (conn->active_write_t_num[dir] == 0) pthread_cond_broadcast(&conn->buf_read_cond[dir]);
    pthread_mutex_unlock(&conn->active_thread_num_lock[dir]);
}

static int check_active_write_t_num(mpp_proxy_conn* conn, proxy_dir dir)
{
    pthread_mutex_lock(&conn->active_thread_num_lock[dir]);
    int num = conn->active_write_t_num[dir];
    pthread_mutex_unlock(&conn->active_thread_num_lock[dir]);
    return num;
}

static void increase_active_read_t_num(mpp_proxy_conn* conn, proxy_dir dir)
{
    pthread_mutex_lock(&conn->active_thread_num_lock[dir]);
    ++(conn->active_read_t_num[dir]);
    pthread_mutex_unlock(&conn->active_thread_num_lock[dir]);
}

static void decrease_active_read_t_num(mpp_proxy_conn* conn, proxy_dir dir)
{
    pthread_mutex_lock(&conn->active_thread_num_lock[dir]);
    --(conn->active_read_t_num[dir]);
    if (conn->active_read_t_num[dir] == 0)
    {
        pthread_cond_broadcast(&conn->buf_write_cond[dir]);
    }
    pthread_mutex_unlock(&conn->active_thread_num_lock[dir]);

    // if (conn->active_read_t_num[dir] == 0)
    // {
    //     pthread_mutex_lock(&conn->token_lock[dir]);
    //     pthread_cond_broadcast(&conn->token_cond[dir]);
    //     pthread_mutex_unlock(&conn->token_lock[dir]);
    // }
}

static int check_active_read_t_num(mpp_proxy_conn* conn, proxy_dir dir)
{
    pthread_mutex_lock(&conn->active_thread_num_lock[dir]);
    int num = conn->active_read_t_num[dir];
    pthread_mutex_unlock(&conn->active_thread_num_lock[dir]);
    return num;
}

static void read_thread_close(mpp_proxy_conn* conn, proxy_dir dir, subflow* sf)
{
    subflow_shutdown(sf, SHUT_RD);
    decrease_active_read_t_num(conn, dir);
}

static void write_thread_close(mpp_proxy_conn* conn, proxy_dir dir, subflow* sf)
{
    subflow_shutdown(sf, SHUT_WR);
    decrease_active_write_t_num(conn, dir);
}

static void* read_t(void* _args)
{
    proxy_thread_args* args = (proxy_thread_args*) _args;
    mpp_proxy_conn* conn = args->conn;
    proxy_dir dir = args->dir;
    subflow* sf = args->sf;
    free(args);

    int sec_idx;

    mpp_header hdr;
    mpp_header_readable hdr_readable;
    int read_size;

    while (1)
    {
        read_size = subflow_read(sf, (char*) &hdr, MPP_HEADER_LEN);

        log_debug("[read_t] (conn from %s:%d) read %d bytes\n",
                  sf->remote_ip, sf->remote_port, read_size);

        if (read_size == -1)
        {
            thread_error(conn);
            read_thread_close(conn, dir, sf);
            return NULL;
        }

        // remote side has closed the connection
        if (read_size == 0)
        {
            read_thread_close(conn, dir, sf);
            return NULL;
        }

        assert(read_size == MPP_HEADER_LEN);

        mpp_parse_header(&hdr, &hdr_readable);

        log_debug("[read_t] (conn from %s:%d) header info: payload %d bytes, seq_num %d\n",
                  sf->remote_ip, sf->remote_port, hdr_readable.payload_len, hdr_readable.seq_num);

        // require space to read
        pthread_mutex_lock(&conn->buf_info_block[dir]);
        while (conn->valid_space_num[dir] == 0)
        {
            log_warn("[read_t] (conn from %s:%d) no space to read!\n",
                     sf->remote_ip, sf->remote_port);
            if (check_active_write_t_num(conn, dir) == 0)
            {
                pthread_mutex_unlock(&conn->buf_info_block[dir]);
                read_thread_close(conn, dir, sf);
                return NULL;
            };
            pthread_cond_wait(&conn->buf_read_cond[dir], &conn->buf_info_block[dir]);
            if ((check_thread_status(conn) & 1) == 1)
            {
                pthread_mutex_unlock(&conn->buf_info_block[dir]);
                read_thread_close(conn, dir, sf);
                return NULL;
            }
        }
        // now we occupy a space to write
        --(conn->valid_space_num[dir]);
        sec_idx = conn->buf_head[dir];
        conn->buf_head[dir] = (sec_idx + 1) % PROXY_BUF_SEC_NUM;

        // for safety we lock this section first
        pthread_mutex_lock(conn->buf_sec_lock[dir] + sec_idx);

        pthread_mutex_unlock(&conn->buf_info_block[dir]);

        assert(conn->pkt_size[dir][sec_idx] < 0);

        memcpy(conn->buf[dir] + sec_idx * PROXY_BUF_SEC_SIZE, (char*) &hdr, MPP_HEADER_LEN);
        read_size = subflow_read(sf,
                                 conn->buf[dir] + sec_idx * PROXY_BUF_SEC_SIZE + MPP_HEADER_LEN,
                                 hdr_readable.payload_len);

        if (read_size == -1)
        {
            thread_error(conn);
            read_thread_close(conn, dir, sf);
            pthread_mutex_unlock(conn->buf_sec_lock[dir] + sec_idx);
            return NULL;
        }

        assert(read_size == hdr_readable.payload_len);

        conn->pkt_size[dir][sec_idx] = MPP_HEADER_LEN + hdr_readable.payload_len;

        log_debug("[read_t] (conn from %s:%d) seq_num %d is writen on sec %d\n",
                  sf->remote_ip, sf->remote_port, hdr_readable.seq_num, sec_idx);

        pthread_mutex_unlock(conn->buf_sec_lock[dir] + sec_idx);

        // update token
        // pthread_mutex_lock(&conn->token_lock[dir]);

        // int sf_num = 0;
        // if (dir == SINK_SRC)
        // {
        //     sf_num = conn->src_sf_num;
        // }
        // else
        // {
        //     sf_num = conn->sink_sf_num;
        // }

        // int min_idx = 0;
        // int min_value = conn->write_token[dir][0];

        // for (int i = 1; i < sf_num; ++i)
        // {
        //     if (conn->write_token[dir][i] < min_value)
        //     {
        //         min_idx = i;
        //         min_value = conn->write_token[dir][i];
        //     }
        // }

        // ++(conn->write_token[dir][min_idx]);

        // pthread_cond_broadcast(&conn->token_cond[dir]);
        // pthread_mutex_unlock(&conn->token_lock[dir]);

        // there is new data to read
        pthread_mutex_lock(&conn->buf_info_block[dir]);
        ++(conn->valid_data_num[dir]);
        pthread_cond_signal(&conn->buf_write_cond[dir]);
        pthread_mutex_unlock(&conn->buf_info_block[dir]);
        
    }
}

static void* write_t(void* _args)
{
    proxy_thread_args* args = (proxy_thread_args*) _args;
    mpp_proxy_conn* conn = args->conn;
    proxy_dir dir = args->dir;
    subflow* sf = args->sf;
    int* token = args->token;
    free(args);

    int sec_idx;
    int write_size;

    while (1)
    {

        log_debug("[write_t] (conn to %s:%d) require new data\n", sf->remote_ip, sf->remote_port);

        // pthread_mutex_lock(&conn->token_lock[dir]);
        // log_debug("[token][write_t] (conn to %s:%d) token num: %d\n", 
        //           sf->remote_ip, sf->remote_port, *token);
        // while (*token <= 0)
        // {
        //     log_trace("[write_t] (conn to %s:%d) no token\n", sf->remote_ip, sf->remote_port);
        //     if (check_active_read_t_num(conn, dir) == 0)
        //     {
        //         log_trace("[write_t] (conn to %s:%d) normal quit\n",
        //                   sf->remote_ip, sf->remote_port);
        //         pthread_mutex_unlock(&conn->token_lock[dir]);
        //         write_thread_close(conn, dir, sf);
        //         return NULL;
        //     };
        //     pthread_cond_wait(&conn->token_cond[dir], &conn->token_lock[dir]);
        //     if ((check_thread_status(conn) & 1) == 1)
        //     {
        //         pthread_mutex_unlock(&conn->token_lock[dir]);
        //         write_thread_close(conn, dir, sf);
        //         return NULL;
        //     }
        // }
        // --(*token);
        // pthread_mutex_unlock(&conn->token_lock[dir]);

        pthread_mutex_lock(&conn->buf_info_block[dir]);
        // after adding token section, we will never enter this while loop
        while (conn->valid_data_num[dir] == 0)
        {
            log_trace("[write_t] (conn to %s:%d) no new data\n", sf->remote_ip, sf->remote_port);
            if (check_active_read_t_num(conn, dir) == 0)
            {
                log_trace("[write_t] (conn to %s:%d) normal quit\n",
                          sf->remote_ip, sf->remote_port);
                pthread_mutex_unlock(&conn->buf_info_block[dir]);
                write_thread_close(conn, dir, sf);
                return NULL;
            };
            pthread_cond_wait(&conn->buf_write_cond[dir], &conn->buf_info_block[dir]);
            if ((check_thread_status(conn) & 1) == 1)
            {
                pthread_mutex_unlock(&conn->buf_info_block[dir]);
                write_thread_close(conn, dir, sf);
                return NULL;
            }
        }
        // now we occupy a piece of data to read
        --(conn->valid_data_num[dir]);
        sec_idx = conn->buf_tail[dir];
        conn->buf_tail[dir] = (sec_idx + 1) % PROXY_BUF_SEC_NUM;

        // for safety we lock this section first
        pthread_mutex_lock(conn->buf_sec_lock[dir] + sec_idx);

        pthread_mutex_unlock(&conn->buf_info_block[dir]);

        log_debug("[write_t] (conn to %s:%d) going to write sec %d\n",
                  sf->remote_ip, sf->remote_port, sec_idx);

        // the read thread who writes this sec hasn't get the lock yet 
        // while (conn->pkt_size[dir][sec_idx] < 0)
        // {
        //     pthread_cond_wait(conn->buf_sec_cond[dir] + sec_idx,
        //                       conn->buf_sec_lock[dir] + sec_idx);
        // }

        assert(conn->pkt_size[dir][sec_idx] >= 0);
        write_size = subflow_write(sf, conn->buf[dir] + sec_idx * PROXY_BUF_SEC_SIZE,
                                   conn->pkt_size[dir][sec_idx]);

        // TODO: we may want to send the data on other subflows is error happens
        if(write_size < conn->pkt_size[dir][sec_idx])
        {
            thread_error(conn);
            write_thread_close(conn, dir, sf);
            return NULL;
        }

        conn->pkt_size[dir][sec_idx] = -1;
        pthread_mutex_unlock(conn->buf_sec_lock[dir] + sec_idx);

        // there is new space to write
        pthread_mutex_lock(&conn->buf_info_block[dir]);
        ++(conn->valid_space_num[dir]);
        pthread_cond_signal(&conn->buf_read_cond[dir]);
        pthread_mutex_unlock(&conn->buf_info_block[dir]);
    }
}

int mpp_proxy_conn_init(mpp_proxy_conn* conn)
{
    if(!conn) return -1;

    for (proxy_dir i = SINK_SRC; i <= SRC_SINK; ++i)
    {
        if (pthread_cond_init(&conn->buf_read_cond[i], NULL) != 0 ||
            pthread_cond_init(&conn->buf_write_cond[i], NULL) != 0 ||
            pthread_cond_init(&conn->token_cond[i], NULL) != 0 ||
            pthread_mutex_init(&conn->buf_info_block[i], NULL) != 0 ||
            pthread_mutex_init(&conn->active_thread_num_lock[i], NULL) != 0 ||
            pthread_mutex_init(&conn->token_lock[i], NULL) != 0) return -1;

        for (int j = 0; j < PROXY_BUF_SEC_NUM; ++j)
        {
            conn->pkt_size[i][j] = -1;
            if (pthread_mutex_init(&conn->buf_sec_lock[i][j], NULL) != 0) return -1;
        }

        conn->buf_head[i] = 0;
        conn->buf_tail[i] = 0;
        conn->valid_data_num[i] = 0;
        conn->valid_space_num[i] = PROXY_BUF_SEC_NUM;

        switch (i)
        {
            case SINK_SRC:
                conn->write_token[i] = (int*) malloc(conn->src_sf_num * sizeof(int));
                if (!conn->write_token[i]) return -1;

                for (int j = 0; j < conn->src_sf_num; ++j)
                {
                    conn->write_token[i][j] = 0;
                }

                conn->active_write_t_num[i] = conn->src_sf_num;
                conn->active_read_t_num[i] = conn->sink_sf_num;
                break;
            case SRC_SINK:
                conn->write_token[i] = (int*) malloc(conn->sink_sf_num * sizeof(int));
                if (!conn->write_token[i]) return -1;

                for (int j = 0; j < conn->sink_sf_num; ++j)
                {
                    conn->write_token[i][j] = 0;
                }

                conn->active_write_t_num[i] = conn->sink_sf_num;
                conn->active_read_t_num[i] = conn->src_sf_num;
                break;
        }
    }

    if (pthread_mutex_init(&conn->stat_lock, NULL) != 0) return -1;
    conn->status = 0;

    return 0;
}

int mpp_proxy_conn_clean(mpp_proxy_conn* conn)
{
    int retval = 0;

    for (proxy_dir i = SINK_SRC; i <= SRC_SINK; ++i)
    {
        if (pthread_cond_destroy(&conn->buf_read_cond[i]) != 0) retval = -1;
        if (pthread_cond_destroy(&conn->buf_write_cond[i]) != 0) retval = -1;
        if (pthread_cond_destroy(&conn->token_cond[i]) != 0) retval = -1;
        if (pthread_mutex_destroy(&conn->buf_info_block[i]) != 0) retval = -1;
        if (pthread_mutex_destroy(&conn->active_thread_num_lock[i]) != 0) retval = -1;
        if (pthread_mutex_destroy(&conn->token_lock[i]) != 0) retval = -1;

        for (int j = 0; j < PROXY_BUF_SEC_NUM; ++j)
        {
            if (pthread_mutex_destroy(&conn->buf_sec_lock[i][j]) != 0) retval = -1;
        }

        free(conn->write_token[i]);
    }

    if (pthread_mutex_destroy(&conn->stat_lock) != 0) retval = -1;

    free(conn->sink_sf);
    free(conn->src_sf);
    free(conn);

    return retval;
}

int spawn_sink_sf_threads(mpp_proxy_conn* conn, pthread_t* read_thread, pthread_t* write_thread,
                          subflow* sf, int* write_token)
{
    // TODO: error handling
    proxy_thread_args* read_args = (proxy_thread_args*) malloc(sizeof(proxy_thread_args));
    proxy_thread_args* write_args = (proxy_thread_args*) malloc(sizeof(proxy_thread_args));
    read_args->conn = conn;
    read_args->sf = sf;
    read_args->dir = SINK_SRC;
    read_args->token = NULL;
    write_args->conn = conn;
    write_args->sf = sf;
    write_args->dir = SRC_SINK;
    write_args->token = write_token;
    if (pthread_create(read_thread, NULL, read_t, (void*) read_args) != 0 ||
        pthread_create(write_thread, NULL, write_t, (void*) write_args) != 0)
        return -1;
    return 0;
}

int spawn_src_sf_threads(mpp_proxy_conn* conn, pthread_t* read_thread, pthread_t* write_thread,
                         subflow* sf, int* write_token)
{
    proxy_thread_args* read_args = (proxy_thread_args*) malloc(sizeof(proxy_thread_args));
    proxy_thread_args* write_args = (proxy_thread_args*) malloc(sizeof(proxy_thread_args));
    read_args->conn = conn;
    read_args->sf = sf;
    read_args->dir = SRC_SINK;
    read_args->token = NULL;
    write_args->conn = conn;
    write_args->sf = sf;
    write_args->dir = SINK_SRC;
    write_args->token = write_token;
    if (pthread_create(read_thread, NULL, read_t, (void*) read_args) != 0 ||
        pthread_create(write_thread, NULL, write_t, (void*) write_args) != 0)
        return -1;

    return 0;
}

int mpp_proxy_conn_start(mpp_proxy_conn* conn)
{
    conn->sink_src_read_t = (pthread_t*) malloc(conn->sink_sf_num * sizeof(pthread_t));
    if (!conn->sink_src_read_t) return -1;

    conn->sink_src_write_t = (pthread_t*) malloc(conn->src_sf_num * sizeof(pthread_t));
    if (!conn->sink_src_write_t) return -1;

    conn->src_sink_read_t = (pthread_t*) malloc(conn->src_sf_num * sizeof(pthread_t));
    if (!conn->src_sink_read_t) return -1;

    conn->src_sink_write_t = (pthread_t*) malloc(conn->sink_sf_num * sizeof(pthread_t));
    if (!conn->src_sink_write_t) return -1;

    for (int i = 0; i < conn->sink_sf_num; ++i) 
        if (spawn_sink_sf_threads(conn, &conn->sink_src_read_t[i], &conn->src_sink_write_t[i],
                                  &(conn->sink_sf[i]), &(conn->write_token[SRC_SINK][i]))) 
            return -1;

    for (int i = 0; i < conn->src_sf_num; ++i)
        if (spawn_src_sf_threads(conn, &conn->src_sink_read_t[i], &conn->sink_src_write_t[i],
                                 &(conn->src_sf[i]), &(conn->write_token[SINK_SRC][i]))) 
            return -1;

    log_debug("[PROXY_CONN_START] spawn all the threads successfully\n");
    return 0;
}

int mpp_proxy_conn_stop(mpp_proxy_conn* conn)
{
    int retval = 0;
    if (!conn->sink_src_read_t || !conn->sink_src_write_t ||
        !conn->src_sink_read_t || !conn->src_sink_write_t) goto close;

    for (int i = 0; i < conn->sink_sf_num; ++i)
        if (pthread_join(conn->sink_src_read_t[i], NULL) != 0 ||
            pthread_join(conn->src_sink_write_t[i], NULL) != 0) return -1;

    log_debug("[PROXY_CONN_STOP] all the sink threads have returned\n");

    for (int i = 0; i < conn->src_sf_num; ++i)
        if (pthread_join(conn->src_sink_read_t[i], NULL) != 0 ||
            pthread_join(conn->sink_src_write_t[i], NULL) != 0) return -1;

    log_debug("[PROXY_CONN_STOP] all the src threads have returned\n");

close:
    for (int i = 0; i < conn->src_sf_num; ++i)
        if (subflow_close(&conn->src_sf[i]) != 0) retval = -1;
    for (int i = 0; i < conn->sink_sf_num; ++i)
        if (subflow_close(&conn->sink_sf[i]) != 0) retval = -1;

    free(conn->sink_src_read_t);
    free(conn->sink_src_write_t);
    free(conn->src_sink_read_t);
    free(conn->src_sink_write_t);
    return retval;
}

int mpp_proxy_conn_run(mpp_proxy_conn* conn)
{
    int retval = 0;
    if (mpp_proxy_conn_start(conn) == -1) retval = -1;
    if (mpp_proxy_conn_stop(conn) == -1) retval = -1;
    return retval;
}

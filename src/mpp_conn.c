#include "mpp_conn.h"
#include "mpp_header.h"
#include "utils.h"
#include "output_log.h"
#include "subflow.h"

#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static void thread_error(mpp_conn* conn)
{
    // force masters to quit
    pthread_mutex_lock(&conn->thread_stat_lock);
    conn->thread_status |= 1;
    pthread_cond_signal(&conn->write_master_cond);
    pthread_cond_signal(&conn->read_master_cond);
    pthread_mutex_unlock(&conn->thread_stat_lock);

    // force threads to quit
    pthread_mutex_lock(&conn->write_seq_lock);
    pthread_cond_broadcast(&conn->write_seq_cond);
    pthread_mutex_unlock(&conn->write_seq_lock);

    pthread_mutex_lock(&conn->read_seq_lock);
    pthread_cond_broadcast(&conn->read_seq_cond);
    pthread_mutex_unlock(&conn->read_seq_lock);

    // pthread_mutex_lock(&conn->token_lock);
    // pthread_cond_broadcast(&conn->token_cond);
    // pthread_mutex_unlock(&conn->token_lock);
}

static void force_write_thread_quit(mpp_conn* conn)
{
    pthread_mutex_lock(&conn->thread_stat_lock);
    conn->thread_status |= 2;
    pthread_mutex_unlock(&conn->thread_stat_lock);

    // force threads to quit
    pthread_mutex_lock(&conn->write_seq_lock);
    pthread_cond_broadcast(&conn->write_seq_cond);
    pthread_mutex_unlock(&conn->write_seq_lock);

    // pthread_mutex_lock(&conn->token_lock);
    // pthread_cond_broadcast(&conn->token_cond);
    // pthread_mutex_unlock(&conn->token_lock);
}

static void reset_write_thread_quit_flag(mpp_conn* conn)
{
    pthread_mutex_lock(&conn->thread_stat_lock);
    conn->thread_status &= ~2;
    pthread_mutex_unlock(&conn->thread_stat_lock);
}

static void force_read_thread_quit(mpp_conn* conn)
{
    pthread_mutex_lock(&conn->thread_stat_lock);
    conn->thread_status |= 4;
    pthread_mutex_unlock(&conn->thread_stat_lock);

    // force threads to quit
    pthread_mutex_lock(&conn->read_seq_lock);
    pthread_cond_broadcast(&conn->read_seq_cond);
    pthread_mutex_unlock(&conn->read_seq_lock);
}

static void change_topo_start(mpp_conn* conn)
{
    pthread_mutex_lock(&conn->thread_stat_lock);
    conn->thread_status |= 8;
    pthread_mutex_unlock(&conn->thread_stat_lock);
}

static void change_topo_finish(mpp_conn* conn)
{
    pthread_mutex_lock(&conn->thread_stat_lock);
    conn->thread_status &= ~8;
    pthread_mutex_unlock(&conn->thread_stat_lock);
}

static int check_thread_status(mpp_conn* conn)
{
    pthread_mutex_lock(&conn->thread_stat_lock);
    char status = conn->thread_status;
    pthread_mutex_unlock(&conn->thread_stat_lock);

    return status;
}

static void decrease_active_write_thread_num(mpp_conn* conn)
{
    pthread_mutex_lock(&conn->thread_stat_lock);
    --(conn->active_write_thread_num);
    if (conn->active_write_thread_num == 0) pthread_cond_signal(&conn->write_master_cond);
    pthread_mutex_unlock(&conn->thread_stat_lock);
}

static void increase_active_read_thread_num(mpp_conn* conn, int num)
{
    pthread_mutex_lock(&conn->thread_stat_lock);
    conn->active_read_thread_num += num;
    pthread_mutex_unlock(&conn->thread_stat_lock);
}

static void decrease_active_read_thread_num(mpp_conn* conn)
{
    pthread_mutex_lock(&conn->thread_stat_lock);
    --(conn->active_read_thread_num);
    if (conn->active_read_thread_num == 0) pthread_cond_signal(&conn->read_master_cond);
    pthread_mutex_unlock(&conn->thread_stat_lock);
}

static int check_active_read_thread_num(mpp_conn* conn)
{
    pthread_mutex_lock(&conn->thread_stat_lock);
    int num = conn->active_read_thread_num;
    pthread_mutex_unlock(&conn->thread_stat_lock);
    return num;
}

static int check_active_write_thread_num(mpp_conn* conn)
{
    pthread_mutex_lock(&conn->thread_stat_lock);
    int num = conn->active_write_thread_num;
    pthread_mutex_unlock(&conn->thread_stat_lock);
    return num;
}

static void write_thread_close(mpp_conn* conn, subflow* sf)
{
    subflow_shutdown(sf, SHUT_WR);
    decrease_active_write_thread_num(conn);
}

static void read_thread_close(mpp_conn* conn, subflow* sf)
{
    subflow_shutdown(sf, SHUT_RD);
    decrease_active_read_thread_num(conn);
}

#ifndef READ_BUF_RAW_PKT
static void increase_active_read_buf_thread(mpp_conn* conn)
{
    pthread_mutex_lock(&conn->read_buf_info_lock);
    ++(conn->active_read_buf_thread);
    log_info("[READ_BUF_SIZE] increase_read_buf_size [%d]\n", conn->active_read_buf_thread);
    pthread_mutex_unlock(&conn->read_buf_info_lock);
}

static void decrease_active_read_buf_thread(mpp_conn* conn)
{
    pthread_mutex_lock(&conn->read_buf_info_lock);
    --(conn->active_read_buf_thread);
    log_info("[READ_BUF_SIZE] decrease_read_buf_size [%d]\n", conn->active_read_buf_thread);
    if (conn->active_read_buf_thread == 0) pthread_cond_signal(&conn->read_buf_info_cond);
    pthread_mutex_unlock(&conn->read_buf_info_lock);
}

static int check_read_buf_status(mpp_conn* conn)
{
    pthread_mutex_lock(&conn->read_buf_info_lock);
    int num = conn->read_buf_status;
    pthread_mutex_unlock(&conn->read_buf_info_lock);
    return num;
}

static int increase_read_buf_size(mpp_conn* conn)
{
    log_info("[READ_BUF_SIZE] trying to increase_read_buf_size [%d]\n",
             conn->read_buf_size);
    pthread_mutex_lock(&conn->read_buf_info_lock);
    conn->read_buf_status = 1;
    while (conn->active_read_buf_thread > 0)
    {
        log_info("[READ_BUF_SIZE] read_buf_size [%d]\n", conn->active_read_buf_thread);
        pthread_mutex_unlock(&conn->read_seq_lock);
        pthread_cond_signal(&conn->read_master_cond);
        pthread_cond_wait(&conn->read_buf_info_cond, &conn->read_buf_info_lock);
        pthread_mutex_lock(&conn->read_seq_lock);
    }

    int old_read_buf_size = conn->read_buf_size;

    conn->read_buf_size += conn->read_buf_size / 4;
    log_info("[READ_BUF_SIZE] increase from %d to %d\n",
             old_read_buf_size, conn->read_buf_size);

    conn->read_buf_data_loc = (int*) realloc(conn->read_buf_data_loc,
                                             conn->read_buf_size * sizeof(int));
    if(!conn->read_buf_data_loc)
    {
        pthread_mutex_unlock(&conn->read_buf_info_lock);
        return -1;
    }

    conn->read_buf_lock = \
        (pthread_mutex_t*) realloc(conn->read_buf_lock,
                                   conn->read_buf_size * sizeof(pthread_mutex_t));
    if(!conn->read_buf_lock)
    {
        pthread_mutex_unlock(&conn->read_buf_info_lock);
        return -1;
    }

    conn->read_buf_first_byte = (int*) realloc(conn->read_buf_first_byte,
                                               conn->read_buf_size * sizeof(int));
    if(!conn->read_buf_first_byte)
    {
        pthread_mutex_unlock(&conn->read_buf_info_lock);
        return -1;
    }

    conn->read_buf_hdr_readable = \
        (mpp_header_readable*) realloc(conn->read_buf_hdr_readable,
                                       conn->read_buf_size * sizeof(mpp_header_readable));
    if(!conn->read_buf_hdr_readable)
    {
        pthread_mutex_unlock(&conn->read_buf_info_lock);
        return -1;
    }

    conn->read_buf = (char*) realloc(conn->read_buf, conn->read_buf_size * READ_BUF_SEC_SIZE);
    if(!conn->read_buf)
    {
        pthread_mutex_unlock(&conn->read_buf_info_lock);
        return -1;
    }

    for (int i = old_read_buf_size; i < conn->read_buf_size; ++i)
    {
        conn->read_buf_first_byte[i] = -1;
        conn->read_buf_data_loc[i] = -1;
        if (pthread_mutex_init(conn->read_buf_lock + i, NULL) != 0)
        {       
            pthread_mutex_unlock(&conn->read_buf_info_lock);
            return -1;
        }
    }

    // reconfig read_buf_data_loc based on new read_buf_size
    int old_read_buf_data_loc[old_read_buf_size];

    for (int i = 0; i < old_read_buf_size; ++i)
    {
        old_read_buf_data_loc[i] = conn->read_buf_data_loc[i];
    }

    int head = conn->min_read_seq % old_read_buf_size;
    for (int i = head; i < old_read_buf_size; ++i)
    {
        int seq_num = i - head + conn->min_read_seq;
        conn->read_buf_data_loc[seq_num % conn->read_buf_size] = old_read_buf_data_loc[i];
    }

    for (int i = 0; i < head; ++i)
    {
        int seq_num = i - head + conn->min_read_seq + old_read_buf_size;
        conn->read_buf_data_loc[seq_num % conn->read_buf_size] = old_read_buf_data_loc[i];
    }

    for (int i = old_read_buf_size; i < conn->read_buf_size; ++i)
    {
        int seq_num = i - old_read_buf_size + conn->min_read_seq + old_read_buf_size;
        conn->read_buf_data_loc[seq_num % conn->read_buf_size] = i;
    }

    conn->read_buf_status = 0;
    pthread_cond_signal(&conn->read_master_cond);
    pthread_cond_broadcast(&conn->read_seq_cond);

    pthread_mutex_unlock(&conn->read_buf_info_lock);
    return 0;
}
#endif

static void* write_t(void* _args)
{
    thread_args* args = (thread_args*) _args;
    mpp_conn* conn = args->conn;
    subflow* sf = args->sf;
    int* token = args->token;

    int sec_idx;
    int curr_seq_num;

    while (1)
    {
        // quit when error happens; 
        // can also quit when force quit happens during update_topo
        if ((check_thread_status(conn) & 1) == 1 || 
            ((check_thread_status(conn) & 2) > 0 && (check_thread_status(conn) & 8) > 0))
        {
            write_thread_close(conn, sf);
            return NULL;
        }

        // pthread_mutex_lock(&conn->token_lock);
        // while (*token <= 0)
        // {
        //     if((check_thread_status(conn) & 3) > 0)
        //     {
        //         pthread_mutex_unlock(&conn->token_lock);
        //         write_thread_close(conn, sf);
        //         return NULL;
        //     }
        //     pthread_cond_wait(&conn->token_cond, &conn->token_lock);
        // }
        // --(*token);
        // pthread_mutex_unlock(&conn->token_lock);

        // wait until there is data in buf that we can use
        pthread_mutex_lock(&conn->write_seq_lock);

        while (conn->next_write_seq <= conn->min_write_seq)
        {
            if((check_thread_status(conn) & 3) > 0)
            {
                pthread_mutex_unlock(&conn->write_seq_lock);
                write_thread_close(conn, sf);
                return NULL;
            }

            pthread_cond_wait(&conn->write_seq_cond, &conn->write_seq_lock);
        }
        // we are going to use this seq_num
        curr_seq_num = conn->min_write_seq++;
        pthread_mutex_unlock(&conn->write_seq_lock);

        sec_idx = curr_seq_num % WRITE_BUF_SEC_NUM;

        pthread_mutex_lock(conn->write_buf_lock + sec_idx);

        // the section should have data
        assert(conn->write_buf_pkt_size[sec_idx] > 0);

        log_debug("[write_t] (conn to %s:%d) writing seq_num %d (pkt_size %d bytes) sec_idx: %d\n",
                  sf->remote_ip, sf->remote_port, curr_seq_num, conn->write_buf_pkt_size[sec_idx],
                  sec_idx);

        if (subflow_write(sf, conn->write_buf + sec_idx * WRITE_BUF_SEC_SIZE,
                          conn->write_buf_pkt_size[sec_idx]) < conn->write_buf_pkt_size[sec_idx])
        {
            thread_error(conn);
            write_thread_close(conn, sf);
            return NULL;
        }

        log_debug("[write_t] (conn to %s:%d) finish writing seq_num %d (pkt_size %d bytes)\n",
                  sf->remote_ip, sf->remote_port, curr_seq_num, conn->write_buf_pkt_size[sec_idx]);

        conn->write_buf_pkt_size[sec_idx] = 0;

        // update min_write_seq
        pthread_mutex_lock(&conn->write_seq_lock);
        ++(conn->min_write_seq_finish);
        pthread_cond_signal(&conn->write_master_cond);
        pthread_mutex_unlock(&conn->write_seq_lock);

        pthread_mutex_unlock(conn->write_buf_lock + sec_idx);
    }
}

int max_buf_usage = 0; // for debug

static void* read_t(void* _args)
{
    thread_args* args = (thread_args*) _args;
    mpp_conn* conn = args->conn;
    subflow* sf = args->sf;

    mpp_header hdr;
    mpp_header_readable hdr_readable;
    int sec_idx;
    int read_size;

    while (1)
    {
        if ((check_thread_status(conn) & 5) > 0)
        {
            read_thread_close(conn, sf);
            return NULL;
        }

        // read and parse header
        read_size = subflow_read(sf, (char*) &hdr, MPP_HEADER_LEN);

        log_debug("[read_t] (conn from %s:%d) read %d bytes (header)\n",
                  sf->remote_ip, sf->remote_port, read_size);

        if (read_size == -1)
        {
            thread_error(conn);
            read_thread_close(conn, sf);
            return NULL;
        }

        if (read_size == 0)
        {
            // other side closes socket, normal termination
            read_thread_close(conn, sf);
            return NULL;
        }

        // we should always be able to read a complete header
        assert(read_size == MPP_HEADER_LEN);

        mpp_parse_header(&hdr, &hdr_readable);

        log_debug("[read_t] (conn from %s:%d) header info: payload %d bytes, seq_num %d\n",
                  sf->remote_ip, sf->remote_port, hdr_readable.payload_len,
                  hdr_readable.seq_num);
        log_debug("[read_t] current min_read_seq is %d\n", conn->min_read_seq);

        // wait until there is space in buf (for current seq_num)
        pthread_mutex_lock(&conn->read_seq_lock);
#ifndef READ_BUF_RAW_PKT
        if (hdr_readable.seq_num == conn->min_read_seq) conn->min_read_seq_null = 0;
        log_info("[BUF_TEST] max_buf_usage: %d, buf_size: %d\n",
                 max_buf_usage, conn->read_buf_size);
#endif
        if (max_buf_usage < hdr_readable.seq_num - conn->min_read_seq + 1)
            max_buf_usage = hdr_readable.seq_num - conn->min_read_seq + 1;
        

#ifdef READ_BUF_RAW_PKT
        while (conn->next_read_seq >= conn->min_read_seq + READ_BUF_SEC_NUM)
#else
        while (hdr_readable.seq_num > conn->min_read_seq + conn->read_buf_size - 1)
#endif
        {

#ifndef READ_BUF_RAW_PKT
            // check whether read_buf doesn't have enough size,
            // or the application is reading slow
            if (conn->min_read_seq_null == 1 && check_read_buf_status(conn) == 0)
            {
                if (increase_read_buf_size(conn) == -1)
                {
                    log_warn("[read_t] increase_read_buf_size fails!\n");
                    thread_error(conn);
                    read_thread_close(conn, sf);
                    return NULL;
                }

                log_warn("increase_read_buf_size succeed [%d]\n", conn->read_buf_size);
                continue;
            }
#endif

            log_warn("[read_t] (conn from %s:%d) no space to read!\n",
                     sf->remote_ip, sf->remote_port);
            pthread_cond_wait(&conn->read_seq_cond, &conn->read_seq_lock);

            if((check_thread_status(conn) & 5) > 0)
            {
                pthread_mutex_unlock(&conn->read_seq_lock);
                read_thread_close(conn, sf);
                return NULL;
            }
        }

#ifdef READ_BUF_RAW_PKT
        sec_idx = conn->next_read_seq % READ_BUF_SEC_NUM;
        ++conn->next_read_seq;
#else
        sec_idx = conn->read_buf_data_loc[hdr_readable.seq_num % conn->read_buf_size];
#endif

        pthread_mutex_unlock(&conn->read_seq_lock);

#ifndef READ_BUF_RAW_PKT
        increase_active_read_buf_thread(conn);
#endif

        pthread_mutex_lock(conn->read_buf_lock + sec_idx);

        // the section is already writen, discard data
        // this won't happen in current implementation (unless packet is resent)
//         if (conn->read_buf_first_byte[sec_idx] != -1)
//         {
//             assert(read_buf_hdr_readable[sec_idx].seq_num >= hdr_readable.seq_num);
//             assert((read_buf_hdr_readable[sec_idx].seq_num - hdr_readable.seq_num) \
//                    % READ_BUF_SEC_NUM == 0);

//         log_warn("[read_t] (conn from %s:%d)  fails to write seq_num %d on sec %d\n",
//                  sf->remote_ip, sf->remote_port, hdr_readable.seq_num, sec_idx);

//             pthread_mutex_unlock(conn->read_buf_lock + sec_idx);

//             char temp_buf[hdr_readable.payload_len];
//             if (subflow_read(sf, temp_buf, hdr_readable.payload_len) < hdr_readable.payload_len)
//             {
//                 read_thread_error(conn);
//                 read_thread_close(conn, sf);
//                 return NULL;
//             }

//             continue;
//         }

        assert(conn->read_buf_first_byte[sec_idx] == -1);

        char* payload_pos = conn->read_buf + sec_idx * READ_BUF_SEC_SIZE;

#ifdef READ_BUF_RAW_PKT
        memcpy(payload_pos, (char*) &hdr, MPP_HEADER_LEN);
        payload_pos += MPP_HEADER_LEN;
#endif

        // the section is not writen
        log_debug("[read_t] (conn from %s:%d) going to read sec_idx %d\n",
                  sf->remote_ip, sf->remote_port, sec_idx);
        read_size = subflow_read(sf, payload_pos, hdr_readable.payload_len);

        log_debug("[read_t] (conn from %s:%d) write seq_num %d (payload %d bytes) on sec %d\n",
                  sf->remote_ip, sf->remote_port, hdr_readable.seq_num, read_size, sec_idx);

        if (read_size == -1)
        {
            pthread_mutex_unlock(conn->read_buf_lock + sec_idx);
#ifndef READ_BUF_RAW_PKT
            decrease_active_read_buf_thread(conn);
#endif
            thread_error(conn);
            read_thread_close(conn, sf);
            return NULL;
        }

        assert(read_size == hdr_readable.payload_len);

        mpp_set_header_readable(conn->read_buf_hdr_readable + sec_idx, hdr_readable.msg,
                             hdr_readable.payload_len, hdr_readable.seq_num);

        conn->read_buf_first_byte[sec_idx] = 0;

        // only need to signal mpp_sink_read
        pthread_cond_signal(&conn->read_master_cond);

        pthread_mutex_unlock(conn->read_buf_lock + sec_idx);

#ifndef READ_BUF_RAW_PKT
        decrease_active_read_buf_thread(conn);
#endif
    }
}

/*
 * mpp_conn_start should spawn both read threads and write threads
 */
int mpp_conn_start(mpp_conn* conn)
{
    if(!conn) return -1;

    conn->write_thread = (pthread_t*) malloc(conn->sf_num * sizeof(pthread_t));
    if(!conn->write_thread) return -1;

    conn->write_t_args = (thread_args*) malloc(conn->sf_num * sizeof(thread_args));
    if(!conn->write_t_args) return -1;

    conn->read_thread = (pthread_t*) malloc(conn->sf_num * sizeof(pthread_t));
    if(!conn->read_thread) return -1;

    conn->read_t_args = (thread_args*) malloc(conn->sf_num * sizeof(thread_args));
    if(!conn->read_t_args) return -1;

    conn->write_token = (int*) malloc(conn->sf_num * sizeof(int));
    if(!conn->write_token) return -1;

#ifndef READ_BUF_RAW_PKT
    conn->min_read_seq_null = 0;

    conn->read_buf_size = READ_BUF_SEC_NUM;

    conn->read_buf_data_loc = (int*) malloc(READ_BUF_SEC_NUM * sizeof(int));
    if(!conn->read_buf_data_loc) return -1;

    conn->read_buf_lock = (pthread_mutex_t*) malloc(READ_BUF_SEC_NUM * sizeof(pthread_mutex_t));
    if(!conn->read_buf_lock) return -1;

    conn->read_buf_first_byte = (int*) malloc(READ_BUF_SEC_NUM * sizeof(int));
    if(!conn->read_buf_first_byte) return -1;

    conn->read_buf_hdr_readable = \
        (mpp_header_readable*) malloc(READ_BUF_SEC_NUM * sizeof(mpp_header_readable));
    if(!conn->read_buf_hdr_readable) return -1;

    conn->read_buf = (char*) malloc(READ_BUF_SEC_NUM * READ_BUF_SEC_SIZE);
    if(!conn->read_buf) return -1;

    if (pthread_mutex_init(&conn->read_buf_info_lock, NULL) != 0) return -1;
    if (pthread_cond_init(&conn->read_buf_info_cond, NULL) != 0) return -1;

    conn->active_read_buf_thread = 0;
    conn->read_buf_status = 0;
#endif

    if (pthread_cond_init(&conn->write_master_cond, NULL) != 0 ||
        pthread_cond_init(&conn->write_seq_cond, NULL) != 0 ||
        pthread_cond_init(&conn->read_master_cond, NULL) != 0 ||
        pthread_cond_init(&conn->read_seq_cond, NULL) != 0 ||
        pthread_cond_init(&conn->token_cond, NULL) != 0 ||
        pthread_mutex_init(&conn->write_seq_lock, NULL) != 0 ||
        pthread_mutex_init(&conn->read_seq_lock, NULL) != 0 ||
        pthread_mutex_init(&conn->thread_stat_lock, NULL) != 0 ||
        pthread_mutex_init(&conn->token_lock, NULL) != 0) return -1;

    for (int i = 0; i < WRITE_BUF_SEC_NUM; ++i)
    {
        conn->write_buf_pkt_size[i] = 0;
        if (pthread_mutex_init(conn->write_buf_lock + i, NULL) != 0) return -1;
    }

    for (int i = 0; i < READ_BUF_SEC_NUM; ++i)
    {
        conn->read_buf_first_byte[i] = -1;
#ifndef READ_BUF_RAW_PKT
        conn->read_buf_data_loc[i] = i;
#endif
        if (pthread_mutex_init(conn->read_buf_lock + i, NULL) != 0) return -1;
    }

    conn->min_write_seq = 0;
    conn->min_write_seq_finish = 0;
    conn->next_write_seq = 0;
    conn->min_read_seq = 0;
#ifdef READ_BUF_RAW_PKT
    conn->next_read_seq = 0;
#endif
    conn->thread_status = 0;
    conn->active_write_thread_num = conn->sf_num;
    conn->active_read_thread_num = conn->sf_num;

    for (int i = 0; i < conn->sf_num; ++i)
    {
        conn->write_token[i] = 0;

        conn->write_t_args[i].conn = conn;
        conn->write_t_args[i].sf = conn->sf + i;
        conn->write_t_args[i].token = conn->write_token + i;
        conn->read_t_args[i].conn = conn;
        conn->read_t_args[i].sf = conn->sf + i;
        conn->read_t_args[i].token = NULL;
        if (pthread_create(conn->write_thread + i, NULL, write_t,
                           (void*) &conn->write_t_args[i]) != 0 ||
            pthread_create(conn->read_thread + i, NULL, read_t,
                           (void*) &conn->read_t_args[i]) != 0)
            return -1;
    }

    return 0;
}

static int shutdown_read(mpp_conn* conn)
{
    int retval = 0;

    force_read_thread_quit(conn);

    if (conn->read_thread)
    {
        for (int i = 0; i < conn->sf_num; ++i)
        {
            subflow_shutdown(&conn->sf[i], SHUT_RD);
            if (pthread_join(conn->read_thread[i], NULL) != 0) retval = -1;
        }
        free(conn->read_thread);
        conn->read_thread = NULL;
    }
    return retval;
}

static int shutdown_write(mpp_conn* conn)
{
    int retval = 0;

    force_write_thread_quit(conn);

    if (conn->write_thread)
    {
        for (int i = 0; i < conn->sf_num; ++i)
            if (pthread_join(conn->write_thread[i], NULL) != 0) retval = -1;
        free(conn->write_thread);
        conn->write_thread = NULL;
    }

    return retval;
}

int mpp_conn_shutdown(mpp_conn* conn, int how)
{
    int retval = 0;
    switch (how)
    {
        case SHUT_RD:
            return shutdown_read(conn);
        case SHUT_WR:
            return shutdown_write(conn);
        case SHUT_RDWR:
            if (shutdown_write(conn) != 0 || shutdown_read(conn) != 0) retval = -1;
            return retval;
        default:
            return -1;
    }
}

int mpp_conn_stop(mpp_conn* conn)
{
	max_buf_usage = 0;
    int retval = 0;
    // generally there should not have any active read threads
    if (mpp_conn_shutdown(conn, SHUT_RDWR) != 0) retval = -1;

    for (int i = 0; i < conn->sf_num; ++i)
    {
        if (subflow_close(&conn->sf[i]) != 0) retval = -1;
    }

    if (pthread_cond_destroy(&conn->write_master_cond) != 0) retval = -1;
    if (pthread_cond_destroy(&conn->write_seq_cond) != 0) retval = -1;
    if (pthread_cond_destroy(&conn->read_master_cond) != 0) retval = -1;
    if (pthread_cond_destroy(&conn->read_seq_cond) != 0) retval = -1;
    if (pthread_cond_destroy(&conn->token_cond) != 0) retval = -1;
    if (pthread_mutex_destroy(&conn->write_seq_lock) != 0) retval = -1;
    if (pthread_mutex_destroy(&conn->read_seq_lock) != 0) retval = -1;
    if (pthread_mutex_destroy(&conn->thread_stat_lock) != 0) retval = -1;
    if (pthread_mutex_destroy(&conn->token_lock) != 0) retval = -1;

    for (int i = 0; i < WRITE_BUF_SEC_NUM; ++i)
        if (pthread_mutex_destroy(conn->write_buf_lock + i) != 0) retval = -1;

#ifdef READ_BUF_RAW_PKT
    for (int i = 0; i < READ_BUF_SEC_NUM; ++i)
#else
    for (int i = 0; i < conn->read_buf_size; ++i)
#endif
    {
        if (pthread_mutex_destroy(conn->read_buf_lock + i) != 0) retval = -1;
    }

#ifndef READ_BUF_RAW_PKT
    if (pthread_mutex_destroy(&(conn->read_buf_info_lock)) != 0) retval = -1;
    if (pthread_cond_destroy(&(conn->read_buf_info_cond)) != 0) retval = -1;

    free(conn->read_buf_data_loc);
    free(conn->read_buf_lock);
    free(conn->read_buf_first_byte);
    free(conn->read_buf_hdr_readable);
    free(conn->read_buf);
    free(conn->write_token);
#endif

    free(conn->write_t_args);
    free(conn->read_t_args);

    // this is created by mpp_source or mpp_sink, also need to free
    free(conn->sf);

    // in the end free conn itself
    free(conn);

    return retval;
}

int mpp_conn_write(mpp_conn* conn, const char* buf, int len)
{
    if (!buf || len == 0) return -1;

    char* curr_user_buf = (char*) buf;
    int remain_len = len;

    mpp_header hdr;
    mpp_header_readable hdr_readable;
    int payload_len;
    int sec_idx;

    while (remain_len > 0)
    {
        if (check_thread_status(conn) == 1) return -1;

        pthread_mutex_lock(&conn->write_seq_lock);
        while (conn->next_write_seq > conn->min_write_seq_finish + WRITE_BUF_SEC_NUM - 1)
        {
            log_warn("[MPP_CONN_WRITE] no space to write!\n");
            if (check_active_write_thread_num(conn) == 0 && (check_thread_status(conn) & 8) == 0)
            {
                pthread_mutex_unlock(&conn->write_seq_lock);
                return len - remain_len;
            };
            pthread_cond_wait(&conn->write_master_cond, &conn->write_seq_lock);
            if (check_thread_status(conn) == 1)
            {
                pthread_mutex_unlock(&conn->write_seq_lock);
                return -1;
            }
        }
        pthread_mutex_unlock(&conn->write_seq_lock);

        // prepare mpp header

        payload_len = min(remain_len, DEFAULT_MPP_PAYLOAD_LEN);
        mpp_set_header_readable(&hdr_readable, MPP_NORMAL,
                                (unsigned short) payload_len, conn->next_write_seq);

        sec_idx = conn->next_write_seq % WRITE_BUF_SEC_NUM;

        pthread_mutex_lock(conn->write_buf_lock + sec_idx);

        log_debug("[MPP_CONN_WRITE] going to write seq_num %d (sec %d), len %d bytes\n",
                  hdr_readable.seq_num, sec_idx, payload_len);

        // section should not have data
        assert(conn->write_buf_pkt_size[sec_idx] == 0);

        mpp_generate_header(&hdr, &hdr_readable);

        memcpy(conn->write_buf + sec_idx * WRITE_BUF_SEC_SIZE, (char*) &hdr, MPP_HEADER_LEN);
        memcpy(conn->write_buf + sec_idx * WRITE_BUF_SEC_SIZE + MPP_HEADER_LEN, curr_user_buf,
               payload_len);

        // mark the section as readable
        conn->write_buf_pkt_size[sec_idx] = MPP_HEADER_LEN + payload_len;

        // update token
        // pthread_mutex_lock(&conn->token_lock);
        // int min_idx = 0;
        // int min_value = conn->write_token[0];

        // for (int i = 1; i < conn->sf_num; ++i)
        // {
        //     if (conn->write_token[i] < min_value)
        //     {
        //         min_idx = i;
        //         min_value = conn->write_token[i];
        //     }
        // }

        // ++(conn->write_token[min_idx]);
        // pthread_cond_broadcast(&conn->token_cond);
        // pthread_mutex_unlock(&conn->token_lock);

        // update next_write_seq
        pthread_mutex_lock(&conn->write_seq_lock);
        ++(conn->next_write_seq);
        pthread_cond_broadcast(&conn->write_seq_cond);
        pthread_mutex_unlock(&conn->write_seq_lock);

        remain_len -= payload_len;
        curr_user_buf += payload_len;
        pthread_mutex_unlock(conn->write_buf_lock + sec_idx);
    }

    return len - remain_len;
}

int mpp_conn_read(mpp_conn* conn, char* buf, int len)
{
    if (len <= 0) return 0;

    char* curr_user_buf = buf;
    int remain_len = len;

    int sec_remain_len;
    int curr_read_len;
    int sec_idx;

    while (remain_len > 0)
    {
        if (check_thread_status(conn) == 1) return -1;

        // safe to read min_read_seq without lock, since we are the only writer
#ifdef READ_BUF_RAW_PKT
        sec_idx = conn->min_read_seq % READ_BUF_SEC_NUM;
#else
        pthread_mutex_lock(&conn->read_seq_lock);
        sec_idx = conn->read_buf_data_loc[conn->min_read_seq % conn->read_buf_size];
        pthread_mutex_unlock(&conn->read_seq_lock);
        increase_active_read_buf_thread(conn);
#endif
        
        pthread_mutex_lock(conn->read_buf_lock + sec_idx);

        log_debug("[MPP_SINK_READ] going to read sec %d\n", sec_idx);

        // first check whether this section is valid
        while (conn->read_buf_first_byte[sec_idx] == -1)
        {
#ifndef READ_BUF_RAW_PKT
            pthread_mutex_lock(&conn->read_seq_lock);
            conn->min_read_seq_null = 1;
            pthread_mutex_unlock(&conn->read_seq_lock);
#endif
            log_trace("[MPP_SINK_READ] sec %d is invalid\n", sec_idx);

            // section is invalid and nobody's going to write it
            if (check_active_read_thread_num(conn) == 0 && (check_thread_status(conn) & 8) == 0)
            {
                pthread_mutex_unlock(conn->read_buf_lock + sec_idx);
#ifndef READ_BUF_RAW_PKT
                decrease_active_read_buf_thread(conn);
#endif
                return len - remain_len;
            };

            // WARNING: in the current implementation this is not an error
            // but in the future this may be a problem

#ifndef READ_BUF_RAW_PKT            
            // increasing read_buf
            int update_read_buf_flag = 0;
            while (check_read_buf_status(conn) == 1)
            {
                update_read_buf_flag = 1;
                pthread_mutex_unlock(conn->read_buf_lock + sec_idx);

                pthread_mutex_lock(&conn->read_buf_info_lock);
                --(conn->active_read_buf_thread);
                if (conn->active_read_buf_thread == 0) pthread_cond_signal(&conn->read_buf_info_cond);
                pthread_cond_wait(&conn->read_master_cond, &conn->read_buf_info_lock);
                //might be waken up by another read_t (not increasing_read_buf one)
                ++(conn->active_read_buf_thread);
                pthread_mutex_unlock(&conn->read_buf_info_lock);

                pthread_mutex_lock(&conn->read_seq_lock);
                sec_idx = conn->read_buf_data_loc[conn->min_read_seq % conn->read_buf_size];
                pthread_mutex_unlock(&conn->read_seq_lock);

                pthread_mutex_lock(conn->read_buf_lock + sec_idx);
            }
            // the section may be already written, no need to wait
            if (update_read_buf_flag == 1) continue;
#endif

            pthread_cond_wait(&conn->read_master_cond, conn->read_buf_lock + sec_idx);

            if (check_thread_status(conn) == 1)
            {
                pthread_mutex_unlock(conn->read_buf_lock + sec_idx);
#ifndef READ_BUF_RAW_PKT
                decrease_active_read_buf_thread(conn);
#endif
                return -1;
            }

        }
        
        // section is valid, can read now
        sec_remain_len = conn->read_buf_hdr_readable[sec_idx].payload_len - \
                         conn->read_buf_first_byte[sec_idx];
#ifdef READ_BUF_RAW_PKT
        sec_remain_len += MPP_HEADER_LEN;
#endif
        assert(sec_remain_len > 0);

        curr_read_len = min(remain_len, sec_remain_len);

        log_debug("[MPP_SINK_READ] going to read seq_num %d (sec %d), len %d bytes\n",
                  conn->read_buf_hdr_readable[sec_idx].seq_num, sec_idx, curr_read_len);

        memcpy(curr_user_buf,
               conn->read_buf + sec_idx * READ_BUF_SEC_SIZE + conn->read_buf_first_byte[sec_idx],
               curr_read_len);
        
        // update the section 
        if (curr_read_len < sec_remain_len)
        {
            conn->read_buf_first_byte[sec_idx] += curr_read_len;
        } else
        {
            // mark the section as writable
            conn->read_buf_first_byte[sec_idx] = -1;

            // update min_read_seq
            pthread_mutex_lock(&conn->read_seq_lock);
            ++(conn->min_read_seq);
            pthread_cond_broadcast(&conn->read_seq_cond);
            pthread_mutex_unlock(&conn->read_seq_lock);
        }

        remain_len -= curr_read_len;
        curr_user_buf += curr_read_len;
        pthread_mutex_unlock(conn->read_buf_lock + sec_idx);
#ifndef READ_BUF_RAW_PKT
        decrease_active_read_buf_thread(conn);
#endif

    }

    return len - remain_len;
}

int mpp_conn_change_sfs(mpp_conn* conn, subflow* new_sf, int new_sf_num)
{
    log_info("[MPP_UPDATE_TOPO] start to change subflows\n");
    assert((conn->thread_status & 8) > 0);

    // gracefully shutdown the write_t correpsond to old sfs;
    // read_t of the other side will shutdown automatically
    if (mpp_conn_shutdown(conn, SHUT_WR) != 0) return -1;

    log_info("[MPP_UPDATE_TOPO] shutdown old write threads finish\n");
    reset_write_thread_quit_flag(conn);

    assert(check_active_write_thread_num(conn) == 0);
    conn->active_write_thread_num = new_sf_num;
    increase_active_read_thread_num(conn, new_sf_num);

    pthread_t* old_read_thread = conn->read_thread;
    thread_args* old_reat_t_args = conn->read_t_args;

    conn->read_thread = (pthread_t*) malloc(new_sf_num * sizeof(pthread_t));
    conn->read_t_args = (thread_args*) malloc(new_sf_num * sizeof(thread_args));
    conn->write_thread = (pthread_t*) realloc(conn->write_thread, new_sf_num * sizeof(pthread_t));
    conn->write_t_args = (thread_args*) realloc(
        conn->write_t_args, new_sf_num * sizeof(thread_args));

    for (int i = 0; i < new_sf_num; ++i)
    {
        conn->write_t_args[i].conn = conn;
        conn->write_t_args[i].sf = new_sf + i;
        conn->read_t_args[i].conn = conn;
        conn->read_t_args[i].sf = new_sf + i;
        if (pthread_create(conn->write_thread + i, NULL, write_t,
                           (void*) &conn->write_t_args[i]) != 0 ||
            pthread_create(conn->read_thread + i, NULL, read_t,
                           (void*) &conn->read_t_args[i]) != 0)
            return -1;
    }

    log_info("[MPP_UPDATE_TOPO] create new read and write threads finish\n");

    // when the read_t correspond to old src_sf also terminates, update src_sf
    if (old_read_thread)
    {
        for (int i = 0; i < conn->sf_num; ++i)
        {
            if (pthread_join(old_read_thread[i], NULL) != 0) return -1;
            subflow_close(&conn->sf[i]);
        }
        free(old_read_thread);
        free(old_reat_t_args);
    }

    log_info("[MPP_UPDATE_TOPO] close old read threads finish\n");

    conn->sf_num = new_sf_num;
    conn->sf = new_sf;
    change_topo_finish(conn);

    return 0;
}

void mpp_conn_change_topo_start(mpp_conn* conn)
{
    log_info("[MPP_UPDATE_TOPO] update_topo_start\n");
    change_topo_start(conn);
}

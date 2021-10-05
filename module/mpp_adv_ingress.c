#include "mpp_sink.h"
#include "mpp_ctrl_header.h"
#include "mpp_header.h"
#include "mpp_gateway.h"
#include "output_log.h"
#include "subflow.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

static char** listen_ip_addr;
static int* listen_port;
static int listen_num;

static char** backend_ip_addr;
static int* backend_port;
static int* write_ratio;
static int backend_num;

static char* comm_ip_addr;
static int comm_port;

static subflow_server comm_server;

pthread_cond_t switch_status_cond;
pthread_mutex_t switch_status_lock;

// 0: haven't start; 1: listen; 2: started (update_topo_phase2 finish); 3: update_topo_phase1;
// 4: update_topo_phase1 finish; 5: update_topo_phase2; 6: reset;
char switch_status;

static char check_switch_status()
{
    pthread_mutex_lock(&switch_status_lock);
    char retval = switch_status;
    pthread_mutex_unlock(&switch_status_lock);
    return retval;
}

static void check_and_change_switch_status(char exp_old_status, char new_status)
{
    pthread_mutex_lock(&switch_status_lock);
    assert(switch_status == exp_old_status);
    switch_status = new_status;
    pthread_mutex_unlock(&switch_status_lock);
}

static void change_and_wait_switch_status(char exp_old_status, char new_status, char exp_status)
{
    check_and_change_switch_status(exp_old_status, new_status);
    pthread_cond_signal(&switch_status_cond);
    pthread_mutex_lock(&switch_status_lock);
    while (switch_status == new_status)
        pthread_cond_wait(&switch_status_cond, &switch_status_lock);
    assert(switch_status == exp_status);
    pthread_mutex_unlock(&switch_status_lock);
}

void* ingress_thread()
{
    subflow_server server;

    if (server_init(&server, listen_ip_addr[0], listen_port[0]) == -1)
    {
        fprintf(stderr, "server_init error\n");
        server_close(&comm_server);
        exit(1);
    }

    log_info("[MPP_INGRESS] init successfully\n");

    if (server_bind(&server) == -1 || server_listen(&server) == -1)
    {
        fprintf(stderr, "server_bind | server_listen error\n");
        server_close(&server);
        server_close(&comm_server);
        exit(1);
    }

    log_info("[MPP_INGRESS] bind and listen successfully\n");

    check_and_change_switch_status(0, 1);

    while (1)
    {
        subflow* sf = (subflow*) malloc(sizeof(subflow));

        if (!sf || server_accept(sf, &server) == -1)
        {
            fprintf(stderr, "fails to accept subflow\n");
            free(sf);
            server_close(&server);
            server_close(&comm_server);
            exit(1);
        }

        ingress_t_args* args = (ingress_t_args*) malloc(sizeof(ingress_t_args));
        if (!args)
        {
            free(sf);
            server_close(&server);
            server_close(&comm_server);
            exit(1);
        }

        mpp_source* src;
        mpp_conn* conn;
        pthread_t* t;

        if (mpp_ingress_start(sf, (char**) backend_ip_addr, (int*) backend_port,
                              (int*) write_ratio, backend_num, &src, &conn, &t) == -1)
        {
            server_close(&server);
            server_close(&comm_server);
            exit(1);
        }

        log_info("[MPP_INGRESS] start successfully\n");

        check_and_change_switch_status(1, 2);

        while (1)
        {
            char curr_switch_status;
            pthread_mutex_lock(&switch_status_lock);
            while (switch_status == 2 || switch_status == 4)
                pthread_cond_wait(&switch_status_cond, &switch_status_lock);
            curr_switch_status = switch_status;
            pthread_mutex_unlock(&switch_status_lock);

            if (curr_switch_status == 3)
            {
                log_info("[MPP_INGRESS] updating topo phase1 start\n");
                mpp_conn_change_topo_start(conn);

                if (mpp_source_change_remote(src, (const char**) backend_ip_addr,
                                             (const int*) backend_port, backend_num) == -1)
                {
                    subflow_close(sf);
                    free(sf);
                    server_close(&server);
                    server_close(&comm_server);
                    exit(1);
                }

                log_info("[MPP_INGRESS] mpp_source_change_remote successfully\n");

                log_info("[MPP_INGRESS] updating topo phase1 finish\n");
                check_and_change_switch_status(3, 4);
                pthread_cond_signal(&switch_status_cond);
            }
            else if (curr_switch_status == 5)
            {
                log_info("[MPP_INGRESS] updating topo phase2 start\n");

                if (_mpp_source_connect(src) == -1)
                {
                    fprintf(stderr, "_mpp_source_connect error\n");
                    subflow_close(sf);
                    free(sf);
                    server_close(&server);
                    server_close(&comm_server);
                    exit(1);
                }

                log_info("[MPP_INGRESS] _mpp_source_connect successfully\n");
                mpp_conn_change_sfs(conn, src->sf, src->sf_num);
                log_info("[MPP_INGRESS] updating topo phase2 finish\n");
                check_and_change_switch_status(5, 2);
                pthread_cond_signal(&switch_status_cond);
            }
            else if (curr_switch_status == 6)
            {
                log_info("[MPP_INGRESS] reset start\n");
                mpp_ingress_clean(sf, src, conn, t);
                log_info("[MPP_INGRESS] reset finish\n");
                check_and_change_switch_status(6, 1);
                pthread_cond_signal(&switch_status_cond);
                break;
            }
        }
    }

    server_close(&server);
    return NULL;
}

int main(int argc, char* argv[])
{
   if (argc < 9)
    {
        fprintf(stderr, "usage: [program_name] listen_num backend_num listen_ip1 listen_port1 "
                        "listen_write_ratio1... backend_ip1 backend_port1 backend_write_ratio1 "
                        "... comm_ip comm_port\n");
        exit(1);
    }

    listen_num = atoi(argv[1]);
    backend_num = atoi(argv[2]);

    if (listen_num * 3 + backend_num * 3 + 5 != argc)
    {
        fprintf(stderr, "usage: [program_name] listen_num backend_num listen_ip1 listen_port1 "
                        "listen_write_ratio1... backend_ip1 backend_port1 backend_write_ratio1 "
                        "... comm_ip comm_port\n");
        exit(1);
    }

    comm_ip_addr = argv[argc - 2];
    comm_port = atoi(argv[argc - 1]);

    listen_ip_addr = (char**) malloc(listen_num * sizeof(char*));
    listen_port = (int*) malloc(listen_num * sizeof(int));
    for (int i = 0; i < listen_num; ++i)
    {
        listen_ip_addr[i] = argv[3 + 3 * i];
        listen_port[i] = atoi(argv[3 + 3 * i + 1]);
    }


    backend_ip_addr = (char**) malloc(backend_num * sizeof(char*));
    backend_port = (int*) malloc(backend_num * sizeof(int));
    write_ratio = (int*) malloc(backend_num * sizeof(int));
    for (int i = 0; i < backend_num; ++i)
    {
        backend_ip_addr[i] = argv[3 + 3 * listen_num + 3 * i];
        backend_port[i] = atoi(argv[3 + 3 * listen_num + 3 * i + 1]);
        write_ratio[i] = atoi(argv[3 + 3 * listen_num + 3 * i + 2]);
    }

    if (server_init(&comm_server, comm_ip_addr, comm_port) == -1)
    {
        fprintf(stderr, "comm server_init error\n");
        exit(1);
    }

    log_info("[MPP_INGRESS] comm_server init successfully\n");

    if (server_bind(&comm_server) == -1 || server_listen(&comm_server) == -1)
    {
        fprintf(stderr, "comm server_bind | comm server_listen error\n");
        server_close(&comm_server);
        exit(1);
    }

    log_info("[MPP_INGRESS] comm_server bind and listen successfully\n");

    switch_status = 0;

    pthread_t t;
    pthread_create(&t, NULL, ingress_thread, NULL);
    pthread_detach(t);

    while (1)
    {
        subflow* sf = (subflow*) malloc(sizeof(subflow));

        if (!sf || server_accept(sf, &comm_server) == -1)
        {
            fprintf(stderr, "fails to accept subflow\n");
            free(sf);
            server_close(&comm_server);
            exit(1);
        }

        char msg[MPP_CTRL_MSG_LEN];
        char re_msg[MPP_CTRL_MSG_LEN];
        int read_bytes;

        re_msg[0] = mpp_generate_msg(MPP_RE_SUCCESS);

        while (1)
        {
            read_bytes = subflow_read(sf, msg, MPP_CTRL_MSG_LEN);
            if (read_bytes == 0)
            {
                subflow_close(sf);
                break;
            }

            if (read_bytes < 0)
            {
                subflow_close(sf);
                goto close;
            }
            
            assert(read_bytes == MPP_CTRL_MSG_LEN);
            mpp_ctrl_msg ctrl_msg = mpp_parse_msg(msg[0]);

             if (ctrl_msg == MPP_CTRL_HEALTH_CHECK)
            {
                if(check_switch_status() == 0)
                    re_msg[0] = mpp_generate_msg(MPP_RE_FAIL);
            }
            else if (ctrl_msg == MPP_CTRL_UPDATE_TOPO_PRE)
            {
                char sf_num_char;

                read_bytes = subflow_read(sf, (char*) &sf_num_char, 1);

                if (read_bytes < 0)
                {
                    subflow_close(sf);
                    goto close;
                }

                int old_backend_num = backend_num;

                backend_num = mpp_char_to_int(sf_num_char);
                assert(backend_num >= 0);

                backend_ip_addr = (char**) realloc(backend_ip_addr, backend_num * sizeof(char*));
                backend_port = (int*) realloc(backend_port, backend_num * sizeof(int));

                for (int i = old_backend_num; i < backend_num; ++i)
                {
                    backend_ip_addr[i] = (char*) malloc(INET_ADDRSTRLEN * sizeof(char));
                }

                for (int i = 0; i < backend_num; ++i)
                {
                    mpp_addr addr;
                    read_bytes = subflow_read(sf, (char*) &addr, MPP_ADDR_LEN);

                    if (read_bytes < 0)
                    {
                        subflow_close(sf);
                        goto close;
                    }

                    assert(read_bytes == MPP_ADDR_LEN);

                    mpp_parse_ip(backend_ip_addr[i], addr.ip);
                    backend_port[i] = mpp_parse_port(addr.port);
                }

                change_and_wait_switch_status(2, 3, 4);
            }
            else if (ctrl_msg == MPP_CTRL_UPDATE_TOPO)
            {
                change_and_wait_switch_status(4, 5, 2);
            }
            else if (ctrl_msg == MPP_CTRL_RESET)
            {
                change_and_wait_switch_status(2, 6, 1);
            }
            else if (ctrl_msg == MPP_CTRL_CLOSE)
            {
                subflow_close(sf);
                goto close;
            }
            else continue;

            if (subflow_write(sf, re_msg, MPP_CTRL_MSG_LEN) == MPP_CTRL_MSG_LEN)
            {
                log_info("[MPP_INGRESS] send response to controller\n");
            } else
            {
                log_warn("[MPP_INGRESS] fail to send response to controller\n");
                subflow_close(sf);
                goto close;
            }

        }
    }

close:
    log_info("[MPP_INGRESS] closing ...\n");
    server_close(&comm_server);
    return 0;
}

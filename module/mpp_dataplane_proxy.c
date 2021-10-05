#include "mpp_proxy.h"
#include "mpp_ctrl_header.h"
#include "mpp_header.h"
#include "output_log.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

static char** listen_ip_addr;
static int* listen_port;
static int* listen_write_ratio;
static int listen_num;

static char** backend_ip_addr;
static int* backend_port;
static int* backend_write_ratio;
static int backend_num;

static char* comm_ip_addr;
static int comm_port;

char switch_status;

void* dataplane_proxy_thread()
{
    mpp_proxy proxy;

    if (mpp_proxy_init(&proxy, (const char**) listen_ip_addr, (const int*) listen_port,
                       listen_num, (const char**) backend_ip_addr, (const int*) backend_port,
                       backend_num) == -1)
    {
        fprintf(stderr, "mpp_proxy_init error\n");
        mpp_proxy_clean(&proxy);
        return NULL;
    }

    log_info("[MPP_PROXY] init successfully\n");

    if (mpp_proxy_bind(&proxy) == -1 || mpp_proxy_listen(&proxy) == -1)
    {
        fprintf(stderr, "mpp_proxy_bind | mpp_proxy_listen error\n");
        mpp_proxy_clean(&proxy);
        return NULL;
    }

    log_info("[MPP_PROXY] bind and listen successfully\n");

    switch_status = 1;

     while (1)
     {
        subflow* sink_sf;
        if ((sink_sf = mpp_proxy_accept(&proxy)) == NULL)
        {
            fprintf(stderr, "mpp_proxy_accept error\n");
            mpp_proxy_clean(&proxy);
            return NULL;
        }

        log_info("[MPP_PROXY] receive successfully\n");

        for(int i = 0; i < proxy.sink.sf_num; ++i)
            log_info("[MPP_PROXY] %d-th accept: %s:%i\n", i, sink_sf[i].remote_ip,
                     sink_sf[i].remote_port);

        proxy_t_args* args = (proxy_t_args*) malloc(sizeof(proxy_t_args));
        if (!args)
        {
            for (int i = 0; i < proxy.sink.sf_num; ++i)
            {
                subflow_close(&sink_sf[i]);
            }
            free(sink_sf);
            mpp_proxy_clean(&proxy);
            return NULL;
        }

        args->sink_sf_num = proxy.sink.sf_num;
        args->sink_sf = sink_sf;
        args->listen_write_ratio = (int*) listen_write_ratio;
        args->backend_write_ratio = (int*) backend_write_ratio;

        mpp_proxy_thread(args);

        log_info("[MPP_PROXY] run successfully\n");
     }

    mpp_proxy_clean(&proxy);
    return NULL;
}

int main(int argc, char* argv[])
{
    if (argc < 9)
    {
        fprintf(stderr, "usage: [program_name] listen_num backend_num listen_ip1 listen_port1 "
                        "listen_write_ratio1 ... backend_ip1 backend_port1 backend_write_ratio1 "
                        "... comm_ip comm_port\n");
        exit(1);
    }

    listen_num = atoi(argv[1]);
    backend_num = atoi(argv[2]);

    if ((listen_num + backend_num) * 3 + 5 != argc)
    {
        fprintf(stderr, "usage: [program_name] listen_num backend_num listen_ip1 listen_port1 "
                        "listen_write_ratio1 ... backend_ip1 backend_port1 backend_write_ratio1 "
                        "... comm_ip comm_port\n");
        exit(1);
    }

    comm_ip_addr = argv[argc - 2];
    comm_port = atoi(argv[argc - 1]);

    listen_ip_addr = (char**) malloc(listen_num * sizeof(char*));
    listen_port = (int*) malloc(listen_num * sizeof(int));
    listen_write_ratio = (int*) malloc(listen_num * sizeof(int));
    for (int i = 0; i < listen_num; ++i)
    {
        listen_ip_addr[i] = argv[3 + 3 * i];
        listen_port[i] = atoi(argv[3 + 3 * i + 1]);
        listen_write_ratio[i] = atoi(argv[3 + 3 * i + 2]);
    }


    backend_ip_addr = (char**) malloc(backend_num * sizeof(char*));
    backend_port = (int*) malloc(backend_num * sizeof(int));
    backend_write_ratio = (int*) malloc(backend_num * sizeof(int));
    for (int i = 0; i < backend_num; ++i)
    {
        backend_ip_addr[i] = argv[3 + 3 * listen_num + 3 * i];
        backend_port[i] = atoi(argv[3 + 3 * listen_num + 3 * i + 1]);
        backend_write_ratio[i] = atoi(argv[3 + 3 * listen_num + 3 * i + 2]);
    }

    switch_status = 0;

    pthread_t t;
    pthread_create(&t, NULL, dataplane_proxy_thread, NULL);
    pthread_detach(t);

    subflow_server comm_server;

    if (server_init(&comm_server, comm_ip_addr, comm_port) == -1)
    {
        fprintf(stderr, "comm server_init error\n");
        exit(1);
    }

    log_info("[MPP_PROXY] comm_server init successfully\n");

    if (server_bind(&comm_server) == -1 || server_listen(&comm_server) == -1)
    {
        fprintf(stderr, "comm server_bind | comm server_listen error\n");
        server_close(&comm_server);
        exit(1);
    }

    log_info("[MPP_PROXY] comm_server bind and listen successfully\n");

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
        int read_bytes;

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
                char re_msg[MPP_CTRL_MSG_LEN];
                if(switch_status == 0)
                    re_msg[0] = mpp_generate_msg(MPP_RE_FAIL);
                else re_msg[0] = mpp_generate_msg(MPP_RE_SUCCESS);
                if (subflow_write(sf, re_msg, MPP_CTRL_MSG_LEN) == MPP_CTRL_MSG_LEN)
                {
                    log_info("[MPP_EGRESS_SWITCH] send response to controller\n");
                } else
                {
                    log_warn("[MPP_EGRESS_SWITCH] fail to send response to controller\n");
                    subflow_close(sf);
                    goto close;
                }
            }
            else if (ctrl_msg == MPP_CTRL_CLOSE)
            {
                subflow_close(sf);
                goto close;
            }
        }
    }

close:
    log_info("[MPP_PROXY] closing ...\n");
    server_close(&comm_server);
    return 0;
}

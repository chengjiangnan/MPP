#include "mpp_sink.h"
#include "mpp_conn.h"
#include "subflow.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

int mpp_sink_init(mpp_sink* sink, const char* listen_ip[], const int listen_port[], int sf_num)
{
    sink->sf_num = sf_num;

    sink->sf_server = (subflow_server*) malloc(sf_num * sizeof(subflow_server));
    if (!sink->sf_server) return -1;

    for (int i = 0; i < sf_num; ++i)
    {
        if (server_init(&(sink->sf_server[i]), listen_ip[i], listen_port[i]) == -1)
        {
            return -1;
        }
    }

    return 0;
}

int mpp_sink_change_listen(mpp_sink* sink, const char* new_listen_ip[],
                           const int new_listen_port[], int new_sf_num)
{
    mpp_sink_clean(sink);
    return mpp_sink_init(sink, new_listen_ip, new_listen_port, new_sf_num);
}

int mpp_sink_bind(mpp_sink* sink)
{
    for (int i = 0; i < sink->sf_num; ++i)
    {
        if(server_bind(&sink->sf_server[i]) == -1) return -1;
    }
    return 0;
}

int mpp_sink_listen(mpp_sink* sink)
{
    for (int i = 0; i < sink->sf_num; ++i)
    {
        if(server_listen(&sink->sf_server[i]) == -1) return -1;
    }
    return 0;
}

typedef struct server_accept_args
{
    subflow_server* sf_server;
    subflow* sf;
    int retval;
} server_accept_args;

static void* server_accept_t(void* _args)
{
    server_accept_args* args = (server_accept_args*) _args;
    if (server_accept(args->sf, args->sf_server) == -1)
        args->retval = -1;
    else
        args->retval = 0;
    return NULL;
}

subflow* _mpp_sink_accept(mpp_sink* sink)
{
    subflow* sf = (subflow*) malloc(sink->sf_num * sizeof(subflow));
    if (!sf) return NULL;

    pthread_t thread[sink->sf_num];
    server_accept_args args[sink->sf_num];

    int error = 0;

    for (int i = 0; i < sink->sf_num; ++i)
    {
        args[i].sf_server = &sink->sf_server[i];
        args[i].sf = &sf[i];
        if(pthread_create(&thread[i], NULL, server_accept_t, (void*) &args[i]) != 0) error = -1;
    }

    for (int i = 0; i < sink->sf_num; ++i)
    {
        if(pthread_join(thread[i], NULL) != 0 || args[i].retval == -1) error = -1;
    }

    if (error == -1)
    {
        free(sf);
        return NULL;
    }

    return sf;
}

mpp_conn* mpp_sink_accept(mpp_sink* sink)
{
    
    subflow* sf = _mpp_sink_accept(sink);
    if (!sf) return NULL;

    mpp_conn* conn = (mpp_conn*) malloc(sizeof(mpp_conn));
    if (!conn)
    {
        free(sf);
        return NULL;
    }

    conn->sf_num = sink->sf_num;
    conn->sf = sf;

    return conn;
}

void mpp_sink_clean(mpp_sink* sink)
{
    for (int i = 0; i < sink->sf_num; ++i)
    {
        server_close(&sink->sf_server[i]);
    }
    free(sink->sf_server);
}

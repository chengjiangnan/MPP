#include "mpp_source.h"
#include "mpp_conn.h"
#include "subflow.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

int mpp_source_init(mpp_source* src, const char* remote_ip[], const int remote_port[], int sf_num)
{
    src->sf_num = sf_num;

    src->sf = (subflow*) malloc(sf_num * sizeof(subflow));
    if (!src->sf) return -1;

    for (int i = 0; i < sf_num; ++i)
    {
        if (client_init(&(src->sf[i]), remote_ip[i], remote_port[i]) == -1)
        {
            return -1;
        }
    }

    return 0;
}

int mpp_source_change_remote(mpp_source* src, const char* new_remote_ip[],
                             const int new_remote_port[], int new_sf_num)
{
    // shouldn't call mpp_source_clean because mpp_conn are using these subflows
    return mpp_source_init(src, new_remote_ip, new_remote_port, new_sf_num);
}

typedef struct client_args
{
    subflow* sf;
    int retval;
} client_args;

static void* client_connect_t(void* _args)
{
    client_args* args = (client_args*) _args;
    args->retval = client_connect(args->sf);
    return NULL;
}

int _mpp_source_connect(const mpp_source* src)
{
    pthread_t thread[src->sf_num];
    client_args args[src->sf_num];

    for (int i = 0; i < src->sf_num; ++i)
    {
        args[i].sf = &(src->sf[i]);
        if(pthread_create(&thread[i], NULL, client_connect_t, (void*) &args[i]) != 0) return -1;
    }

    for (int i = 0; i < src->sf_num; ++i)
    {
        if(pthread_join(thread[i], NULL) != 0 || args[i].retval == -1) return -1;
    }

    return 0;
}

mpp_conn* mpp_source_connect(mpp_source* src)
{
    if (_mpp_source_connect(src) == -1) return NULL;

    mpp_conn* conn = (mpp_conn*) malloc(sizeof(mpp_conn));
    if (!conn) return NULL;

    conn->sf_num = src->sf_num;
    conn->sf = src->sf;

    src->sf = NULL;

    return conn;
}

void mpp_source_clean(mpp_source* src)
{
    if (src->sf) free(src->sf);
}

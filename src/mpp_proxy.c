#include "mpp_proxy.h"
#include "mpp_proxy_conn.h"
#include "mpp_source.h"
#include "mpp_sink.h"
#include "output_log.h"
#include "subflow.h"

#include <stdlib.h>

// TODO: put it in proxy data structure
static int source_sf_num;
static int* source_port;
static char** source_ip;

int mpp_proxy_init(mpp_proxy* proxy,
                   const char* listen_ip[], const int listen_port[], int listen_sf_num,
                   const char* backend_ip[], const int backend_port[], int backend_sf_num)
{
    if (mpp_sink_init(&(proxy->sink), listen_ip, listen_port, listen_sf_num) == -1)
        return -1;
    
    source_sf_num = backend_sf_num;
    source_ip = (char**) backend_ip;
    source_port = (int*) backend_port;
    return 0;
}

int mpp_proxy_bind(mpp_proxy* proxy)
{
    return mpp_sink_bind(&(proxy->sink));
}

int mpp_proxy_listen(mpp_proxy* proxy)
{
    return mpp_sink_listen(&(proxy->sink));
}

subflow* mpp_proxy_accept(mpp_proxy* proxy)
{
    return  _mpp_sink_accept(&proxy->sink);
}

static int mpp_proxy_connect(int* src_sf_num, subflow** src_sf)
{
    mpp_source src;
    if (mpp_source_init(&src, (const char**) source_ip, (const int*) source_port,
                        source_sf_num) == -1 ||
        _mpp_source_connect(&src) == -1)
    {
        mpp_source_clean(&src);
        return -1;
    }

    *src_sf_num = src.sf_num;
    *src_sf = src.sf;

    src.sf = NULL;
    mpp_source_clean(&src);

    return 0;
}

void* mpp_proxy_thread(void* _args)
{
    proxy_t_args* args = (proxy_t_args*) _args;
    int sink_sf_num = args->sink_sf_num;
    subflow* sink_sf = args->sink_sf;
    int* listen_write_ratio = args->listen_write_ratio;
    int* backend_write_ratio = args->backend_write_ratio;
    free(args);

    int src_sf_num;
    subflow* src_sf;

    if (mpp_proxy_connect(&src_sf_num, &src_sf) == -1) return NULL;

    mpp_proxy_conn* conn = (mpp_proxy_conn*) malloc(sizeof(mpp_proxy_conn));
    if (!conn)
    {
        free(src_sf);
        return NULL;
    } 

    conn->sink_sf_num = sink_sf_num;
    conn->sink_sf = sink_sf;
    conn->src_sf_num = src_sf_num;
    conn->src_sf = src_sf;
 
    if (mpp_proxy_conn_init(conn) == -1) goto conn_close;

    mpp_proxy_conn_run(conn);

conn_close:
    mpp_proxy_conn_clean(conn);
    
    log_info("[MPP_PROXY_THREAD] finish successfully\n");
    return NULL;
}

void mpp_proxy_clean(mpp_proxy* proxy)
{
    mpp_sink_clean(&(proxy->sink));
}

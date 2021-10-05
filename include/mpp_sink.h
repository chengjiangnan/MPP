#ifndef __MPP_SINK_H__
#define __MPP_SINK_H__ 1

#include "mpp_conn.h"
#include "subflow.h"

typedef struct mpp_sink
{
    int sf_num;
    subflow_server* sf_server;
} mpp_sink;

/*
 * initialize a mpp sink with multiple listen (ip address, port number) pairs
 * return 0 if success; -1 otherwise
 */
int mpp_sink_init(mpp_sink* sink, const char* listen_ip[], const int listen_port[], int sf_num);

/*
 * change sink's listen pairs
 * return 0 if success; -1 otherwise
 */
int mpp_sink_change_listen(mpp_sink* sink, const char* new_listen_ip[],
                           const int new_listen_port[], int new_sf_num);

/*
 * let mpp_sink bind to sockets
 * return 0 if success; -1 otherwise
 */
int mpp_sink_bind(mpp_sink* sink);

/*
 * let mpp_sink listen on sockets
 * return 0 if success; -1 otherwise
 */
int mpp_sink_listen(mpp_sink* sink);

/*
 * accept subflows from the remote sides
 * return a mpp_conn pointer if success; NULL otherwise
 */
mpp_conn* mpp_sink_accept(mpp_sink* sink);

/*
 * clean mpp_sink
 * only call this when all the mpp_conn are closed 
 */
void mpp_sink_clean(mpp_sink* sink);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Below are utility functions; user shouldn't call these
///////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * accept subflows from the remote sides
 * return those subflows if success; NULL otherwise
 */
subflow* _mpp_sink_accept(mpp_sink* sink);

#endif /*__MPP_SINK_H__*/

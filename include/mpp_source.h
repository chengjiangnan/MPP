#ifndef __MPP_SOURCE_H__
#define __MPP_SOURCE_H__ 1

#include "mpp_conn.h"
#include "subflow.h"

typedef struct mpp_source
{
    int sf_num;
    subflow* sf;
} mpp_source;

/*
 * initialize a mpp source with multiple remote (ip address, port number) pairs
 * return 0 if success; -1 otherwise
 */
int mpp_source_init(mpp_source* src, const char* remote_ip[], const int remote_port[], int sf_num);

/*
 * change source's remote pairs
 * return 0 if success; -1 otherwise
 */
int mpp_source_change_remote(mpp_source* src, const char* new_remote_ip[],
                             const int new_remote_port[], int new_sf_num);

/*
 * connect mpp source to the remote sides
 * return a mpp_conn pointer if success; NULL otherwise
 */
mpp_conn* mpp_source_connect(mpp_source* src);

/*
 * clean mpp_source
 */
void mpp_source_clean(mpp_source* src);

///////////////////////////////////////////////////////////////////////////////////////////////////
// Below are utility functions; user shouldn't call these
///////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * connect mpp source to the remote sides
 * return 0 if success; -1 otherwise
 */
int _mpp_source_connect(const mpp_source* src);

#endif /*__MPP_SOURCE_H__*/

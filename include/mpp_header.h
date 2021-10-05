#ifndef __MPP_HEADER_H__
#define __MPP_HEADER_H__ 1

/* mpp message */
typedef enum { MPP_NORMAL = 1 } mpp_msg;

typedef struct mpp_header
{
    char ctrl_msg;
    char payload_len[2];
    char seq_num[4];
} mpp_header;

typedef struct mpp_header_readable
{
    mpp_msg msg;
    unsigned short payload_len;
    unsigned int seq_num;
} mpp_header_readable;

#define MPP_HEADER_LEN sizeof(mpp_header)
#define TCP_MSS 9000
#define DEFAULT_MPP_TOTAL_LEN TCP_MSS
#define DEFAULT_MPP_PAYLOAD_LEN (DEFAULT_MPP_TOTAL_LEN - MPP_HEADER_LEN)

/*
 * set mpp_header_readable with values 
 */
void mpp_set_header_readable(mpp_header_readable* hdr_readable, mpp_msg msg,
                             unsigned short payload_len, unsigned int seq_num);

/*
 * generate mpp_header based on mpp_header_readable
 */
void mpp_generate_header(mpp_header* hdr, const mpp_header_readable* hdr_readable);

/*
 * parse header into mpp_header_readable
 */
void mpp_parse_header(const mpp_header* hdr, mpp_header_readable* hdr_readable);

#endif /*__MPP_HEADER_H__*/

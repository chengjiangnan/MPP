#include "mpp_header.h"

#include <stdint.h>
#include <string.h>

void pack_unsigned_int(char* buf, unsigned int val)
{
    unsigned char* ubuf = (unsigned char *) buf;
    ubuf[0] = (val>>24) & 0xff;
    ubuf[1] = (val>>16) & 0xff;
    ubuf[2] = (val>>8) & 0xff;
    ubuf[3] = val & 0xff;
}

unsigned int unpack_unsigned_int(const char *buf)
{
    const uint8_t* ubuf = (const uint8_t*) buf;
    return (unsigned int) (ubuf[0]<<24) | (ubuf[1]<<16) | (ubuf[2]<<8) | ubuf[3];
}

void pack_unsigned_short(char* buf, unsigned short val)
{
    unsigned char* ubuf = (unsigned char *) buf;
    ubuf[0] = (val>>8) & 0xff;
    ubuf[1] = val & 0xff;
}

unsigned short unpack_unsigned_short(const char* buf)
{
    unsigned char* ubuf = (unsigned char *) buf;
    return (unsigned short) (ubuf[0]<<8) | ubuf[1];
}

void pack_mpp_msg(char* buf, mpp_msg msg)
{
    *buf = msg + '0';
}

mpp_msg unpack_mpp_msg(char buf)
{
    return (mpp_msg) buf - '0';
}

void mpp_set_header_readable(mpp_header_readable* hdr_readable, mpp_msg msg,
                             unsigned short payload_len, unsigned int seq_num)
{
    hdr_readable->msg = msg;
    hdr_readable->payload_len = payload_len;
    hdr_readable->seq_num = seq_num;
}

void mpp_generate_header(mpp_header* hdr, const mpp_header_readable* hdr_readable)
{
    pack_mpp_msg(&(hdr->ctrl_msg), hdr_readable->msg);
    pack_unsigned_short(hdr->payload_len, hdr_readable->payload_len);
    pack_unsigned_int(hdr->seq_num, hdr_readable->seq_num);
}

void mpp_parse_header(const mpp_header* hdr, mpp_header_readable* hdr_readable)
{
    hdr_readable->msg = unpack_mpp_msg(hdr->ctrl_msg);
    hdr_readable->payload_len = unpack_unsigned_short(hdr->payload_len);
    hdr_readable->seq_num = unpack_unsigned_int(hdr->seq_num);   
}

#include "mpp_header.h"

#include <stdio.h>

int main(int argc, char* argv[])
{
    mpp_header hdr;

    mpp_header_readable hdr_readable;

    mpp_set_header_readable(&hdr_readable, MPP_NORMAL, DEFAULT_MPP_PAYLOAD_LEN, 10);

    printf("[GENERATE_HEADER] msg: %d, payload_len: %hu, seq_num: %u\n",
           hdr_readable.msg, hdr_readable.payload_len, hdr_readable.seq_num);
    mpp_generate_header(&hdr, &hdr_readable);

    mpp_header_readable hdr_readable2;

    mpp_parse_header(&hdr, &hdr_readable2);
    printf("[PARSE_HEADER] msg: %d, payload_len: %hu, seq_num: %u\n",
           hdr_readable2.msg, hdr_readable2.payload_len, hdr_readable2.seq_num);

    return 0;
}

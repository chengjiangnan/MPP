#include "mpp_header.h"
#include "output_log.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <sys/sendfile.h>

static char* filename;

int read_fd, write_fd;

typedef struct seq_info
{
    off_t payload_offset;
    unsigned short payload_len;
} seq_info;

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: [program_name] filename\n");
        exit(1);
    }

    filename = argv[1];

    char output_filename[100];
    strcpy(output_filename, "decode_");
    strcat(output_filename, filename);

    log_info("[MPP_DECODER] going to decode %s\n", filename);

    read_fd = open(filename, O_RDONLY);
    write_fd = open(output_filename, O_WRONLY | O_CREAT, 0666);

    off_t curr_offset = 0; // current offset we are working on

    unsigned int max_seq_num = 0; // the max seq num of this file

    unsigned int curr_size = 1;
    seq_info* seq_infos = (seq_info*) malloc(curr_size * sizeof(seq_info));
    if (seq_infos == NULL)
    {
        log_warn("[MPP_DECODER] no enough memory for malloc\n");
        return -1;
    }
    
    while (1)
    {
        mpp_header hdr;
        ssize_t hdr_size = pread(read_fd, (void*) &hdr, MPP_HEADER_LEN, curr_offset);

        if (hdr_size < 0)
        {
            fprintf(stderr, "pread error\n");
            exit(1);
        }

        if (hdr_size != MPP_HEADER_LEN)
        {
            assert(hdr_size == 0);
            log_info("[MPP_DECODER] reach EOF\n");

            if(curr_offset == 0)
            {
                log_warn("[MPP_DECODER] no data!\n");
                free(seq_infos);
                return -1;
            }

            break;
        }

        // parse header
        mpp_header_readable hdr_readable;
        mpp_parse_header(&hdr, &hdr_readable);

        log_debug("[MPP_DECODER] msg: %d, payload_len: %u, seq_num: %u\n",
                  hdr_readable.msg, hdr_readable.payload_len, hdr_readable.seq_num);

        // update max_seq_num if necessary
        if (hdr_readable.seq_num > max_seq_num) max_seq_num = hdr_readable.seq_num;

        while (curr_size < hdr_readable.seq_num + 1)
        {
            curr_size *= 2;
            seq_info* ptr = (seq_info*) realloc(seq_infos, curr_size * sizeof(seq_info));
            if (ptr == NULL)
            {
                log_warn("[MPP_DECODER] no enough memory for realloc\n");
                free(seq_infos);
                return -1;
            }
            seq_infos = ptr;
        }

        log_debug("curr_size: %d, seq_num %u\n", curr_size, hdr_readable.seq_num);

        seq_infos[hdr_readable.seq_num].payload_offset = curr_offset + MPP_HEADER_LEN;
        seq_infos[hdr_readable.seq_num].payload_len = hdr_readable.payload_len;

        curr_offset += MPP_HEADER_LEN + hdr_readable.payload_len;
    }

    log_info("[MPP_DECODER] max_seq_num: %d\n", max_seq_num);

    log_info("[MPP_DECODER] start decoding\n");

    for (int i = 0; i <= max_seq_num; ++i)
    {
        log_info("[MPP_DECODER] copying packet %d\n", i);
        sendfile(write_fd, read_fd, &(seq_infos[i].payload_offset),
                 (size_t) seq_infos[i].payload_len);
    }

    log_info("[MPP_DECODER] finish\n");
   
    close(read_fd);
    close(write_fd);

    return 0;
}

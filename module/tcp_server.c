#include "subflow.h"
#include "output_log.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static char* listen_ip_addr;
static int listen_port;

#define WRITE_NAME "sample.pdf"
#define READ_NAME "file_copy.txt"
#define BUF_SIZE 50000

subflow sf;

FILE* write_fp;
FILE* read_fp;

void* write_t(void* args)
{
    char buf[BUF_SIZE];
    while (1)
    {
        int curr_size = 0;
        if ((curr_size = fread(buf, 1, BUF_SIZE, write_fp)) == 0) break;

        log_info("[WRITE_T] going to write %d bytes\n", curr_size);

        if (subflow_write(&sf, buf, curr_size) == -1)
        {
            fprintf(stderr, "subflow_write error\n");
            return NULL;
        }
    }

    subflow_shutdown(&sf, SHUT_WR);
    return NULL;
}

void* read_t(void* args)
{
    char buf[BUF_SIZE];
    int curr_size;
    while (1)
    {
        log_info("[READ_T] going to read %d bytes\n", (int) BUF_SIZE);
        if ((curr_size = subflow_read(&sf, buf, BUF_SIZE)) == -1)
        {
            fprintf(stderr, "subflow_read error\n");
            return NULL;
        }

        log_info("[READ_T] read %d bytes\n", curr_size);
        fwrite(buf, 1, curr_size, read_fp);
        if (curr_size < BUF_SIZE) break;
    }

    subflow_shutdown(&sf, SHUT_RD);
    return NULL;
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: [program_name] listen_ip listen_port\n");
        exit(1);
    }

    listen_ip_addr = argv[1];
    listen_port = atoi(argv[2]);

    subflow_server server; 
    if (server_init(&server, listen_ip_addr, listen_port) == -1)
    {
        fprintf(stderr, "server_init error\n");
        exit(1);
    };

    log_info("[SERVER] init successfully\n");

    if (server_bind(&server) == -1 || server_listen(&server) == -1)
    {
        fprintf(stderr, "server bind | listen error\n");
        server_close(&server);
        exit(1);
    }

    log_info("[SERVER] bind and listen successfully\n");


    if (server_accept(&sf, &server) == -1)
    {
        fprintf(stderr, "server_accept error\n");
        server_close(&server);
        exit(1);
    }

    write_fp = fopen(WRITE_NAME, "r");
    read_fp = fopen(READ_NAME, "w");

    pthread_t write_thread;
    pthread_create(&write_thread, NULL, write_t, NULL);
    pthread_t read_thread;
    pthread_create(&read_thread, NULL, read_t, NULL);

    pthread_join(write_thread, NULL);
    pthread_join(read_thread, NULL);

    log_info("[SERVER] write successfully, going to quit\n");
    
    fclose(write_fp);
    fclose(read_fp);
    subflow_close(&sf);
    server_close(&server);

    return 0;
}

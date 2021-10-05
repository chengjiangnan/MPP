#include "subflow.h"
#include "output_log.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static char* backend_ip_addr;
static int backend_port;

#define WRITE_NAME "file.txt"
#define READ_NAME "sample_copy.pdf"
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
        fprintf(stderr, "usage: [program_name] backend_ip backend_port\n");
        exit(1);
    }

    backend_ip_addr = argv[1];
    backend_port = atoi(argv[2]);

    if (client_init(&sf, backend_ip_addr, backend_port) == -1)
    {
        perror("client_init error");
        exit(1);
    };

    if(client_connect(&sf) == -1)
    {
        perror("client_connect error");
        exit(1);
    }

    log_info("[CLIENT] connect to %s:%d\n", backend_ip_addr, backend_port);

    write_fp = fopen(WRITE_NAME, "r");
    read_fp = fopen(READ_NAME, "w");

    pthread_t write_thread;
    pthread_create(&write_thread, NULL, write_t, NULL);
    pthread_t read_thread;
    pthread_create(&read_thread, NULL, read_t, NULL);

    pthread_join(write_thread, NULL);
    pthread_join(read_thread, NULL);

    log_info("[MPP_CLIENT] finish successfully, going to quit\n");


    fclose(write_fp);
    fclose(read_fp);
    subflow_close(&sf);

    return 0;
}

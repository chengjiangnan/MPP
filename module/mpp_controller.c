#include "subflow.h"
#include "mpp_ctrl_header.h"
#include "output_log.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "usage: [program_name] remote_ip remote_port operation ...\n");
        exit(1);
    }

    char* remote_ip_addr = argv[1];
    int remote_port = atoi(argv[2]);
    char* operation = argv[3];

    // monitor operations are a little different
    if (strcmp(operation, "stat") == 0 || strcmp(operation, "stop") == 0)
    {
        subflow sf;

        if (client_init(&sf, remote_ip_addr, remote_port) == -1)
        {
            perror("client_init");
            exit(1);
        };

        if(client_connect(&sf) == -1)
        {
            perror("client_connect");
            exit(1);
        }

        int retval = 0;

        if (subflow_write(&sf, operation, 4) == 4)
        {
            log_info("[CONTROLLER] send operation %s to %s:%d\n",
                     operation, remote_ip_addr, remote_port);
            if (strcmp(operation, "stop") == 0) goto monitor_close;
        } else
        {
            log_warn("[CONTROLLER] fail to send operation %s to %s:%d\n",
                     operation, remote_ip_addr, remote_port);
            retval = 1;
            goto close;
        }

        char stat[100];
        if (subflow_read(&sf, stat, 100) > 0)
        {
            log_info("[CONTROLLER] receive success stat [%s] from %s:%d\n",
                     stat, remote_ip_addr, remote_port);
        } else
        {
            log_warn("[CONTROLLER] cannot receive stat from %s:%d\n",
                     remote_ip_addr, remote_port);
            retval = 1;
        }

        // TODO: how to parse and use this stat info?

monitor_close:
        subflow_close(&sf);
        return retval;
    }


    // below are operations that directly control MPP
    mpp_ctrl_msg ctrl_msg;

    if (strcmp(operation, "health_check") == 0) ctrl_msg = MPP_CTRL_HEALTH_CHECK;
    else if (strcmp(operation, "update_topo") == 0) ctrl_msg = MPP_CTRL_UPDATE_TOPO;
    else if (strcmp(operation, "update_topo_pre") == 0)
    {
        if (argc < 7)
        {
            log_warn("[CONTROLLER] not enough arguments for update_topo\n");
            exit(1);
        }
        ctrl_msg = MPP_CTRL_UPDATE_TOPO_PRE;
    }
    else if (strcmp(operation, "reset") == 0) ctrl_msg = MPP_CTRL_RESET;
    else if (strcmp(operation, "close") == 0) ctrl_msg = MPP_CTRL_CLOSE;
    else
    {
        log_warn("[CONTROLLER] unknown signal\n");
        exit(1);
    }

    subflow sf;

    if (client_init(&sf, remote_ip_addr, remote_port) == -1)
    {
        perror("client_init");
        exit(1);
    };

    if(client_connect(&sf) == -1)
    {
        perror("client_connect");
        exit(1);
    }

    log_info("[CONTROLLER] connect to %s:%d\n", remote_ip_addr, remote_port);

    int retval = 0;

    if (ctrl_msg == MPP_CTRL_CLOSE || ctrl_msg == MPP_CTRL_RESET ||
        ctrl_msg == MPP_CTRL_UPDATE_TOPO ||
        ctrl_msg == MPP_CTRL_HEALTH_CHECK)
    {
        char msg_char = mpp_generate_msg(ctrl_msg);
        if (subflow_write(&sf, &msg_char, MPP_CTRL_MSG_LEN) == MPP_CTRL_MSG_LEN)
        {
            log_info("[CONTROLLER] send signal %d to %s:%d\n",
                     ctrl_msg, remote_ip_addr, remote_port);
            if (ctrl_msg == MPP_CTRL_CLOSE) goto close;
        } else
        {
            log_warn("[CONTROLLER] fail to send signal %d to %s:%d\n",
                     ctrl_msg, remote_ip_addr, remote_port);
            retval = 1;
            goto close;
        }
    } else
    {
        // ctrl_msg = MPP_CTRL_UPDATE_TOPO_PRE
        int sf_num = atoi(argv[4]);
        char sf_num_char = mpp_int_to_char(sf_num);

        int pkt_size = MPP_CTRL_MSG_LEN + 1 + MPP_ADDR_LEN * sf_num;
        char pkt[pkt_size];

        pkt[0] = mpp_generate_msg(ctrl_msg);
        pkt[1] = sf_num_char;

        for (int i = 0; i < sf_num; ++i)
        {
            mpp_addr addr;
            mpp_generate_ip(addr.ip, argv[5 + 2 * i]);
            mpp_generate_port(addr.port, atoi(argv[6+ 2 * i]));
            memcpy(pkt + MPP_CTRL_MSG_LEN + 1 + MPP_ADDR_LEN * i, (char*) &addr, MPP_ADDR_LEN);
        }

        if (subflow_write(&sf, pkt, pkt_size) == pkt_size)
        {
            log_info("[CONTROLLER] send update_topo signal with arguments to %s:%d\n",
                     remote_ip_addr, remote_port);
        } else
        {
            log_warn("[CONTROLLER] fail to send update_topo signal to %s:%d\n",
                     remote_ip_addr, remote_port);
            retval = 1;
            goto close;
        }
    }

    char re_msg_char;
    if (subflow_read(&sf, &re_msg_char, MPP_CTRL_MSG_LEN) == MPP_CTRL_MSG_LEN ||
        mpp_parse_msg(re_msg_char) == MPP_RE_SUCCESS)
    {
        log_info("[CONTROLLER] receive success signal from %s:%d\n",
                 remote_ip_addr, remote_port);
    } else
    {
        log_warn("[CONTROLLER] receive other (non-successful) signal from %s:%d\n",
                 remote_ip_addr, remote_port);
        retval = 1;
    }

close:    
    subflow_close(&sf);
    return retval;
}

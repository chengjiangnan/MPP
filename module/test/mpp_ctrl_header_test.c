#include "mpp_ctrl_header.h"

#include <stdio.h>

int main(int argc, char* argv[])
{
    mpp_ctrl_msg msg =  MPP_CTRL_UPDATE;
    char* ip_readable = "127.255.0.1";
    int port_readable = 53210;

    char msg_char = mpp_generate_msg(msg);
    mpp_ctrl_msg msg2 = mpp_parse_msg(msg_char);

    char ip[4];
    char ip_readable2[16];
    mpp_generate_ip(ip, ip_readable);
    mpp_parse_ip(ip_readable2, ip);

    char port[2];
    int port_readable2;
    mpp_generate_port(port, port_readable);

    port_readable2 = mpp_parse_port(port);

    printf("[PARSE_HEADER] msg: %d, ip: %s, port: %d\n",
           msg2, ip_readable2, port_readable2);

    return 0;
}

#include "mpp_ctrl_header.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char mpp_int_to_char(int num)
{
    return num + '0';
}

int mpp_char_to_int(char c)
{
    return (int) c - '0';
}

char mpp_generate_msg(mpp_ctrl_msg msg)
{
    return msg + '0';
}

mpp_ctrl_msg mpp_parse_msg(char msg_char)
{
    return (mpp_ctrl_msg) msg_char - '0';
}

void mpp_generate_ip(char ip[4], const char* ip_readable)
{
    unsigned int octet[4];
    sscanf(ip_readable, "%u.%u.%u.%u", &octet[0], &octet[1], &octet[2], &octet[3]);

    for (int i = 0; i < 4; ++i)
        ip[i] = octet[i] & 0xff;
}

void mpp_parse_ip(char* ip_readable, char ip[4])
{
    unsigned int octet[4];
    for (int i = 0; i < 4; ++i)
        octet[i] = ip[i] & 0xff;
    sprintf(ip_readable, "%u.%u.%u.%u", octet[0], octet[1], octet[2], octet[3]);
}

void mpp_generate_port(char port[2], int port_readable)
{
    port[0] = ((unsigned int) port_readable / 256) & 0xff;
    port[1] = ((unsigned int) port_readable % 256) & 0xff;
}

int mpp_parse_port(char port[2])
{
    unsigned int octet[2];
    for (int i = 0; i < 2; ++i)
        octet[i] = port[i] & 0xff;
    return octet[0] * 256 + octet[1];
}

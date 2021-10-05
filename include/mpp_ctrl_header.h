#ifndef __MPP_CTRL_HEADER_H__
#define __MPP_CTRL_HEADER_H__ 1

/* mpp ctrl message */
typedef enum { MPP_CTRL_HEALTH_CHECK = 1,
               MPP_CTRL_UPDATE_TOPO_PRE,
               MPP_CTRL_UPDATE_TOPO,
               MPP_CTRL_RESET,
               MPP_CTRL_CLOSE,
               MPP_RE_SUCCESS,
               MPP_RE_FAIL } mpp_ctrl_msg;

typedef struct mpp_addr
{
    char ip[4];
    char port[2];
} mpp_addr;

#define MPP_CTRL_MSG_LEN 1
#define MPP_ADDR_LEN sizeof(mpp_addr)

/*
 * translate int into char
 */
char mpp_int_to_char(int num);

/*
 * translate char into int
 */
int mpp_char_to_int(char c);

/*
 * translate mpp_ctrl_msg into char
 */
char mpp_generate_msg(mpp_ctrl_msg msg);

/*
 * translate char into mpp_ctrl_msg
 */
mpp_ctrl_msg mpp_parse_msg(char msg_char);

/*
 * translate ip into char[4]
 */
void mpp_generate_ip(char ip[4], const char* ip_readable);

/*
 * translate char[4] into ip_readable
 */
void mpp_parse_ip(char* ip_readable, char ip[4]);

/*
 * translate port into char[2]
 */
void mpp_generate_port(char port[2], int port_readable);

/*
 * translate char[2] into port
 */
int mpp_parse_port(char port[2]);

#endif /*__MPP_CTRL_HEADER_H__*/

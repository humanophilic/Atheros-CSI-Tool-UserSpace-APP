/*
 * =====================================================================================
 *       Filename:  main.c
 *
 *    Description:  Here is an example for receiving CSI matrix 
 *                  Basic CSi procesing fucntion is also implemented and called
 *                  Check csi_fun.c for detail of the processing function
 *        Version:  1.0
 *
 *         Author:  Yaxiong Xie
 *         Email :  <xieyaxiongfly@gmail.com>
 *   Organization:  WANDS group @ Nanyang Technological University
 *   
 *   Copyright (c)  WANDS group @ Nanyang Technological University
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "csi_fun.h"

#define BUFSIZE 4096

int quit;
unsigned char buf_addr[BUFSIZE];
unsigned char data_buf[1500];

COMPLEX csi_matrix[3][3][114];
csi_struct*   csi_status;

void sig_handler(int signo)
{
    if (signo == SIGINT)
        quit = 1;
}

int close_socket(int s) {
    if (s != 0) {
        close(s);
    }
}

int main(int argc, char* argv[])
{
    FILE*       fp;
    int         fd;
    int         i;
    int         total_msg_cnt,cnt;
    int         log_flag;
    int         sock = 0;
    unsigned char endian_flag;
    u_int16_t   buf_len;
    struct sockaddr_in addr;
    
    log_flag = 1;
    csi_status = (csi_struct*)malloc(sizeof(csi_struct));
    /* check usage */
    switch(argc) {
        case 4:
            fp = fopen(argv[3],"w");
            if (!fp){
                printf("Fail to open <output_file>, are you root?\n");
                fclose(fp);
                return 0;
            }
            /* no break */
        case 3:
            break;
        default:
            puts("Invalid arguments!");
            puts("Usage: recv_csi <server_ip_address> <dest_port_number> <output_file>");
            return -1;
            break;
    }

    /* open socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("socket error\n");
        return -1;
    }
    memset(&addr, 0, sizeof(struct sockaddr_in));

    /* specify server IP and port */
    unsigned short port = (unsigned short)strtol(argv[1], NULL, 10);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = inet_addr(argv[2]);

    /* connect to the server */
    if (connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1) {
        printf("connect error\n");
        close(sock);
        return -1;
    }

    fd = open_csi_device();
    if (fd < 0){
        perror("Failed to open the device...");
        return errno;
    }
    
    printf("#Receiving data! Press Ctrl+C to quit!\n");

    quit = 0;
    total_msg_cnt = 0;

    while(1){
        if (1 == quit){
            fclose(fp);
            close_csi_device(fd);
            close_socket(sock);
            return 0;
        }

        /* keep listening to the kernel and waiting for the csi report */
        cnt = read_csi_buf(buf_addr,fd,BUFSIZE);

        if (cnt){
            total_msg_cnt += 1;

            /* fill the status struct with information about the rx packet */
            record_status(buf_addr, cnt, csi_status);

            /* 
             * fill the payload buffer with the payload
             * fill the CSI matrix with the extracted CSI value
             */
            record_csi_payload(buf_addr, csi_status, data_buf, csi_matrix); 
            
            /* Till now, we store the packet status in the struct csi_status 
             * store the packet payload in the data buffer
             * store the csi matrix in the csi buffer
             * with all those data, we can build our own processing function! 
             */
            //porcess_csi(data_buf, csi_status, csi_matrix);   
            
            printf("Recv %dth msg with rate: 0x%02x | payload len: %d\n",total_msg_cnt,csi_status->rate,csi_status->payload_len);
            
            /* log the received data for off-line processing */
            if (log_flag){
                buf_len = csi_status->buf_len;
                fwrite(&buf_len,1,2,fp);
                fwrite(buf_addr,1,buf_len,fp);
                send(sock, &buf_len, 2, 0);
                send(sock, buf_addr, buf_len, 0);
            }
        }
    }
    fclose(fp);
    close_csi_device(fd);
    free(csi_status);
    close(sock);
    return 0;
}

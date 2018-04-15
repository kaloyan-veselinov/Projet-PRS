#ifndef _SOCK_H
#define _SOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signal.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define DEBUG 1

#define TRUE 1
#define FALSE 0

#define RCVSIZE 1500
#define HEADER_SIZE 6
#define DATA_SIZE (RCVSIZE-HEADER_SIZE)
#define ACK_SIZE (HEADER_SIZE+3)
#define SYN_SIZE 4*sizeof(char)
#define BUFFER_SIZE 20
#define WINDOW 1

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

typedef struct client_address{
    int desc;
    struct sockaddr_in addr;
}ADDRESS;

struct sockaddr_in init_addr(int port, int addr);

int create_socket(int port);

int my_bind(int socket, struct sockaddr* addr);

int random_port();

int my_accept(int desc, struct sockaddr_in* addr);

void send_disconnect_message(int data_desc, struct sockaddr_in adresse);

#endif

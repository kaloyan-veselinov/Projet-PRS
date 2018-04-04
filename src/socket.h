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

#include <netinet/in.h>
#include <arpa/inet.h>

#define RCVSIZE 1024

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

struct sockaddr_in init_addr(int port, int addr);

int create_socket(int port);

int my_bind(int socket, struct sockaddr* addr);

int my_accept(int desc, struct sockaddr_in* addr);

#endif

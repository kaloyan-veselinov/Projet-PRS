#ifndef _SOCK_H
#define _SOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define RCVSIZE 1024

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

int open_socket(int mode);

struct sockaddr_in init_addr(int port, int addr);

void my_bind(int socket, struct sockaddr* addr);

int my_connect(int socket, struct sockaddr* addr);

int my_accept(int fd, struct sockaddr_in* addr, struct sockaddr_in* client);
#endif


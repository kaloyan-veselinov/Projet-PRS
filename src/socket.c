#include "socket.h"

int open_socket(int mode) {

  int sock;
  sock = socket(AF_INET, mode,0);
  if (sock < 0) {
    fprintf(stderr, "Erreur d'ouverture de la socket avec retour de %d\n", sock);
    exit(-1);
  }

  int reuse = 1; /* PERMET LA REUTILISATION DE L'ADRESSE */
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  return sock;
}

struct sockaddr_in init_addr(int port, int addr) {
  struct sockaddr_in my_addr;
  memset((char*)&my_addr,0,sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(port);
  my_addr.sin_addr.s_addr= htonl(addr);

  return my_addr;
}

void my_bind(int socket, struct sockaddr* addr) {
  int res = bind (socket, addr , sizeof(*addr));
  if (res < 0) {
    fprintf(stderr, "Erreur de bind de la socket\n");
    exit(-1);
  }
}

int my_connect(int desc, struct sockaddr* adresse) {
  socklen_t addrLen = sizeof(*adresse);

  char msg[RCVSIZE] = "SYN";
  sendto(desc,msg,strlen(msg),0, adresse, addrLen);

  printf("SYN envoyé.\n");

  memset(msg, 0, RCVSIZE);

  recv(desc, msg, sizeof(msg),0);
  if (strncmp("SYN-ACK", msg, strlen("SYN-ACK")) != 0) {
    fprintf(stderr, "Connexion refusée\n");
    return -1;
  }

  char sport[RCVSIZE];
  strcpy(sport, msg+strlen("SYN-ACK"));
  printf("Valeur reçue en sport : %s\n", sport);

  int port = atoi(sport);

  printf("SYN-ACK reçu\n");

  strcpy(msg,"ACK");
  sendto(desc,msg,strlen(msg),0, (struct sockaddr*) adresse, addrLen);


  printf("ACK envoyé\n");

  return port;

}



int my_accept(int desc, struct sockaddr_in* addr, struct sockaddr_in* client) {

  socklen_t clientLen = sizeof(client);
  socklen_t addrLen = sizeof(*addr);

  char msg[RCVSIZE];

  int msgSize= recvfrom(desc,msg,RCVSIZE,0, &client, &clientLen);

  if (strncmp("SYN", msg, strlen("SYN")) != 0) {
    fprintf(stderr, "Connexion refusée\n");
    return -1;
  }

  printf("SYN reçu.\n");

  int util_sock = open_socket(SOCK_DGRAM);
  my_bind(util_sock, (struct sockaddr*) addr);

  getsockname(util_sock, (struct sockaddr*) addr, &addrLen);

  printf("Port : %d\n", addr->sin_port);
  int out = addr->sin_port;

  strcpy(msg,"SYN-ACK");
  sprintf(msg+ strlen("SYN-ACK"), "%d", out);
  sendto(desc,msg,strlen(msg),0, (struct sockaddr*) &client, clientLen);

  printf("SYN-ACK envoyé\n");

  recvfrom(desc,msg,RCVSIZE,0, &client, &clientLen);

  if (strncmp("ACK", msg, strlen("ACK")) != 0) {
    fprintf(stderr, "Connexion refusée, reçu : %.3s\n", msg);
    return -1;
  }

  printf("ACK reçu, connexion acceptée.\n");

  return util_sock;
}


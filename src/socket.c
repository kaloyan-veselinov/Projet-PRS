#include "socket.h"

struct sockaddr_in init_addr(int port, int addr) {
  struct sockaddr_in my_addr;
  memset((char*)&my_addr,0,sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(port);
  my_addr.sin_addr.s_addr= htonl(addr);

  return my_addr;
}

int create_socket(int port) {
    struct sockaddr_in adresse = init_addr(port, INADDR_ANY);
    int desc = socket(AF_INET, SOCK_DGRAM, 0);
    int valid = 1;

    if (desc < 0) {
        perror("cannot create socket\n");
        return -1;
    }

    setsockopt(desc, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));

    my_bind(desc,(struct sockaddr *)&adresse);

    return desc;
}

int my_bind(int socket, struct sockaddr* addr) {
  if (bind (socket, addr , sizeof(*addr)) < 0) {
    fprintf(stderr, "Erreur de bind de la socket\n");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}


int my_accept(int desc, struct sockaddr_in* addr) {

  char msg[RCVSIZE];
  memset(msg, '\0', RCVSIZE);

  socklen_t addr_len = sizeof(addr);
  memset(addr, 0, addr_len);


  if(recvfrom(desc,msg,RCVSIZE,0, (struct sockaddr *)&addr, &addr_len)==-1){
    perror("Error receiving on connection socket");
    return EXIT_FAILURE;
  }

  if (strncmp("SYN", msg, strlen("SYN")) != 0) {
    perror("Mauvais message de connexion\n");
    return EXIT_FAILURE;
  }
  printf("SYN reçu.\n");

  struct sockaddr_in data_addr;
  int data_desc = create_socket(0);
  getsockname(data_desc, (struct sockaddr*) &data_addr, &addr_len);

  printf("Port : %d\n", data_addr.sin_port);

  sprintf(msg, "SYN-ACK%04d", ntohs(data_addr.sin_port));
  sendto(desc,msg,strlen(msg)+1,0, (struct sockaddr*) &addr, addr_len);
  printf("SYN-ACK envoyé\n");

  if(recvfrom(desc,msg,RCVSIZE,0, (struct sockaddr*) &addr, &addr_len)==-1){
    perror("Erreur de réception du ACK de connexion\n.");
    return EXIT_FAILURE;
  }

  if (strncmp("ACK", msg, strlen("ACK")) != 0) {
    fprintf(stderr, "Mauvais message d'ACK de connexion.\n");
    EXIT_FAILURE;
  }
  printf("ACK reçu, connexion acceptée.\n");

  return data_desc;
}

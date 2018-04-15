#include "socket.h"

int initialized = 0;

void set_timeout(int desc, long tv_sec, long tv_usec) {
  struct timeval tv;

  tv.tv_sec  = tv_sec;
  tv.tv_usec = tv_usec;
  setsockopt(desc, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
}

void send_disconnect_message(int data_desc, struct sockaddr_in adresse) {
  sendto(data_desc,
         "FIN",
         4 * sizeof(char),
         0,
         (struct sockaddr *)&adresse,
         sizeof(adresse));
}

struct sockaddr_in init_addr(int port, int addr) {
  struct sockaddr_in my_addr;

  memset((char *)&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family      = AF_INET;
  my_addr.sin_port        = htons(port);
  my_addr.sin_addr.s_addr = htonl(addr);

  return my_addr;
}

int create_socket(int port) {
    #if DEBUG
  printf("Creating socket on port %d\n", port);
    #endif /* if DEBUG */
  struct sockaddr_in adresse = init_addr(port, INADDR_ANY);
  int desc                   = socket(AF_INET, SOCK_DGRAM, 0);
  int valid                  = 1;

  if (desc < 0) {
    perror("cannot create socket\n");
    return -1;
  }

  setsockopt(desc, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));


  my_bind(desc, (struct sockaddr *)&adresse);

    #if DEBUG
  printf("Socket created on port %d\n", port);
    #endif /* if DEBUG */

  return desc;
}

int my_bind(int socket, struct sockaddr *addr) {
  #if DEBUG
  printf("Binding socket on port %d\n",
         ntohs(((struct sockaddr_in *)addr)->sin_port));
  #endif /* if DEBUG */

  if (bind(socket, addr, sizeof(*addr)) < 0) {
    fprintf(stderr, "Erreur de bind de la socket\n");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int random_value(int min, int max) {
  if (!initialized) {
    srand(time(NULL));
    initialized = 1;
  }
  return rand() % (max - min) + min;
}

int random_port() {
  int port = random_value(1025, 9999);

  #if DEBUG
  printf("Generated random port %d\n", port);
  #endif /* if DEBUG */
  return port;
}

int my_accept(int desc, struct sockaddr_in *addr) {
  char msg[RCVSIZE];

  memset(msg,  '\0', RCVSIZE);

  socklen_t addr_len = sizeof(addr);
  memset(addr, 0,    addr_len);

  #if DEBUG
  printf("Wainting for SYN on desc %d \n", desc);
  #endif /* if DEBUG */

  if (recvfrom(desc, msg, SYN_SIZE, 0, (struct sockaddr *)&addr,
               &addr_len) == -1) {
    perror("Error receiving on connection socket");
    return EXIT_FAILURE;
  }
  #if DEBUG
  printf("SYN received on desc %d: %s \n", desc, msg);
  #endif /* if DEBUG */

  if (strncmp("SYN", msg, SYN_SIZE) != 0) {
    perror("Mauvais message de connexion\n");
    return EXIT_FAILURE;
  }
  printf("SYN confirmé\n");

  int data_desc, port;

  do {
    port      = random_port();
    data_desc = create_socket(port);
  } while (data_desc == -1);
  printf("Port : %d\n", port);

  sprintf(msg, "SYN-ACK%04d", port);
  sendto(desc, msg, strlen(msg) + 1, 0, (struct sockaddr *)&addr, addr_len);
  #if DEBUG
  printf("SYN-ACK envoyé\n");
  #endif /* if DEBUG */

  // TODO add timer

  if (recvfrom(desc, msg, RCVSIZE, 0, (struct sockaddr *)&addr,
               &addr_len) == -1) {
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

long timedifference_usec(struct timeval t0, struct timeval t1) {
  return (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_usec - t0.tv_usec);
}

void update_rto(long *rto, long *srtt, long *rtt, long *rttvar) {
  long delta = *rtt - *srtt;

  *srtt   += G * delta;
  *rttvar += H * (abs((int)delta) - *rttvar);
  *rto     = *srtt + 4 * (*rttvar);
}

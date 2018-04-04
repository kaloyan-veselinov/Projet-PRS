#include "serveur1-PerformancesRadicalementSuperieures.h"

int desc, data_desc;
FILE* file;
struct sockaddr_in adresse;
pthread_mutex_t start;
socklen_t addr_len;

void end_handler() {
  close(data_desc);
  close(desc);
  fclose(file);
  exit(EXIT_SUCCESS);
}

void *send_thread(void *arg) {
  int bytes_read;
  int sequence_nb = 1;
  char buffer[RCVSIZE+1];
  if(pthread_mutex_unlock(&start)!=0) perror("Err unlocking start\n");
  do{
    memset(buffer, '\0', RCVSIZE);
    sprintf(buffer, "%6d", sequence_nb);
    bytes_read = fread(buffer+HEADER_SIZE, 1, DATA_SIZE, file);

    if(bytes_read==0) {
      memset(buffer, '\0', RCVSIZE);
      strcpy(buffer, "FIN");
      printf("EOF\n");
    }
    else if(bytes_read==-1) perror("Error reading file\n");

    sendto(desc, buffer, RCVSIZE, 0, (struct sockaddr *)&adresse, sizeof(adresse));
    sequence_nb++;
  } while(bytes_read != 0);
  fclose(file);
  pthread_exit(NULL);
}

void *ack_thread(void *arg) {
    pthread_mutex_lock(&start);
    char buffer[ACK_SIZE+1];
    int rcv;
    struct sockaddr_in src_addr;

    do{
        memset(buffer, '\0', ACK_SIZE);
        rcv=recvfrom(desc, buffer, HEADER_SIZE, 0, (struct sockaddr *)&src_addr, &addr_len);
        printf("%s\n", buffer);
    } while(strcmp(buffer, "END")!=0);

    // TODO perte de packets

    pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {
  signal(SIGTSTP, end_handler);

  struct sockaddr_in src_addr;
  socklen_t addr_len = sizeof(src_addr);
  char buffer[RCVSIZE] = {0};

  int port;
  if(argc==2) port = atoi(argv[1]);
  else port = 4242;
  desc = create_socket(port);

  memset(&src_addr, 0, addr_len);
  data_desc = my_accept(desc, &src_addr);
  close(desc);

  if(recvfrom(data_desc, buffer, sizeof(buffer), 0, (struct sockaddr *)&src_addr, &addr_len)==-1) {
    perror("Error receiving file name\n");
    end_handler();
  }

  file = fopen(buffer, "r");

  addr_len = sizeof(adresse);

  pthread_t snd;
  pthread_t ack;
  pthread_mutex_init(&start, NULL);
  pthread_mutex_lock(&start);

  if(pthread_create (&snd, NULL, send_thread, (void*)NULL)!=0){
    perror("Error creating send thread");
    exit(EXIT_FAILURE);
  }
  if(pthread_create (&ack, NULL, ack_thread, (void*)NULL)!=0){
    perror("Error creating ack thread");
    exit(EXIT_FAILURE);
  }

  pthread_join (snd, NULL);
  pthread_join (ack, NULL);

  end_handler();
  return 0;
}

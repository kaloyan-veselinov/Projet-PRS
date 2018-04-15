#include "serveur1-PerformancesRadicalementSuperieures.h"

int   desc, data_desc;
FILE *file;
struct sockaddr_in adresse;
pthread_mutex_t    ack_mutex;
pthread_cond_t     ack_cond;

socklen_t addr_len;
int data_desc_open = FALSE;
int file_open      = FALSE;
int desc_open      = FALSE;
int ack_received   = TRUE;

void end_handler() {
  #if DEBUG
  printf("Entering end handler\n");
  #endif /* if DEBUG */

  send_disconnect_message(data_desc, adresse);

  if (data_desc_open) close(data_desc);

  if (desc_open) close(desc);

  if (file_open) fclose(file);
  exit(EXIT_SUCCESS);
}

void* send_thread(void *args) {
  ADDRESS *client_address    = args;
  int data_desc              = client_address->desc;
  struct sockaddr_in adresse = client_address->addr;

  char segment_buffer[BUFFER_SIZE][RCVSIZE];
  int  bytes_read = 0;
  int  sequence_nb = 1;
  int  snd;
  int  i = 0;
  int datagram_size;

  do {
    i = 0;
    do {
      memset(segment_buffer[sequence_nb%BUFFER_SIZE], '\0', RCVSIZE);
      sprintf(segment_buffer[sequence_nb%BUFFER_SIZE], "%06d", sequence_nb);
      bytes_read = fread(segment_buffer[sequence_nb%BUFFER_SIZE] + HEADER_SIZE, 1, DATA_SIZE, file);
      datagram_size = bytes_read + (HEADER_SIZE*sizeof(char));

      pthread_mutex_lock(&ack_mutex);
      if(!ack_received) pthread_cond_wait(&ack_cond, &ack_mutex);
      ack_received = FALSE;
      pthread_mutex_unlock(&ack_mutex);

      //TODO add current sending pointer
      //TODO add buffer for datagram sizes
      //TODO add file reading before wait
      //TODO retransmission
      //TODO timer
      //TODO add send-ack synchronization
      //TODO implement slow start -> until a datagram has been lost, window *2 each time; when a datagram is lost, divide window by 2, and then linear phase

      if (bytes_read == -1) perror("Error reading file\n");
      else if (bytes_read > 0) {
        snd = sendto(data_desc,
                     segment_buffer[sequence_nb%BUFFER_SIZE],
                     datagram_size,
                     0,
                     (struct sockaddr *)&adresse,
                     sizeof(adresse));
        printf("Send size: %d, RCVSIZE: %d\n", snd, RCVSIZE);
        if (snd < 0) {
          perror("Error sending segment\n");
          pthread_exit(NULL);
        }
        #if DEBUG
        printf("Sent segment %06d\n", sequence_nb);
        #endif /* if DEBUG */
        sequence_nb++;
      }
      i++;
    } while(i<WINDOW && bytes_read != 0);
  } while (bytes_read != 0);
  sleep(1);
  end_handler();
  pthread_exit(NULL);
}

void* ack_thread(void *args) {
  ADDRESS *client_address    = args;
  int data_desc              = client_address->desc;

  char buffer[ACK_SIZE + 1];
  struct sockaddr_in src_addr;
  int rcv;

  do {
    memset(buffer, '\0', ACK_SIZE + 1);
    #if DEBUG
    printf("Waiting ACK\n");
    #endif /* if DEBUG */
    rcv = recvfrom(data_desc,
                   buffer,
                   ACK_SIZE,
                   0,
                   (struct sockaddr *)&src_addr,
                   &addr_len);

    if (rcv < 0) {
      perror("Error receiving ACK\n");
      pthread_exit(NULL);
    }
    printf("%s\n", buffer);

    pthread_mutex_lock(&ack_mutex);
    ack_received = TRUE;
    pthread_cond_signal(&ack_cond);
    pthread_mutex_unlock(&ack_mutex);
  } while (strncmp(buffer, "FIN", strlen("FIN") + 1) != 0);

  // TODO perte de packets

  pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {
  signal(SIGTSTP, end_handler);

  struct sockaddr_in src_addr;
  socklen_t addr_len   = sizeof(src_addr);
  char buffer[RCVSIZE] = { 0 };


  int port;

  if (argc == 2) port = atoi(argv[1]);
  else port = 4242;
  desc      = create_socket(port);
  desc_open = TRUE;

  memset(&src_addr, 0, addr_len);
  data_desc      = my_accept(desc, &src_addr);
  data_desc_open = TRUE;
  #if DEBUG
  printf("Data file descriptor: %d\n", data_desc);
  #endif /* if DEBUG */

  #if DEBUG
  printf("Waiting for file name\n");
  #endif /* if DEBUG */

  if (recvfrom(data_desc, buffer, sizeof(buffer), 0, (struct sockaddr *)&src_addr,
               &addr_len) == -1) {
    perror("Error receiving file name\n");
    end_handler();
  }
  adresse = src_addr;

  #if DEBUG
  printf("Opening file\n");
  #endif /* if DEBUG */
  file = fopen(buffer, "r");

  if (file == NULL) {
    perror("Error opening file\n");
    end_handler();
  }
  file_open = TRUE;

  addr_len = sizeof(adresse);

  pthread_t snd;
  pthread_t ack;
  pthread_mutex_init(&ack_mutex, NULL);
  pthread_cond_init(&ack_cond, NULL);

  ADDRESS addr;
  addr.addr = adresse;
  addr.desc = data_desc;

  #if DEBUG
  printf("Creating send thread\n");
  #endif /* if DEBUG */

  if (pthread_create(&snd, NULL, send_thread, (void *)&addr) != 0) {
    perror("Error creating send thread");
    exit(EXIT_FAILURE);
  }
  #if DEBUG
  printf("Creating ack thread\n");
  #endif /* if DEBUG */

  if (pthread_create(&ack, NULL, ack_thread, (void *)&addr) != 0) {
    perror("Error creating ack thread");
    exit(EXIT_FAILURE);
  }

  pthread_join(snd, NULL);
  pthread_join(ack, NULL);
  #if DEBUG
  printf("Joining threads\n");
  #endif /* if DEBUG */

  end_handler();
  return 0;
}

// TODO add current sending pointer

// TODO add buffer for datagram sizes
// TODO add file reading before wait
// TODO retransmission and dropped packets
// TODO RTT
// TODO implement slow start -> until a datagram has been lost, window *2 each
// time; when a datagram is lost, divide window by 2, and then linear phase
// TODO implement connect with UDP

#include "serveur1-PerformancesRadicalementSuperieures.h"

int   desc, data_desc;
FILE *file;
struct sockaddr_in adresse;

int data_desc_open = FALSE;
int file_open      = FALSE;
int desc_open      = FALSE;

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

void handle_client(int data_desc, struct sockaddr_in adresse, socklen_t addr_len) {
  int  end = FALSE;
  int  last_ack  = 0;
  int timeout_ack = FALSE;
  struct timeval snd_time[BUFFER_SIZE];
  struct timeval ack_time;

  struct sockaddr_in src_addr;

  char segment_buffer[BUFFER_SIZE][RCVSIZE];
  char ack_buffer[ACK_SIZE];
  int  bytes_read_buffer[BUFFER_SIZE];
  int  sequence_nb = 1;

  int  snd;
  int  rcv;

  int  i = 0;
  int  j;

  int  datagram_size;
  int  p_buff;

  long rtt;
  long srtt = 100000;
  long rto = 100000;
  long rttvar = 0;

  do {

    // transmission
    i = 0;
    do {
      if (timeout_ack) {
        sequence_nb = last_ack + 1;
        p_buff = sequence_nb % BUFFER_SIZE;
        timeout_ack = FALSE;
      }
      else{
        p_buff = sequence_nb % BUFFER_SIZE;
        memset(segment_buffer[p_buff], '\0', RCVSIZE);
        sprintf(segment_buffer[p_buff], "%06d", sequence_nb);
        bytes_read_buffer[p_buff] = fread(segment_buffer[p_buff] + HEADER_SIZE,
                                          1,
                                          DATA_SIZE,
                                          file);
      }

      datagram_size = bytes_read_buffer[p_buff] + (HEADER_SIZE * sizeof(char));

      if (bytes_read_buffer[p_buff] == -1) perror("Error reading file\n");
      else if (bytes_read_buffer[p_buff] > 0) {
        snd = sendto(data_desc,
                     segment_buffer[p_buff],
                     datagram_size,
                     0,
                     (struct sockaddr *)&adresse,
                     sizeof(adresse));

        gettimeofday(snd_time + p_buff, 0);

        if (snd < 0) perror("Error sending segment\n");
        printf("Sent segment %06d\n", sequence_nb);
        sequence_nb++;
        i++;
      }
      end = bytes_read_buffer[p_buff] == 0;
      printf("End: %d, bytes_read: %d", end, bytes_read_buffer[p_buff]);
    } while (i < WINDOW && !end);

    // acknoledgment
    for(j=0; j<i; j++){
      set_timeout(data_desc, 0, rto);
      memset(ack_buffer, '\0', ACK_SIZE + 1);
      #if DEBUG
      printf("Waiting ACK %d\n", last_ack + 1);
      #endif /* if DEBUG */
      rcv = recvfrom(data_desc,
                     ack_buffer,
                     ACK_SIZE,
                     0,
                     (struct sockaddr *)&src_addr,
                     &addr_len);

      if (rcv < 0) {
        if (errno == EWOULDBLOCK) {
          perror("Blocked");
          timeout_ack = TRUE;
          end = FALSE;
          break;
        }
        else perror("Error receiving ACK\n");
      }
      else {
        last_ack = atoi(ack_buffer + 3);
        gettimeofday(&ack_time, 0);
        rtt = timedifference_usec(snd_time[last_ack%BUFFER_SIZE], ack_time);
        update_rto(&rto, &srtt, &rtt, &rttvar);
        printf("RTO %ld ", rto);
        printf("Received ACK %d\n", last_ack);
      }
    }
  } while (!end);
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
  printf("Waiting for file name\n");
  #endif /* if DEBUG */

  if (recvfrom(data_desc, buffer, sizeof(buffer), 0, (struct sockaddr *)&src_addr,
               &addr_len) == -1) {
    perror("Error receiving file name\n");
    end_handler();
  }
  adresse = src_addr;

  file = fopen(buffer, "r");

  if (file == NULL) {
    perror("Error opening file\n");
    end_handler();
  }
  file_open = TRUE;

  addr_len = sizeof(adresse);

  handle_client(data_desc, adresse, addr_len);

  end_handler();
  return 0;
}

#include "serveur1-PerformancesRadicalementSuperieures.h"

int desc, data_desc;
FILE *file;

int data_desc_open = FALSE;
int file_open = FALSE;
int desc_open = FALSE;

__sighandler_t end_handler() {
#if DEBUG
  printf("Entering end handler\n");
#endif /* if DEBUG */

  send_disconnect_message(data_desc);

  if (data_desc_open) close(data_desc);

  if (desc_open) close(desc);

  if (file_open) fclose(file);
  exit(EXIT_SUCCESS);
}

void handle_client(int data_desc, long srtt, long rttvar) {
  SEGMENT segment;
  char ack_buffer[RCVSIZE];
  unsigned int sequence_number = 1;
  ssize_t snd, rcv;
  size_t datagram_size;
  short end;
  short retransmission;
  unsigned int parsed_ack = 0;
  long rtt, rto; // defines timeout delay
  struct timeval ack_time;

  set_timeout(data_desc, 0, 100000);

  do {
    // Initializing segment
    segment.nb_ack = 0;
    memset(segment.data, '\0', RCVSIZE);

    // Loading segment into buffer
    sprintf(segment.data, "%06d", sequence_number);
    segment.msg_size = fread(segment.data + HEADER_SIZE, 1, DATA_SIZE, file);
    if (segment.msg_size == -1) perror("Error reading file\n");
    datagram_size = segment.msg_size + (HEADER_SIZE * sizeof(char));

    // End if the message is empty
    end = (segment.msg_size == 0);
    if (!end) {
      // Send data to client and get sent time
      snd = send(data_desc, segment.data, datagram_size, 0);
      gettimeofday(&segment.snd_time, 0);
      retransmission = FALSE;

      if (snd < 0) perror("Error sending segment\n");
      else {
        printf("Sent segment %06d\n", sequence_number);

        do {
          // Waiting for ACK
          memset(ack_buffer, '\0', ACK_SIZE + 1);
          rcv = recv(data_desc, ack_buffer, ACK_SIZE, 0);

          if (rcv < 0) {
            // Timeout, resending segment
            send(data_desc, segment.data, datagram_size, 0);
            retransmission = TRUE;
          } else{
            // Received segment
            parsed_ack = (unsigned int) atoi(ack_buffer + 3);
            printf("Received ACK %d\n", parsed_ack);
            segment.nb_ack++;

            // Karn's algorithm, updating rto only if no retransmission
            if(!retransmission) {
              gettimeofday(&ack_time, 0);
              rtt = timedifference_usec(segment.snd_time, ack_time);
              update_rto(&rto, &srtt, &rtt, &rttvar);
              set_timeout(data_desc, 0, rto);
            }
          }

        } while(segment.nb_ack == 0);

        // Increment sequence number for next segment
        sequence_number++;
      }
    }
  } while (!end);
}

int main(int argc, char const *argv[]) {
  signal(SIGTSTP, (__sighandler_t) end_handler);

  struct sockaddr_in src_addr;
  socklen_t addr_len = sizeof(src_addr);
  char buffer[RCVSIZE] = {0};

  uint16_t port;

  if (argc == 2) {
    errno = 0;
    port = (uint16_t) strtol(argv[1], NULL, 10);
    if (errno != 0) {
      port = 4242;
    }
  } else port = 4242;

  desc = create_socket(port);
  desc_open = TRUE;

  // Initialize the timeout
  long srtt = 100000;
  long rttvar = 0;
  memset(&src_addr, 0, addr_len);
  data_desc = my_accept(desc, &src_addr, &srtt, &rttvar);
  data_desc_open = TRUE;

  if (recv(data_desc, buffer, sizeof(buffer), 0) == -1) {
    perror("Error receiving file name\n");
    end_handler();
  }

  file = fopen(buffer, "r");

  if (file == NULL) {
    perror("Error opening file\n");
    end_handler();
  }
  file_open = TRUE;

  handle_client(data_desc, srtt, rttvar);

  end_handler();
  return 0;
}

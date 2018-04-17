// TODO add current sending pointer

// TODO implement connect with UDP

#include "serveur1-PerformancesRadicalementSuperieures.h"

int   desc, data_desc;
FILE *file;

int data_desc_open = FALSE;
int file_open      = FALSE;
int desc_open      = FALSE;

void end_handler() {
  #if DEBUG
  printf("Entering end handler\n");
  #endif /* if DEBUG */

  send_disconnect_message(data_desc);

  if (data_desc_open) close(data_desc);

  if (desc_open) close(desc);

  if (file_open) fclose(file);
  exit(EXIT_SUCCESS);
}

int first_non_ack(int sgmt_acknoledged_buff[BUFFER_SIZE],
                  int sequence_nb,
                  int window) {
  int res = sequence_nb - window;

  while (res < sequence_nb &&
         sgmt_acknoledged_buff[res % BUFFER_SIZE]) res++;
  return res;
}

void reinit_sgmt_ack_buff(int sgmt_acknoledged_buff[BUFFER_SIZE],
                          int sequence_nb,
                          int window) {
  for (int i = sequence_nb - window; i < sequence_nb;
       i++) sgmt_acknoledged_buff[i % BUFFER_SIZE] = FALSE;
}

int load_sgmt(FILE *file, char buff[RCVSIZE], int sequence_nb) {
  memset(buff, '\0', RCVSIZE);
  sprintf(buff, "%06d", sequence_nb);
  int rd = fread(buff + HEADER_SIZE, 1, DATA_SIZE, file);

  if (rd == -1) perror("Error reading file\n");
  return rd;
}

int send_sgmts(FILE          *file,
               int            data_desc,
               char           segments[BUFFER_SIZE][RCVSIZE],
               int            bytes_read[BUFFER_SIZE],
               struct timeval snd_time[BUFFER_SIZE],
               int           *last_loaded_sgmt,
               int            sequence_nb,
               int            window) {
  int p_buff, datagram_size, snd, end;
  int nb_sent = 0;

  do {
    p_buff = sequence_nb % BUFFER_SIZE;

    if (sequence_nb > *last_loaded_sgmt) {
      bytes_read[p_buff] = load_sgmt(file, segments[p_buff], sequence_nb);
      *last_loaded_sgmt  = sequence_nb;
    }

    datagram_size = bytes_read[p_buff] + (HEADER_SIZE * sizeof(char));

    end = (bytes_read[p_buff] == 0) && (*last_loaded_sgmt == sequence_nb);

    if (!end) {
      snd = send(data_desc,
                 segments[p_buff],
                 datagram_size,
                 0);

      gettimeofday(snd_time + p_buff, 0);

      if (snd < 0) perror("Error sending segment\n");
      else{
        printf("Sent segment %06d\n", sequence_nb);
        sequence_nb++;
        nb_sent++;
      }
    }
  } while (nb_sent < window && !end);
  return nb_sent;
}

void handle_client(int data_desc) {
  int parsed_ack;

  struct timeval snd_time[BUFFER_SIZE];
  struct timeval ack_time;

  char segments[BUFFER_SIZE][RCVSIZE];
  char ack_buffer[ACK_SIZE];
  int  bytes_read[BUFFER_SIZE];
  int  sgmt_acknoledged_buff[BUFFER_SIZE] = { FALSE };
  int  sequence_nb                        = 1;
  int  window                             = 1;
  int  last_loaded_sgmt                   = 0;

  int rcv;

  int sgmt_sent;
  int nb_ack_to_rcv = 0;

  long rtt;
  long srtt   = 100000;
  long rto    = 100000;
  long rttvar = 0;

  do {
    sgmt_sent = send_sgmts(file,
                           data_desc,
                           segments,
                           bytes_read,
                           snd_time,
                           &last_loaded_sgmt,
                           sequence_nb,
                           window);
    sequence_nb += sgmt_sent;
    nb_ack_to_rcv += sgmt_sent;

    // reinit_sgmt_ack_buff(sgmt_acknoledged_buff, sequence_nb, i);

    // acknoledgment
    while (nb_ack_to_rcv > 0) {
      set_timeout(data_desc, 0, rto);
      memset(ack_buffer, '\0', ACK_SIZE + 1);
      rcv = recv(data_desc,
                 ack_buffer,
                 ACK_SIZE,
                 0);

      if (rcv < 0) {
        if (errno == EWOULDBLOCK) {
          sequence_nb = first_non_ack(sgmt_acknoledged_buff,
                                      sequence_nb,
                                      sgmt_sent);
          fprintf(stderr, "Timeout on ACK %d\n", sequence_nb);
          window = 1;
          nb_ack_to_rcv--;
          break;
        }
        else perror("Error receiving ACK\n");
      }
      else {
        parsed_ack = atoi(ack_buffer + 3);

        if (sgmt_acknoledged_buff[parsed_ack % BUFFER_SIZE]) {
          fprintf(stderr, "Selective ACK on segment %d\n", parsed_ack);
          sequence_nb                                     = parsed_ack + 1;
          sgmt_acknoledged_buff[parsed_ack % BUFFER_SIZE] = FALSE;
          window                                          = 1;
          nb_ack_to_rcv--;
          break;
        }
        else {
          nb_ack_to_rcv--;
          window++;
          gettimeofday(&ack_time, 0);
          rtt = timedifference_usec(snd_time[parsed_ack % BUFFER_SIZE], ack_time);
          update_rto(&rto, &srtt, &rtt, &rttvar);
          printf("RTO %ld ",          rto);
          printf("Received ACK %d\n", parsed_ack);
          sgmt_acknoledged_buff[parsed_ack % BUFFER_SIZE] = TRUE;
        }
      }
    }
  } while (sgmt_sent != 0 || nb_ack_to_rcv != 0);
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

  handle_client(data_desc);

  end_handler();
  return 0;
}

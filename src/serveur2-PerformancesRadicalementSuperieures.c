#include "socket.h"

int data_desc;
FILE *file;

int data_desc_open = FALSE;
int file_open = FALSE;

void end_handler() {
    fprintf(stderr, "%d entered end_handler\n", getpid());

    // Sending FIN
    send_disconnect_message(data_desc);

    // Closing data socket and file
    if (data_desc_open) close(data_desc);
    if (file_open) fclose(file);

    exit(EXIT_SUCCESS);
}

size_t get_datagram_size(SEGMENT segment) {
    return segment.data_size + (HEADER_SIZE * sizeof(char));
}

unsigned int first_non_ack_segment(SEGMENT segments[BUFFER_SIZE], unsigned int next_sequence_number, int nb_sent) {
    unsigned int res = next_sequence_number - nb_sent;
    while (res < next_sequence_number && segments[res % BUFFER_SIZE].nb_ack) res++;
    return res;
}

void handle_client(int data_desc, RTT_DATA rtt_data) {
    SEGMENT segments[BUFFER_SIZE];
    char ack_buffer[RCVSIZE];
    unsigned int sequence_number = 1;
    unsigned int last_loaded_segment = 0;
    unsigned int p_buff, parsed_p_buff, first_not_ack = 0;
    unsigned int parsed_ack = 0;
    int window = 50;
    int nb_sent;
    int nb_ack;
    ssize_t snd, rcv;
    size_t datagram_size;
    short end;

    struct timeval ack_time;

    do {
        nb_sent = 0;
        do {
            p_buff = sequence_number % BUFFER_SIZE;

            // Load data only if datagram hasn't been loaded yet
            if (sequence_number > last_loaded_segment) {
                // Initializing segment
                segments[p_buff].nb_ack = 0;
                memset(segments[p_buff].data, '\0', RCVSIZE);

                // Loading segment into buffer
                sprintf(segments[p_buff].data, "%06d", sequence_number);
                segments[p_buff].data_size = fread(segments[p_buff].data + HEADER_SIZE, 1, DATA_SIZE, file);
                if (segments[p_buff].data_size == -1) perror("Error reading file\n");
            }

            // End if the message is empty
            end = (segments[p_buff].data_size == 0);

            if (!end) {
                datagram_size = get_datagram_size(segments[p_buff]);

                // Send data to client and get sent time
                snd = send(data_desc, segments[p_buff].data, datagram_size, 0);
                gettimeofday(&(segments[p_buff].snd_time), 0);

                if (snd > 0) {
                    printf("Sent segment %06d\n", sequence_number);
                    nb_sent++;
                    sequence_number++;
                } else {
                    perror("Error sending segment\n");
                    exit(EXIT_FAILURE);
                }
            }
        } while (!end && nb_sent < window);

        if (nb_sent > 0) {
            do {
                // Waiting for ACK
                memset(ack_buffer, '\0', ACK_SIZE + 1);
                rcv = recv(data_desc, ack_buffer, ACK_SIZE, 0);

                if (rcv > 0) {
                    parsed_ack = (unsigned int) atoi(ack_buffer + 3);
                    parsed_p_buff = parsed_ack % BUFFER_SIZE;
                    nb_ack = ++segments[parsed_p_buff].nb_ack;
                    printf("Nb_ack for %d = %d\n", parsed_ack, nb_ack);

                    if (nb_ack == 1) {
                        printf("Received ACK %d", parsed_ack);

                        // Karn's algorithm, updating rto only if no retransmission and no duplicated ACK
                        gettimeofday(&ack_time, 0);
                        rtt_data.rtt = timedifference_usec(segments[parsed_p_buff].snd_time, ack_time);
                        update_rto(&rtt_data);
                        set_timeout(data_desc, 0, rtt_data.rto);
                    } else if (nb_ack >= 3 && parsed_ack < sequence_number) {
                        // Duplicated ACK, resending only if segment in current window
                        printf("Duplicated ACK on %d \n", parsed_ack);
                        snd = send(data_desc, segments[(parsed_ack+1) % BUFFER_SIZE].data,
                                   get_datagram_size(segments[(parsed_ack+1)  % BUFFER_SIZE]), 0);
                        if (snd < 0) {
                            perror("Error resending segment on duplicated ACK");
                            exit(EXIT_FAILURE);
                        }
                    }
                } else {
                    if (errno == EWOULDBLOCK) {
                        // Timeout, resending first non-acknoledged segment in current window
                        first_not_ack = first_non_ack_segment(segments, sequence_number, nb_sent);
                        if (first_not_ack < sequence_number) {
                            printf("Timeout, resending %d\n", first_not_ack);
                            send(data_desc, segments[first_not_ack % BUFFER_SIZE].data,
                                 get_datagram_size(segments[first_not_ack % BUFFER_SIZE]), 0);
                        }
                    } else {
                        perror("Unknown error receiving file\n");
                        exit(EXIT_FAILURE);
                    }
                }

            } while ((parsed_ack + 1) != sequence_number && first_not_ack < sequence_number);
        }

    } while (nb_sent != 0);
}

int main(int argc, char const *argv[]) {
    char buffer[RCVSIZE] = {0};
    uint16_t port;
    RTT_DATA rtt_data;
    int desc;

    signal(SIGTSTP, (__sighandler_t) end_handler);

    // Get public port from argv
    if (argc == 2) {
        errno = 0;
        port = (uint16_t) strtol(argv[1], NULL, 10);
        if (errno != 0) port = 4242;
    } else port = 4242;

    // Create public connection socket
    desc = create_socket(port);

    // Initialize the timeout
    data_desc = my_accept(desc, &rtt_data);
    data_desc_open = TRUE;

    // Closing public connection socket
    close(desc);

    // Waiting for file name on public soc
    if (recv(data_desc, buffer, sizeof(buffer), 0) == -1)
        perror("Error receiving file name\n");

    // Opening file
    file = fopen(buffer, "r");
    if (file == NULL) {
        perror("Error opening file\n");
        end_handler();
    }
    file_open = TRUE;

    handle_client(data_desc, rtt_data);

    end_handler();
    return 0;
}

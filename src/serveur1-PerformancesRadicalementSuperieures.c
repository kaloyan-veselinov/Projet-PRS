// TODO rto initialization

// TODO retransmission doesn't work correctly

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


unsigned int first_non_ack(SEGMENT segments[BUFFER_SIZE],
                           unsigned int next_segment,
                           unsigned int nb_segment_sent) {
    unsigned int res = next_segment - nb_segment_sent;

    while (res < next_segment &&
           segments[res % BUFFER_SIZE].nb_ack)
        res++;
    return (res == next_segment) ? -1 : res;
}

void reinit_sgmt_ack_buff(SEGMENT segments[BUFFER_SIZE],
                          unsigned int next_segment,
                          unsigned int nb_segment_sent) {
    for (int i = next_segment - nb_segment_sent; i < next_segment;
         i++)
        segments[i % BUFFER_SIZE].nb_ack = 0;
}

size_t load_sgmt(FILE *file, char buff[RCVSIZE], int next_segment) {
    memset(buff, '\0', RCVSIZE);
    sprintf(buff, "%06d", next_segment);
    size_t rd = fread(buff + HEADER_SIZE, 1, DATA_SIZE, file);

    if (rd == -1) perror("Error reading file\n");
    return rd;
}

unsigned int send_sgmts(FILE *file,
                        int data_desc,
                        SEGMENT segments[BUFFER_SIZE],
                        unsigned int *last_loaded_segment,
                        unsigned int next_segment,
                        unsigned int cwnd) {
    int p_buff, end;
    size_t datagram_size;
    ssize_t snd;
    unsigned int nb_sent = 0;

    do {
        p_buff = next_segment % BUFFER_SIZE;

        if (next_segment > *last_loaded_segment) {
            segments[p_buff].msg_size = load_sgmt(file, segments[p_buff].data, next_segment);
            *last_loaded_segment = next_segment;
        }

        datagram_size = segments[p_buff].msg_size + (HEADER_SIZE * sizeof(char));

        end = (segments[p_buff].msg_size == 0) && (*last_loaded_segment == next_segment);

        if (!end) {
            snd = send(data_desc, segments[p_buff].data, datagram_size, 0);

            gettimeofday(&segments[p_buff].snd_time, 0);

            if (snd < 0) perror("Error sending segment\n");
            else {
                printf("Sent segment %06d\n", next_segment);
                next_segment++;
                nb_sent++;
            }
        }
    } while (nb_sent < cwnd && !end);
    return nb_sent;
}

int rcv_ack(FILE *file,
            int data_desc,
            SEGMENT segments[BUFFER_SIZE],
            unsigned int *last_loaded_segment,
            unsigned int *next_segment,
            unsigned int *cwnd,
            unsigned int *ssthresh,
            unsigned int nb_sgmt_sent,
            long *srtt,
            long *rttvar) {
    char ack_buffer[ACK_SIZE + 1];
    int nb_rcv_ack;
    unsigned int first_non_ack_sgmt, parsed_ack = 0;
    ssize_t rcv;
    long rtt, rto;
    struct timeval ack_time;
    unsigned int max_rcv_ack = 0;
    int timeout = FALSE;
    int selective_ack = 0;

    reinit_sgmt_ack_buff(segments, *next_segment, nb_sgmt_sent);

    while (parsed_ack != (*next_segment - 1) && !timeout) {
        memset(ack_buffer,
               '\0', ACK_SIZE + 1);
        rcv = recv(data_desc, ack_buffer, ACK_SIZE, 0);

        // Timeout
        if (rcv < 0) {
            if (selective_ack) {
                *next_segment = max_rcv_ack + 1;
                timeout = TRUE;
            } else {
                first_non_ack_sgmt = first_non_ack(segments, *next_segment, nb_sgmt_sent);
                send_sgmts(file, data_desc, segments, last_loaded_segment, first_non_ack_sgmt, 1);
                *ssthresh = *cwnd / 2;
                *cwnd = 1;
            }
        }

            // No timeout
        else {
            parsed_ack = (unsigned int) atoi(ack_buffer + 3);
            nb_rcv_ack = ++segments[parsed_ack % BUFFER_SIZE].nb_ack;
            if (max_rcv_ack < parsed_ack)
                max_rcv_ack = parsed_ack;

            // Normal ACK
            if (nb_rcv_ack == 1) {
                gettimeofday(&ack_time, 0);
                rtt = timedifference_usec(segments[parsed_ack % BUFFER_SIZE].snd_time, ack_time);
                update_rto(&rto, srtt, &rtt, rttvar);
                set_timeout(data_desc, 0, rto);
                printf("Received ACK %d\n", parsed_ack);

                if (selective_ack) {
                    // Window deflation
                    if (parsed_ack > selective_ack) {
                        *cwnd = *ssthresh;
                    }
                }
            }

                // Selective ACK
            else if (nb_rcv_ack > 1) {
                *ssthresh = *cwnd / 2;
                *cwnd = *ssthresh + nb_rcv_ack;

                if (nb_rcv_ack == 2) {
                    if ((parsed_ack + 1) != *next_segment) {
                        fprintf(stderr,
                                "Selective ACK on segment %d\n", parsed_ack);
                        send_sgmts(file, data_desc, segments, last_loaded_segment, parsed_ack + 1, 1);
                        selective_ack = parsed_ack;
                    }
                }
            }
        }
    }

    if (*cwnd < *ssthresh) *cwnd *= 2;
    else *cwnd += 1;

    return 0;
}

void handle_client(int data_desc, long srtt, long rttvar) {
    SEGMENT segments[BUFFER_SIZE];
    unsigned int next_segment = 1;
    unsigned int cwnd = 1;
    unsigned int last_loaded_sgmt = 0;
    unsigned int ssthresh = 42;
    unsigned int nb_sgmt_sent;

    set_timeout(data_desc, 0, 100000);

    do {
        nb_sgmt_sent = send_sgmts(file,
                                  data_desc,
                                  segments,
                                  &last_loaded_sgmt,
                                  next_segment,
                                  cwnd);

        next_segment += nb_sgmt_sent;

        if (nb_sgmt_sent > 0) {
            rcv_ack(file,
                    data_desc,
                    segments,
                    &last_loaded_sgmt,
                    &next_segment,
                    &cwnd,
                    &ssthresh,
                    nb_sgmt_sent,
                    &srtt,
                    &rttvar);

        }
    } while (nb_sgmt_sent != 0);
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

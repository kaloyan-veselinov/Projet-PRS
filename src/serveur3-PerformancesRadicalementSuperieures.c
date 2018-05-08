#include <sys/wait.h>
#include "socket.h"

int desc, data_desc;
FILE *file;
pid_t pid;

int data_desc_open = FALSE;
int file_open = FALSE;
int desc_open = FALSE;

void end_handler() {
    fprintf(stderr, "%d entered end_handler\n", getpid());
    if (pid == 0) {
        send_disconnect_message(data_desc);

        if (data_desc_open) close(data_desc);

        if (file_open) fclose(file);
    } else {
        if (desc_open) close(desc);

        // On attend la fin de tous les processus fils
        int status = 0;
        while ((wait(&status)) > 0);
    }
    exit(EXIT_SUCCESS);
}

void handle_client(int data_desc, RTT_DATA rtt_data) {
    SEGMENT segment;
    char ack_buffer[RCVSIZE];
    unsigned int sequence_number = 1;
    ssize_t snd, rcv;
    size_t datagram_size;
    short end;
    short retransmission;
    unsigned int parsed_ack = 0;
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

            // First transmission of this message
            retransmission = FALSE;

            if (snd > 0) {
                printf("Sent segment %06d\n", sequence_number);

                do {
                    // Waiting for ACK
                    memset(ack_buffer, '\0', ACK_SIZE + 1);
                    rcv = recv(data_desc, ack_buffer, ACK_SIZE, 0);

                    if (rcv > 0) {
                        parsed_ack = (unsigned int) atoi(ack_buffer + 3);
                        printf("Received ACK %d\n", parsed_ack);

                        if (parsed_ack == sequence_number) {
                            segment.nb_ack++;

                            // Karn's algorithm, updating rto only if no retransmission
                            if (!retransmission) {
                                gettimeofday(&ack_time, 0);
                                rtt_data.rtt = timedifference_usec(segment.snd_time, ack_time);
                                update_rto(&rtt_data);
                                set_timeout(data_desc, 0, rtt_data.rto);
                            }
                        }
                        else{
                            send(data_desc, segment.data, datagram_size, 0);
                            retransmission = TRUE;
                        }
                    } else {
                        if (errno == EWOULDBLOCK) {
                            // Timeout, resending segment
                            send(data_desc, segment.data, datagram_size, 0);
                            retransmission = TRUE;
                        } else {
                            perror("Unknown error receiving file\n");
                            exit(EXIT_FAILURE);
                        }
                    }

                } while (segment.nb_ack == 0);

                // Increment sequence number for next segment
                sequence_number++;
            } else perror("Error sending segment\n");
        }
    } while (!end);
}

int main(int argc, char const *argv[]) {
    char buffer[RCVSIZE] = {0};
    uint16_t port;
    RTT_DATA rtt_data;

    signal(SIGTSTP, (__sighandler_t) end_handler);

    // Get public port from argv
    if (argc == 2) {
        errno = 0;
        port = (uint16_t) strtol(argv[1], NULL, 10);
        if (errno != 0) port = 4242;
    } else port = 4242;

    // Create public connection socket
    desc = create_socket(port);
    desc_open = TRUE;

    for (;;) {
        data_desc = my_accept(desc, &rtt_data);
        data_desc_open = TRUE;

        // Creating data-handling child process
        pid = fork();
        if (pid == 0) {
            if (desc_open) close(desc);

            // Waiting for file name on data socket
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
        } else {
            if (data_desc_open) close(data_desc);
        }
    }
}

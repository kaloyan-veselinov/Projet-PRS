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

size_t get_datagram_size(SEGMENT segment) {
    return segment.data_size + (HEADER_SIZE * sizeof(char));
}

int data_desc;

SEGMENT segments[BUFFER_SIZE];
unsigned int sequence_number = 1;
unsigned int window = 10;
unsigned int nb_sent;

pthread_mutex_t mutex;
pthread_cond_t send_cond;

void *send_thread(void *args) {
    unsigned int last_loaded_segment = 0;
    unsigned int p_buff;
    ssize_t snd;
    size_t datagram_size;
    short end;
    unsigned int local_sequence_number;
    unsigned int local_window;
    unsigned int local_nb_sent;
    do {
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&send_cond, &mutex);
        local_sequence_number = sequence_number;
        local_window = window;
        local_nb_sent = 0;
        pthread_mutex_unlock(&mutex);

        do {
            p_buff = local_nb_sent % BUFFER_SIZE;

            pthread_mutex_lock(&mutex);

            // Load data only if datagram hasn't been loaded yet
            if (local_sequence_number > last_loaded_segment) {
                // Initializing segment
                segments[p_buff].nb_ack = 0;
                memset(segments[p_buff].data, '\0', RCVSIZE);

                // Loading segment into buffer
                sprintf(segments[p_buff].data, "%06d", local_sequence_number);
                segments[p_buff].data_size = fread(segments[p_buff].data + HEADER_SIZE, 1, DATA_SIZE, file);
                if (segments[p_buff].data_size == -1) perror("Error reading file\n");
                last_loaded_segment = local_sequence_number;
            }

            // End if the message is empty
            end = (segments[p_buff].data_size == 0);

            if (!end) {
                datagram_size = get_datagram_size(segments[p_buff]);

                // Send data to client and get sent time
                snd = send(data_desc, segments[p_buff].data, datagram_size, 0);
                gettimeofday(&(segments[p_buff].snd_time), 0);

                if (snd > 0) {
                    printf("Sent segment %06d\n", local_sequence_number);
                    local_nb_sent++;
                    local_sequence_number++;
                } else {
                    perror("Error sending segment\n");
                    exit(EXIT_FAILURE);
                }
            }

            pthread_mutex_unlock(&mutex);

        } while (!end && local_nb_sent < local_window);

    } while (local_nb_sent != 0);
    pthread_exit(NULL);
}

void *ack_thread(void *args) {
    char ack_buffer[RCVSIZE];
    unsigned int parsed_p_buff;
    unsigned int max_sent_sequence_nb;
    unsigned int parsed_ack = 0;
    unsigned int max_acknoledged_segment = 0;
    unsigned int local_nb_sent;
    int nb_ack;
    ssize_t snd, rcv;
    RTT_DATA rtt_data;
    struct timeval ack_time;

    // Initializing timer on data socket
    rtt_data.srtt = 100000;
    rtt_data.rttvar = 0;
    set_timeout(data_desc, 0, 100000);

    do {
        pthread_mutex_lock(&mutex);
        local_nb_sent = nb_sent;
        max_sent_sequence_nb = sequence_number;
        pthread_mutex_unlock(&mutex);

        do {
            // Waiting for ACK
            memset(ack_buffer, '\0', ACK_SIZE + 1);
            rcv = recv(data_desc, ack_buffer, ACK_SIZE, 0);

            if (rcv > 0) {
                parsed_ack = (unsigned int) atoi(ack_buffer + 3);
                parsed_p_buff = parsed_ack % BUFFER_SIZE;

                pthread_mutex_lock(&mutex);
                nb_ack = ++segments[parsed_p_buff].nb_ack;
                pthread_mutex_unlock(&mutex);

                if (nb_ack == 1) {
                    printf("Received ACK %d\n", parsed_ack);

                    if (parsed_ack > max_acknoledged_segment) max_acknoledged_segment = parsed_ack;

                    // Karn's algorithm, updating rto only if no retransmission and no duplicated ACK
                    gettimeofday(&ack_time, 0);

                    pthread_mutex_lock(&mutex);
                    rtt_data.rtt = timedifference_usec(segments[parsed_p_buff].snd_time, ack_time);
                    pthread_mutex_unlock(&mutex);

                    update_rto(&rtt_data);
                    set_timeout(data_desc, 0, rtt_data.rto);
                } else if (nb_ack >= 3 && parsed_ack < sequence_number) {
                    // Duplicated ACK, resending only if segment in current window
                    printf("Duplicated ACK on %d \n", parsed_ack);

                    pthread_mutex_lock(&mutex);
                    snd = send(data_desc, segments[(parsed_ack + 1) % BUFFER_SIZE].data,
                               get_datagram_size(segments[(parsed_ack + 1) % BUFFER_SIZE]), 0);
                    pthread_mutex_unlock(&mutex);

                    if (snd < 0) {
                        perror("Error resending segment on duplicated ACK");
                        exit(EXIT_FAILURE);
                    }
                }
            } else {
                if (errno == EWOULDBLOCK) {
                    // Timeout, resending window starting at first non-ack segment
                    printf("Timeout, restarting at %d\n", max_acknoledged_segment + 1);

                    pthread_mutex_lock(&mutex);
                    sequence_number = max_acknoledged_segment + 1;
                    pthread_cond_signal(&send_cond);
                    pthread_mutex_unlock(&mutex);

                } else {
                    perror("Unknown error receiving file\n");
                    exit(EXIT_FAILURE);
                }
            }

        } while ((max_acknoledged_segment + 1) < max_sent_sequence_nb);

        pthread_cond_signal(&send_cond);

    } while (local_nb_sent != 0);
    pthread_exit(NULL);
}

void handle_client(int desc) {
    pthread_t snd;
    pthread_t ack;

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&send_cond, NULL);

    data_desc = desc;

    if (pthread_create(&snd, NULL, send_thread, NULL) != 0) {
        perror("Error creating send thread");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&ack, NULL, ack_thread, NULL) != 0) {
        perror("Error creating ack thread");
        exit(EXIT_FAILURE);
    }

    pthread_join(snd, NULL);
    pthread_join(ack, NULL);
    end_handler();
}


int main(int argc, char const *argv[]) {
    char buffer[RCVSIZE] = {0};
    uint16_t port;

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
        data_desc = my_accept(desc);
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

            handle_client(data_desc);

            end_handler();
        } else {
            if (data_desc_open) close(data_desc);
        }
    }
}

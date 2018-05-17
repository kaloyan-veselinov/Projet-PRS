#include <sys/wait.h>
#include "socket.h"

int desc, data_desc;
FILE *file;
pid_t pid;

int data_desc_open = FALSE;
int file_open = FALSE;
int desc_open = FALSE;

struct sockaddr_in addr;
socklen_t addrlen = sizeof(addr);

void end_handler() {
    fprintf(stderr, "%d entered end_handler\n", getpid());
    if (pid == 0) {
        send_disconnect_message(data_desc, addr, addrlen);

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

int nb_segment;

SEGMENT segments[BUFFER_SIZE];
unsigned int sequence_number = 1;
unsigned int max_ack = 0;
unsigned int window = 30;

pthread_mutex_t mutex;

void * send_thread(void *pVoid) {
    unsigned int last_loaded_segment = 0;
    unsigned int p_buff;
    ssize_t snd;
    size_t datagram_size;
    unsigned int local_sequence_number;
    unsigned int local_window;
    unsigned int local_nb_sent;
    unsigned int local_max_ack;
    short snd_end;

    do {
        pthread_mutex_lock(&mutex);
        local_sequence_number = max_ack + 1;
        local_window = window;
        local_nb_sent = 0;
        local_max_ack = max_ack;
        pthread_mutex_unlock(&mutex);

        do {
            p_buff = local_sequence_number % BUFFER_SIZE;

            pthread_mutex_lock(&mutex);

            // Load data only if datagram hasn't been loaded yet
            if (local_sequence_number > last_loaded_segment) {
                segments[p_buff].nb_ack = 0;

                // Initializing buffer
                memset(segments[p_buff].data, '\0', RCVSIZE);

                // Loading sequence number into buffer
                sprintf(segments[p_buff].data, "%06d", local_sequence_number);

                // Loading data into buffer
                segments[p_buff].data_size = fread(segments[p_buff].data + HEADER_SIZE, 1, DATA_SIZE, file);
                if (segments[p_buff].data_size == -1) perror("Error reading file\n");
                last_loaded_segment = local_sequence_number;
            }

            // End if the message is empty
            snd_end = (segments[p_buff].data_size == 0);

            if (!snd_end) {
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

        } while (!snd_end && local_nb_sent < local_window);

    } while (local_max_ack < nb_segment);
    fprintf(stderr, "Exiting send thread\n");
    pthread_exit(NULL);
}

void * ack_thread(void *pVoid) {
    char ack_buffer[RCVSIZE];
    unsigned int parsed_p_buff;
    unsigned int parsed_ack = 0;
    unsigned int local_max_ack = 0;
    int nb_ack;
    ssize_t rcv;

    do {
        // Waiting for ACK
        memset(ack_buffer, '\0', ACK_SIZE + 1);
        rcv = recv(data_desc, ack_buffer, ACK_SIZE, 0);

        if (rcv > 0) {
            parsed_ack = (unsigned int) atoi(ack_buffer + 3);
            parsed_p_buff = parsed_ack % BUFFER_SIZE;
            printf("Received ACK %d\n", parsed_ack);

            pthread_mutex_lock(&mutex);
            nb_ack = ++segments[parsed_p_buff].nb_ack;
            pthread_mutex_unlock(&mutex);

            if (parsed_ack > local_max_ack) {
                local_max_ack = parsed_ack;

                pthread_mutex_lock(&mutex);
                max_ack = local_max_ack;
                pthread_mutex_unlock(&mutex);
            }

            if (nb_ack >= 3 && parsed_ack < sequence_number) {
                // Duplicated ACK, resending only if segment in current window
                printf("Duplicated ACK on %d \n", parsed_ack);
                pthread_mutex_lock(&mutex);
                max_ack = local_max_ack;
                pthread_mutex_unlock(&mutex);
            }
        } else {
            perror("Unknown error receiving file\n");
            exit(EXIT_FAILURE);
        }

    } while (local_max_ack < nb_segment);
    fprintf(stderr, "Exiting ack thread\n");
    pthread_exit(NULL);
}

void handle_client(int desc) {
    char buffer[RCVSIZE] = {0};
    ssize_t file_size;

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

    fseek(file, 0L, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);
    nb_segment = (int) ceil(((double)file_size) / DATA_SIZE) ;
    fprintf(stderr, "segments to send %d\n", nb_segment);


    pthread_t snd;
    pthread_t ack;

    pthread_mutex_init(&mutex, NULL);

    set_timeout(desc, 0, 0);

    data_desc = desc;

    if (pthread_create(&snd, NULL, send_thread, NULL) != 0) {
        perror("Error creating send thread");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&ack, NULL, ack_thread, NULL) != 0) {
        perror("Error creating ack thread");
        exit(EXIT_FAILURE);
    }

    pthread_join(ack, NULL);
    pthread_join(snd, NULL);
    fprintf(stderr, "Threads joined\n");
    end_handler();
}

int main(int argc, char const *argv[]) {
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
        data_desc = my_accept(desc, &addr, &addrlen);
        data_desc_open = TRUE;

        // Creating data-handling child process
        pid = fork();
        if (pid == 0) {
            if (desc_open) close(desc);

            handle_client(data_desc);

            end_handler();
        } else {
            if (data_desc_open) close(data_desc);
        }
    }
}

#include "socket.h"

int data_desc;
int data_desc_open = FALSE;

struct sockaddr_in addr;
socklen_t addrlen = sizeof(addr);

FILE *file;
int file_open = FALSE;

int nb_segment;

SEGMENT segments[BUFFER_SIZE];
unsigned int max_ack = 0;

#define WINDOW 40

void end_handler() {
    fprintf(stderr, "%d entered end_handler\n", getpid());

    // Sending FIN

    send_disconnect_message(data_desc, addr, addrlen);
    fprintf(stderr, "%d send FIN message\n", getpid());

    // Closing data socket and file
    if (data_desc_open) close(data_desc);
    fprintf(stderr, "%d closed data desc\n", getpid());
    if (file_open) fclose(file);
    fprintf(stderr, "%d closed file, exiting now\n", getpid());

    exit(EXIT_SUCCESS);
}

size_t get_datagram_size(size_t data_size) {
    return data_size + (HEADER_SIZE * sizeof(char));
}

void send_thread() {
    unsigned int last_loaded_segment = 0;
    unsigned int p_buff;
    ssize_t snd;
    unsigned int local_sequence_number;
    unsigned int local_nb_sent;
    unsigned int local_max_ack = 0;

    while (local_max_ack < nb_segment) {
        local_max_ack = max_ack;
        local_sequence_number = local_max_ack + 1;
        local_nb_sent = 0;

        while (local_sequence_number <= nb_segment && local_nb_sent < WINDOW) {
            p_buff = local_sequence_number % BUFFER_SIZE;

            // Load data only if datagram hasn't been loaded yet
            if (local_sequence_number > last_loaded_segment) {
                // Loading sequence number into buffer
                sprintf(segments[p_buff].data, "%06d", local_sequence_number);

                // Loading data into buffer
                segments[p_buff].data_size = get_datagram_size(
                        fread(segments[p_buff].data + HEADER_SIZE, 1, DATA_SIZE, file));
                last_loaded_segment = local_sequence_number;
            }

            snd = send(data_desc, segments[p_buff].data, segments[p_buff].data_size, 0);

            if (snd > 0) {
                local_nb_sent++;
                local_sequence_number++;
            } else {
                perror("Error sending segment\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void *ack_thread() {
    char ack_buffer[ACK_SIZE + 1];
    unsigned int parsed_ack = 0;
    unsigned int local_max_ack = 0;
    ssize_t rcv;
    unsigned last_ack = 0;

    while (local_max_ack < nb_segment) {
        memset(ack_buffer, 0, ACK_SIZE + 1);
        rcv = recv(data_desc, ack_buffer, ACK_SIZE, 0);

        if (rcv > 0) {
            errno = 0;
            parsed_ack = (unsigned int) strtol(ack_buffer + 3, NULL, 10);
            if (errno != 0) perror("Error parsing ack");

            if (parsed_ack > local_max_ack) {
                local_max_ack = parsed_ack;

                max_ack = local_max_ack;
            }
            if (parsed_ack == last_ack) {
                local_max_ack = parsed_ack;
                max_ack = local_max_ack;
            }
            last_ack = parsed_ack;
        } else {
            perror("Unknown error receiving file\n");
            exit(EXIT_FAILURE);
        }
    }
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
    nb_segment = (int) ceil(((double) file_size) / DATA_SIZE);

    pthread_t ack;

    data_desc = desc;

    if (pthread_create(&ack, NULL, (void *(*)(void *)) ack_thread, NULL) != 0) {
        perror("Error creating ack thread");
        exit(EXIT_FAILURE);
    }

    send_thread();

    end_handler();
}

int main(int argc, char const *argv[]) {
    uint16_t port;
    int desc;

    signal(SIGTSTP, (__sighandler_t) end_handler);

    // Get public port from argv
    if (argc == 2) {
        errno = 0;
        port = (uint16_t) strtol(argv[1], NULL, 10);
        if (errno != 0) {
            perror("Error parsing port\n");
            exit(EXIT_FAILURE);
        }
    } else {
        perror("Numero de port non-fourni\n");
        exit(EXIT_FAILURE);
    }

    // Create public connection socket
    desc = create_socket(port);

    // Initialize the timeout
    data_desc = my_accept(desc, &addr, &addrlen);
    data_desc_open = TRUE;

    // Closing public connection socket
    close(desc);

    handle_client(data_desc);

    return 0;
}

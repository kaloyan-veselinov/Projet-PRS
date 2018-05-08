#include "socket.h"

int random_generator_initialized = 0;

struct sockaddr_in init_addr(uint16_t port, uint32_t addr) {
    struct sockaddr_in my_addr;

    memset((char *) &my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = htonl(addr);

    return my_addr;
}

int create_socket(uint16_t port) {
    struct sockaddr_in adresse = init_addr(port, INADDR_ANY);
    int desc = socket(AF_INET, SOCK_DGRAM, 0);
    int valid = 1;
    if (desc < 0) {
        perror("cannot create socket\n");
        return -1;
    }
    setsockopt(desc, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));
    my_bind(desc, (struct sockaddr *) &adresse);
    return desc;
}

int my_bind(int socket, struct sockaddr *addr) {
    if (bind(socket, addr, sizeof(*addr)) < 0) {
        perror("Erreur de bind de la socket\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

uint16_t random_value(uint16_t min, uint16_t max) {
    if (!random_generator_initialized) {
        srand((unsigned int) time(NULL));
        random_generator_initialized = TRUE;
    }
    return (uint16_t) (rand() % (max - min) + min);
}

uint16_t random_port() {
    return random_value(1025, 9999);
}

int my_accept(int desc, RTT_DATA *rtt_data) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    char msg[RCVSIZE];
    struct timeval snd_time, rcv_time;
    int data_desc;
    uint16_t port;

    // Waiting for connection message
    memset(msg, '\0', RCVSIZE);
    memset(&addr, 0, addr_len);
    if (recvfrom(desc, msg, SYN_SIZE, 0, (struct sockaddr *) &addr, &addr_len) == -1) {
        perror("Error receiving on connection socket");
        return EXIT_FAILURE;
    }
    printf("SYN received: %s \n", msg);
    if (strncmp("SYN", msg, SYN_SIZE) != 0) {
        perror("Mauvais message de connexion\n");
        return EXIT_FAILURE;
    }

    // Generating ephemeral data port
    do {
        port = random_port();
        data_desc = create_socket(port);
    } while (data_desc == -1);
    printf("Ephemeral data port: %d\n", port);

    // Sending SYN-ACK
    memset(msg, '\0', RCVSIZE);
    sprintf(msg, "SYN-ACK%04d", port);
    sendto(desc, msg, strlen(msg) + 1, 0, (struct sockaddr *) &addr, addr_len);
    gettimeofday(&snd_time, 0);

    // Waiting for ACK of SYN-ACK
    memset(msg, '\0', RCVSIZE);
    if (recvfrom(desc, msg, RCVSIZE, 0, (struct sockaddr *) &addr,
                 &addr_len) == -1) {
        perror("Erreur de réception du ACK de connexion\n.");
        return EXIT_FAILURE;
    }
    gettimeofday(&rcv_time, 0);
    if (strncmp("ACK", msg, strlen("ACK")) != 0) {
        perror("Mauvais message d'ACK de connexion.\n");
        EXIT_FAILURE;
    }
    perror("ACK reçu, connexion établie.\n");

    // Caching the receivers' socket
    connect(data_desc, (struct sockaddr *) &addr, addr_len);

    // Initializing timer on data socket
    rtt_data->rtt = timedifference_usec(snd_time, rcv_time);
    rtt_data->srtt = 100000;
    rtt_data->rttvar = 0;
    update_rto(rtt_data);
    set_timeout(data_desc, 0, rtt_data->rto);

    return data_desc;
}

long timedifference_usec(struct timeval t0, struct timeval t1) {
    return (t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_usec - t0.tv_usec);
}

void update_rto(RTT_DATA *rtt_data) {
    long delta = rtt_data->rtt - rtt_data->srtt;

    rtt_data->srtt += G * delta;
    rtt_data->rttvar += H * (abs((int) delta) - rtt_data->rttvar);
    rtt_data->rto = rtt_data->srtt + 4 * (rtt_data->rttvar);
}

void set_timeout(int desc, long tv_sec, long tv_usec) {
    struct timeval tv;

    tv.tv_sec = tv_sec;
    tv.tv_usec = tv_usec;
    setsockopt(desc, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof tv);
}

void send_disconnect_message(int data_desc) {
    send(data_desc, "FIN", 4*sizeof(char), 0);
}

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
    if (desc < 0) {
        perror("Cannot create socket\n");
        return -1;
    }

    int valid = 1;
    setsockopt(desc, SOL_SOCKET, SO_REUSEADDR, &valid, sizeof(int));
    return (my_bind(desc, (struct sockaddr *) &adresse)<0)?-1:desc;
}

int my_bind(int socket, struct sockaddr *addr) {
    if (bind(socket, addr, sizeof(*addr)) < 0) {
        perror("Erreur de bind de la socket\n");
        return -1;
    }
    return 0;
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

int my_accept(int desc, struct sockaddr_in *addr, socklen_t *addrlen) {
    char msg[RCVSIZE];
    int data_desc;
    uint16_t port;

    // Waiting for connection message
    memset(msg, '\0', RCVSIZE);
    if (recvfrom(desc, msg, SYN_SIZE, 0, (struct sockaddr *) addr, addrlen) < 0) {
        perror("Error receiving on connection socket");
        return -1;
    }
#if DEBUG
    printf("SYN received: %s \n", msg);
#endif

    // Generating ephemeral data port
    do {
        port = random_port();
        data_desc = create_socket(port);
    } while (data_desc == -1);

#if DEBUG
    printf("Ephemeral data port: %d\n", port);
#endif

    // Sending SYN-ACK
    char synAck[12] = {0};
    sprintf(synAck, "SYN-ACK%04d", port);
    if(sendto(desc, synAck, strlen(synAck)+1, 0, (struct sockaddr *)addr,*addrlen)<0){
        perror("Error sending SYN-ACK\n");
        return -1;
    }

    // Waiting for ACK of SYN-ACK
    memset(msg, '\0', RCVSIZE);
    if (recvfrom(desc, msg, 4, 0, (struct sockaddr *) addr, addrlen) < 0) {
        perror("Erreur de réception du ACK de connexion\n.");
        return -1;
    }

#if DEBUG
    printf("ACK reçu\n");
#endif

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

void send_disconnect_message(int data_desc, struct sockaddr_in *addr) {
    ssize_t snd;
    do{
        snd = sendto(data_desc, "FIN", 4*sizeof(char), 0, (struct sockaddr *) addr, sizeof(*addr));
        if(snd < 0) perror("Error sending FIN\n");
    } while(snd != 0);
}

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

    // Caching the receivers' socket
    connect(data_desc, (struct sockaddr *) addr, *addrlen);

    return data_desc;
}

int disconnect_udp_sock(int fd) {
    struct sockaddr_in sin;

    memset((char *)&sin, 0, sizeof(sin));
    sin.sin_family = AF_UNSPEC;
    return (connect(fd, (struct sockaddr *)&sin, sizeof(sin)));
}

void set_timeout(int desc, long tv_sec, long tv_usec) {
    struct timeval tv;

    tv.tv_sec = tv_sec;
    tv.tv_usec = tv_usec;
    setsockopt(desc, SOL_SOCKET, SO_RCVTIMEO, (const char *) &tv, sizeof tv);
}

void send_disconnect_message(int data_desc, struct sockaddr_in addr, socklen_t addrlen) {
    disconnect_udp_sock(data_desc);
    set_timeout(data_desc, 1,0);
    ssize_t snd, rcv;
    char buff[ACK_SIZE+1]={0};

    for(;;){
        snd = sendto(data_desc, "FIN", 4*sizeof(char), 0, (struct sockaddr *) &addr, addrlen);
        if(snd == 0) perror("Error sending FIN\n");
        rcv = recvfrom(data_desc, buff, ACK_SIZE, 0, (struct sockaddr *) &addr, &addrlen);
        if (rcv<0 && errno==EWOULDBLOCK) break;
    }
}
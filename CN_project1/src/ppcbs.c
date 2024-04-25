#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "server_tcp.h"
#include "server_udp_udpr.h"
#include "server_udp.h"
#include "common.h"
#include "err.h"


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fatal("usage: %s <protocol> <port>", argv[0]);
    }

    signal(SIGPIPE, SIG_IGN);

    const char* protocol = argv[1];
    uint16_t port = read_port(argv[2]);
    int socket_fd = -1;
    uint8_t protocol_id;

    if (strcmp(protocol, tcp) == 0) {
        protocol_id = tcp_protocol_id;
        socket_fd = initialize_tcp_socket(port);
        printf("TCP server is running on port %d\n", port);
        handle_tcp_connection(socket_fd, protocol_id);
        printf("TCP server ended port %d\n", port);
    }
    else if (strcmp(protocol, udp) == 0) {
        protocol_id = udp_protocol_id;
        socket_fd = initialize_udp_socket(port);
        printf("UDP server is running on port %d\n", port);
        handle_udp_connection(socket_fd, protocol_id);
    }

    close(socket_fd);
    return 0;
}
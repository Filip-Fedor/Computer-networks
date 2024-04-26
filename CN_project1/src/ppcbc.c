#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "client_tcp.h"
#include "client_udp.h"
#include "client_udpr.h"
#include "common.h"
#include "err.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fatal("Usage: %s <protocol> <server_address> <port>", argv[0]);
    }

    signal(SIGPIPE, SIG_IGN);

    const char *protocol = argv[1];
    int socket_fd;
    uint8_t protocol_id;

    if (strcmp(protocol, tcp) == 0) {
        protocol_id = tcp_protocol_id;
        socket_fd = initialize_tcp_connection(argv[2], argv[3]);
        handle_tcp_client(socket_fd, protocol_id);
        close(socket_fd);
    }
    else if (strcmp(protocol, udp) == 0) {
        protocol_id = udp_protocol_id;
        udp_connection udp_conn = initialize_udp_connection(argv[2], argv[3]);
        handle_udp_client(udp_conn, protocol_id);
        close(udp_conn.socket_fd);
    }
    else if (strcmp(protocol, udpr) == 0) {
        protocol_id = udpr_protocol_id;
        udp_connection udp_conn = initialize_udp_connection(argv[2], argv[3]);
        handle_udpr_client(udp_conn, protocol_id);
        close(udp_conn.socket_fd);
    }
    else {
        error("wrong protocol");
    }

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#include "server_udp_udpr.h"
#include "server_udp.h"
#include "server_udpr.h"
#include "session.h"
#include "common.h"
#include "err.h"
#include "protconst.h"

int initialize_udp_socket(uint16_t port) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    if (bind(socket_fd, (struct sockaddr *)&server_address, 
            sizeof(server_address)) < 0)

        syserr("bind");
    
    return socket_fd;
}

void handle_udp_connection(int socket_fd, uint8_t protocol_id) {
    Session session_active;
    memset(&session_active, 0, sizeof(session_active));

    while (1) {
        remove_socket_timeout(socket_fd);
        CONN_packet conn_packet;
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        if (recv_conn_packet(socket_fd, &client_addr, &client_addr_len,
                            &conn_packet) == EXIT_COMMUNICATION) {

            continue;
        }

        set_socket_timeout(socket_fd, MAX_WAIT);

        protocol_id = conn_packet.protocol_id;
        session_active.client_addr = client_addr;
        session_active.addr_len = client_addr_len;
        session_active.session_id = conn_packet.session_id;
        uint64_t data_len = be64toh(conn_packet.data_len);

        if (protocol_id == udp_protocol_id) {
            if (send_conacc_packet(socket_fd, &client_addr, &client_addr_len,
                                    session_active.session_id) == 
                EXIT_COMMUNICATION) {

                continue;
            }

            if (recv_data_packets(socket_fd, &session_active, data_len) == 
                EXIT_COMMUNICATION) {

                continue;
            }

            send_rcvd_packet(socket_fd, &client_addr, &client_addr_len, 
                            session_active.session_id);

        }
        else {
            size_t first_data_packet_len_read = 0;

            if (send_conacc_packet_and_recv_data_packet(socket_fd, &client_addr,
                                    &client_addr_len, session_active.session_id, 
                                    &first_data_packet_len_read, data_len) == 
                EXIT_COMMUNICATION) {

                continue;
            }

            data_len -= first_data_packet_len_read;

            if (send_acc_packet_and_recv_data_packet(socket_fd, 
                                    &session_active, data_len) == 
                EXIT_COMMUNICATION) {

                continue;
            }

            send_rcvd_packet(socket_fd, &client_addr, &client_addr_len, 
                            session_active.session_id);
        }
    }
}

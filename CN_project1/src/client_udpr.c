#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>

#include "client_udp.h"
#include "common.h"
#include "err.h"
#include "protconst.h"
#include "client_udpr.h"


int recv_conacc_packet_retr(int socket_fd, struct sockaddr_in *server_addr, 
                    uint64_t session_id) {

    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    char buffer[sizeof(CONACC_packet)];

    while (1) {
        ssize_t recv_len = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                                    (struct sockaddr *)&from_addr, &from_len);
        
        if (recv_len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                error("timeout");
                return TIMEOUT_COMMUNICATION;
            }
            error("recvfrom CONACC packet");
            return EXIT_COMMUNICATION;
        }

        if (from_addr.sin_addr.s_addr != server_addr->sin_addr.s_addr ||
            from_addr.sin_port != server_addr->sin_port) {

            continue;
        }

        if (recv_len != sizeof(CONACC_packet)) {
            error("bad CONACC packet size");
            return EXIT_COMMUNICATION;
        }

        CONACC_packet *conacc_packet = (CONACC_packet *)buffer;

        if (conacc_packet->packet_type == conrjt_packet_type) {
            return EXIT_COMMUNICATION;
        }

        if (conacc_packet->packet_type != conacc_packet_type) {
            error("bad CONACC packet type");
            return EXIT_COMMUNICATION;
        }

        if (conacc_packet->session_id != session_id) {
            error("bad CONACC session id");
            return EXIT_COMMUNICATION;
        }

        break;
    }        

    return OK_COMMUNICATION;
}

int send_conn_packet_and_recv_conacc_retr(int socket_fd, 
                        struct sockaddr_in *server_addr, 
                        socklen_t *server_addr_len, uint64_t session_id, 
                        uint8_t protocol_id, size_t data_len_read) {

    CONN_packet conn_packet;
    conn_packet.packet_type = conn_packet_type;
    conn_packet.session_id = session_id;
    conn_packet.protocol_id = protocol_id;
    conn_packet.data_len = htobe64(data_len_read);

    int attempts = 0;
    bool conacc_received = false;

    while (attempts <= MAX_RETRANSMITS && !conacc_received) {

        if (sendto(socket_fd, &conn_packet, sizeof(conn_packet), 0,
                (struct sockaddr *)server_addr, *server_addr_len) < 0) {
            error("sendto CONN packet");
            return EXIT_COMMUNICATION;
        }

        int result_recv_conacc = recv_conacc_packet_retr(socket_fd, 
                                            server_addr, session_id);
                                            
        if (result_recv_conacc == OK_COMMUNICATION) {
            conacc_received = true;
        }
        else if (result_recv_conacc == TIMEOUT_COMMUNICATION) {
            attempts++;
        }
        else {
            return EXIT_COMMUNICATION;
        }
    }

    if (!conacc_received) {
        error("recv CONACC packet, tried MAX_RETR times");
        return EXIT_COMMUNICATION;
    }

    return OK_COMMUNICATION;
}


int recv_acc_packet_retr(int socket_fd, struct sockaddr_in *server_addr, 
                    uint64_t session_id, uint64_t packet_number) {

    char buffer[sizeof(ACC_packet)];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    while (1) {

        ssize_t recv_len = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                                    (struct sockaddr *)&from_addr, &from_len);

        if (recv_len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                error("timeout");
                return TIMEOUT_COMMUNICATION;
            }
            error("recvfrom ACC packet");
            return EXIT_COMMUNICATION;
        }

        if (from_addr.sin_addr.s_addr != server_addr->sin_addr.s_addr ||
            from_addr.sin_port != server_addr->sin_port) {

            continue;
        }

        if (packet_number == first_packet_number && 
            recv_len == sizeof(CONACC_packet)) {

            CONACC_packet *conacc_packet = (CONACC_packet *)buffer;

            if (conacc_packet->packet_type == conacc_packet_type) {
                continue;
            }
        }

        if (recv_len != sizeof(ACC_packet)) {
            error("bad ACC packet size");
            return EXIT_COMMUNICATION;
        }

        ACC_packet *acc_packet = (ACC_packet *)buffer;

        if (acc_packet->packet_type != acc_packet_type) {
            error("bad ACC packet type");
            return EXIT_COMMUNICATION;
        }

        if (acc_packet->session_id != session_id) {
            error("bad ACC session id");
            return EXIT_COMMUNICATION;
        }

        if (be64toh(acc_packet->packet_number) < packet_number) {
            continue;
        }

        if (be64toh(acc_packet->packet_number) != packet_number) {
            error("bad ACC packet number");
            return EXIT_COMMUNICATION;
        }

        break;
    }                            

    return OK_COMMUNICATION;
}


int send_data_packets_and_recv_acc_retr(int socket_fd, 
                            struct sockaddr_in *dest_addr,
                            socklen_t *addr_len, uint64_t session_id, 
                            size_t data_len_read, char *data) {
                                   
    uint64_t packet_number = first_packet_number;
    size_t offset = 0;

    while (offset < data_len_read) {
        ssize_t chunk_size = (data_len_read - offset > BUFFER_SIZE) ? 
                                BUFFER_SIZE : (data_len_read - offset);
        DATA_header header;

        header.packet_type = data_packet_type;
        header.session_id = session_id;
        header.packet_number = htobe64(packet_number);
        header.data_bytes_len = htonl(chunk_size);
        char *data_sending = data + offset;

        char buffer[sizeof(header) + chunk_size];
        memcpy(buffer, &header, sizeof(header));
        memcpy(buffer + sizeof(header), data_sending, chunk_size);

        int attempts = 0;
        bool acc_received = false;

        while (attempts <= MAX_RETRANSMITS && !acc_received) {

            if (sendto(socket_fd, buffer, sizeof(header) + chunk_size, 0, 
                        (struct sockaddr *)dest_addr, *addr_len) < 0) {
                error("sendto DATA packet");
                return EXIT_COMMUNICATION;
            }

            int result_recv_acc = recv_acc_packet_retr(socket_fd, dest_addr,
                                                    session_id, packet_number);
            if (result_recv_acc == OK_COMMUNICATION) {
                acc_received = true;
            }
            else if (result_recv_acc == TIMEOUT_COMMUNICATION) {
                attempts++;
            }
            else {
                return EXIT_COMMUNICATION;
            }
        }

        if (!acc_received) {
            error("recv ACC packet, tried MAX_RETR times");
            return EXIT_COMMUNICATION;
        }

        offset += chunk_size;
        packet_number++;
    }

    return OK_COMMUNICATION;
}

int recv_rcvd_packet_with_retr(int socket_fd, udp_connection *udp_conn, 
                                uint64_t session_id) {

    char buffer[sizeof(ACC_packet)];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    while (1) {

        ssize_t recv_len = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                                    (struct sockaddr *)&from_addr, &from_len);
        
        if (recv_len < 0) {
            error("recvfrom RCVD packet");
            return EXIT_COMMUNICATION;
        }

        if ((from_addr.sin_addr.s_addr != 
                udp_conn->server_addr.sin_addr.s_addr) ||
            (from_addr.sin_port != udp_conn->server_addr.sin_port)) {

            continue;
        }

        if (recv_len == sizeof(ACC_packet)) {
            
            ACC_packet *acc_packet = (ACC_packet *)buffer;
            if (acc_packet->packet_type == acc_packet_type) {
                continue;
            }
        }

        if (recv_len != sizeof(RCVD_packet)) {
            error("bad RCVD packet size");
            return EXIT_COMMUNICATION;
        }

        RCVD_packet *rcvd_packet = (RCVD_packet *)buffer;

        if (rcvd_packet->packet_type != rcvd_packet_type) {
            error("bad RCVD packet type");
            return EXIT_COMMUNICATION;
        }

        if (rcvd_packet->session_id != session_id) {
            error("bad RCVD session id");
            return EXIT_COMMUNICATION;
        }

        break;
    }

    return OK_COMMUNICATION;
}


void handle_udpr_client(udp_connection udp_conn, uint8_t protocol_id) {
    size_t total_read = 0;
    char *data = read_data_from_stdin(&total_read);
    socklen_t server_addr_len = sizeof(udp_conn.server_addr);

    set_socket_timeout(udp_conn.socket_fd, MAX_WAIT);

    uint64_t session_id = rand_session_id_generate();

    if (send_conn_packet_and_recv_conacc_retr(udp_conn.socket_fd, 
                            &udp_conn.server_addr, &server_addr_len, 
                            session_id, protocol_id, total_read) == 
        EXIT_COMMUNICATION) {
            
        free(data);
        return;
    }

    if (send_data_packets_and_recv_acc_retr(udp_conn.socket_fd, 
                                    &udp_conn.server_addr, &server_addr_len,
                                    session_id, total_read, data) == 
        EXIT_COMMUNICATION) {

        free(data);
        return;
    }

    if (recv_rcvd_packet_with_retr(udp_conn.socket_fd, &udp_conn, 
                                    session_id) ==
        EXIT_COMMUNICATION) {

        free(data);
        return;
    }

    free(data);
}
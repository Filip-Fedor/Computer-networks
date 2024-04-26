#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "server_udp.h"
#include "session.h"
#include "common.h" 
#include "err.h"
#include "protconst.h"


int recv_conn_packet(int socket_fd, struct sockaddr_in *client_addr, 
                        socklen_t *client_addr_len, CONN_packet *conn_packet) {

    char buffer[sizeof(CONN_packet)];

    ssize_t received_len = recvfrom(socket_fd, buffer,
                                    sizeof(buffer), 0,
                                    (struct sockaddr *)client_addr, 
                                    client_addr_len);
    if (received_len < 0) {
        error("recvfrom CONN packet");
        return EXIT_COMMUNICATION;
    }

    if (received_len != sizeof(CONN_packet)) {
        error("bad CONN packet size");
        return EXIT_COMMUNICATION;
    }

    memcpy(conn_packet, buffer, received_len);

    if (conn_packet->packet_type != conn_packet_type) {
        error("bad CONN packet type");
        return EXIT_COMMUNICATION;
    }

    if (conn_packet->protocol_id != udp_protocol_id && 
        conn_packet->protocol_id != udpr_protocol_id) {

        error("bad CONN protocol id");
        return EXIT_COMMUNICATION;
    }

    return OK_COMMUNICATION;
}

int send_conrjt_packet(int socket_fd, struct sockaddr_in *client_addr, 
                        socklen_t *client_addr_len, uint64_t session_id) {

    CONRJT_packet conrjt_packet;
    conrjt_packet.packet_type = conrjt_packet_type;
    conrjt_packet.session_id = session_id;

    if (sendto(socket_fd, &conrjt_packet, sizeof(conrjt_packet), 0,
               (struct sockaddr *)client_addr, *client_addr_len) < 0) {

        error("sendto CONRJT packet");
        return EXIT_COMMUNICATION;
    }

    return OK_COMMUNICATION;
}

int send_conacc_packet(int socket_fd, struct sockaddr_in *client_addr, 
                        socklen_t *client_addr_len, uint64_t session_id) {
    
    CONACC_packet conacc_packet;
    conacc_packet.packet_type = conacc_packet_type;
    conacc_packet.session_id = session_id;

    if (sendto(socket_fd, &conacc_packet, sizeof(conacc_packet), 0,
               (struct sockaddr *)client_addr, *client_addr_len) < 0) {

        error("sendto CONACC packet");
        return EXIT_COMMUNICATION;
    }

    return OK_COMMUNICATION;
}

void send_rjt_packet(int socket_fd, struct sockaddr_in *client_addr, 
                        socklen_t *client_addr_len, uint64_t session_id, 
                        uint64_t packet_number) {

    RJT_packet rjt_packet;
    rjt_packet.packet_type = rjt_packet_type;
    rjt_packet.session_id = session_id;
    rjt_packet.packet_number = htobe64(packet_number);


    if (sendto(socket_fd, &rjt_packet, sizeof(rjt_packet), 0,
               (struct sockaddr *)client_addr, *client_addr_len) < 0) {

        error("sendto RJT packet");
    }
}

int recv_data_packets(int socket_fd, Session *session_active,
                        uint64_t data_len) {

    uint64_t packet_number = 0;
    uint64_t read_size = 0;

    while (read_size < data_len) {
        char buffer[MAX_LEN];
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);

        ssize_t received_len = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                                        (struct sockaddr *)&from_addr, 
                                        &from_len);
        if (received_len < 0) {
            error("recvfrom DATA packet");
            return EXIT_COMMUNICATION;
        }

        if ((from_addr.sin_addr.s_addr != 
            session_active->client_addr.sin_addr.s_addr) ||
            (from_addr.sin_port != session_active->client_addr.sin_port)) {

            if (received_len == sizeof(CONN_packet)) {
                CONN_packet *conn_packet = (CONN_packet *) buffer;

                if (conn_packet->packet_type == conn_packet_type) {
                    send_conrjt_packet(socket_fd, &from_addr, &from_len, 
                                        conn_packet->session_id);
                    continue;
                }
            }
            if (received_len == sizeof(DATA_packet)) {
                DATA_packet *data_packet = (DATA_packet *) buffer;

                if (data_packet->packet_type == data_packet_type) {
                    send_rjt_packet(socket_fd, &from_addr, &from_len,
                                    data_packet->session_id, packet_number);
                    continue;
                }
            }

            continue;
        }

        if (received_len < (ssize_t)sizeof(DATA_header)) {
            send_rjt_packet(socket_fd, &session_active->client_addr, 
                            &session_active->addr_len, 
                            session_active->session_id, packet_number);
            error("bad DATA packet size");
            return EXIT_COMMUNICATION;
        }

        DATA_packet *data_packet = (DATA_packet *)buffer;

        uint64_t packet_number_net = be64toh(data_packet->packet_number);
        uint32_t data_bytes_len_net = ntohl(data_packet->data_bytes_len);

        if (received_len != 
                (ssize_t)(sizeof(DATA_header) + data_bytes_len_net)) {
            send_rjt_packet(socket_fd, &session_active->client_addr, 
                            &session_active->addr_len, 
                            session_active->session_id, packet_number);
            error("bad DATA packet size");
            return EXIT_COMMUNICATION;
        }

        if (read_size + data_bytes_len_net > data_len) {
            send_rjt_packet(socket_fd, &session_active->client_addr, 
                            &session_active->addr_len, 
                            session_active->session_id, packet_number);
            error("bad DATA received len");
            return EXIT_COMMUNICATION;
        }

        if (data_packet->session_id != session_active->session_id) {
            send_rjt_packet(socket_fd, &session_active->client_addr, 
                            &session_active->addr_len, 
                            session_active->session_id, packet_number);
            error("bad DATA session id");
            return EXIT_COMMUNICATION;
        } 

        if (data_packet->packet_type != data_packet_type) {
            send_rjt_packet(socket_fd, &session_active->client_addr, 
                            &session_active->addr_len, 
                            session_active->session_id, packet_number);
            error("bad DATA packet type");
            return EXIT_COMMUNICATION;
        }

        if (packet_number_net != packet_number) {
            send_rjt_packet(socket_fd, &session_active->client_addr,
                            &session_active->addr_len, 
                            session_active->session_id, packet_number);
            error("bad DATA packet number");
            return EXIT_COMMUNICATION;
        }
        
        if (data_bytes_len_net < MIN_DATA_LEN || 
            data_bytes_len_net > MAX_DATA_LEN) {
            send_rjt_packet(socket_fd, &session_active->client_addr, 
                            &session_active->addr_len, 
                            session_active->session_id, packet_number);
            error("bad DATA data len");
            return EXIT_COMMUNICATION;
        }
        data_packet->data[data_bytes_len_net] = '\0';

        printf("%s", data_packet->data);
        fflush(stdout);

        read_size += data_bytes_len_net;
        packet_number++;
    }

    return OK_COMMUNICATION;
}


void send_rcvd_packet(int socket_fd, struct sockaddr_in *client_addr, 
                        socklen_t *client_addr_len, uint64_t session_id) {

    RCVD_packet rcvd_packet;
    rcvd_packet.packet_type = rcvd_packet_type;
    rcvd_packet.session_id = session_id;

    if (sendto(socket_fd, &rcvd_packet, sizeof(rcvd_packet), 0,
               (struct sockaddr *)client_addr, *client_addr_len) < 0) {
        error("sendto RCVD packet");
    }
}
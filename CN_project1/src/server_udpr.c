#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include "server_udpr.h"
#include "server_udp.h"
#include "session.h"
#include "common.h" 
#include "err.h"
#include "protconst.h"

int recv_data_packet_retr(int socket_fd, struct sockaddr_in *client_addr,
                            socklen_t *client_addr_len, uint64_t session_id,
                            uint64_t expected_packet_number,
                            size_t *read_size, uint64_t data_len) {

    printf("=== waiting for data packet number: %lu\n", expected_packet_number);

    char buffer[MAX_LEN];
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
            error("recvfrom DATA packet");
            return EXIT_COMMUNICATION;
        }

        if (from_addr.sin_addr.s_addr != client_addr->sin_addr.s_addr ||
            from_addr.sin_port != client_addr->sin_port) {

            printf("Rejected packet from unexpected source: expected %s:%d, got %s:%d\n",
           inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port),
           inet_ntoa(from_addr.sin_addr), ntohs(from_addr.sin_port));

            if (recv_len == sizeof(CONN_packet)) {
                CONN_packet *conn_packet = (CONN_packet *) buffer;

                if (conn_packet->packet_type == conn_packet_type) {
                    send_conrjt_packet(socket_fd, &from_addr, &from_len, 
                                        conn_packet->session_id);
                    continue;
                }
            }

            if (recv_len == sizeof(DATA_packet)) {
                DATA_packet *data_packet = (DATA_packet *) buffer;

                send_rjt_packet(socket_fd, &from_addr, &from_len, 
                                data_packet->session_id, expected_packet_number);
                continue;
            }
            error("recvfrom different address");
            continue;
        }
   

        if (expected_packet_number == first_packet_number && 
            recv_len == sizeof(CONN_packet)) {

            CONN_packet *conn_packet = (CONN_packet *)buffer;

            if (conn_packet->packet_type == conn_packet_type) {
                printf("=== received conn packet. So still wait for data packet number: %lu\n", expected_packet_number);
                continue;
            }
        }

        if (recv_len < (ssize_t)sizeof(DATA_header)) {
            send_rjt_packet(socket_fd, client_addr, client_addr_len, 
                            session_id, expected_packet_number);
            error("bad DATA packet size");
            return EXIT_COMMUNICATION;
        }

        DATA_packet *data_packet = (DATA_packet *) buffer;

        uint64_t this_data_packet_number = be64toh(data_packet->packet_number);
        uint32_t data_bytes_len_net = ntohl(data_packet->data_bytes_len);

        printf("=== Received header: type %u, session ID %" PRIu64 ", packet number %lu, data bytes length %u\n",
        data_packet->packet_type, data_packet->session_id, this_data_packet_number, data_bytes_len_net);

        if (recv_len != (ssize_t)(sizeof(DATA_header) + data_bytes_len_net)) {
            send_rjt_packet(socket_fd, client_addr, client_addr_len, 
                            session_id, expected_packet_number);
            error("bad DATA packet size");
            return EXIT_COMMUNICATION;
        } 

        if (data_packet->session_id != session_id) {
            send_rjt_packet(socket_fd, client_addr, client_addr_len, 
                            session_id, expected_packet_number);
            error("bad DATA session id");
            return EXIT_COMMUNICATION;
        }

        if (data_packet->packet_type != data_packet_type) {
            send_rjt_packet(socket_fd, client_addr, client_addr_len, 
                            session_id, expected_packet_number);
            error("bad DATA packet type");
            return EXIT_COMMUNICATION;
        }

        if (data_bytes_len_net + *read_size > data_len) {
            send_rjt_packet(socket_fd, client_addr, client_addr_len, 
                            session_id, expected_packet_number);
            error("bad DATA received len");
            return EXIT_COMMUNICATION;
        }
        
        if (data_bytes_len_net < MIN_DATA_LEN || 
            data_bytes_len_net > MAX_DATA_LEN) {

            send_rjt_packet(socket_fd, client_addr, client_addr_len, 
                            session_id, expected_packet_number);
            error("bad DATA data len");
            return EXIT_COMMUNICATION;
        }

        if (this_data_packet_number + 1 == expected_packet_number) {
            printf("=== Received previous DATA packet number: %lu. Still wait for DATA packet.\n", this_data_packet_number);
            continue;
        }
        
        if (this_data_packet_number != expected_packet_number) {
            send_rjt_packet(socket_fd, client_addr, client_addr_len, 
                            session_id, expected_packet_number);
            error("bad DATA packet number");
            return EXIT_COMMUNICATION;
        }


        printf("=== Received DATA packet %lu: %.*s\n", this_data_packet_number, data_bytes_len_net, data_packet->data);

        *read_size += data_bytes_len_net;

        break;

    }                            

    
    return OK_COMMUNICATION;

}

int send_conacc_packet_and_recv_data_packet(int socket_fd, 
                        struct sockaddr_in *client_addr, 
                        socklen_t *client_addr_len,
                        uint64_t session_id, 
                        size_t *first_data_packet_len_read, 
                        uint64_t data_len) {
    
    CONACC_packet conacc_packet;
    conacc_packet.packet_type = conacc_packet_type;
    conacc_packet.session_id = session_id;


    printf("Starting to write CONACC packet\n");

    int attempts = 0;
    bool data_received = false;
    
    while (attempts < MAX_RETRANSMITS && !data_received) {

        printf("Sending CONACC packet: type %u, session ID %lu.\n", conacc_packet.packet_type, conacc_packet.session_id);

        if (sendto(socket_fd, &conacc_packet, sizeof(conacc_packet), 0,
                (struct sockaddr *)client_addr, *client_addr_len) < 0) {

            error("sendto CONACC packet");
            return EXIT_COMMUNICATION;
        }

        int result_recv_data = recv_data_packet_retr(socket_fd, client_addr,
                                client_addr_len, session_id, first_packet_number, 
                                first_data_packet_len_read, data_len);
        if (result_recv_data == OK_COMMUNICATION) {
            data_received = true;
            printf("FIRST DATA RECEIVED\n");
        }
        else if (result_recv_data == TIMEOUT_COMMUNICATION) {
            printf("Timeout or error occurred, retrasmissing, do not received DATA first packet.\n");
            attempts++;
        }
        else {
            return EXIT_COMMUNICATION;
        }
    }

    if (!data_received) {
        error("recv DATA packet, tried MAX_RETR times");
        return EXIT_COMMUNICATION;
    }

    printf("Ended writing CONACC packet\n");


    return OK_COMMUNICATION;
}

int send_acc_packet_and_recv_data_packet(int socket_fd, Session *session_active,
                                        uint64_t data_len) {

    uint64_t packet_number = first_packet_number;
    uint64_t read_size = 0;

    printf("Starting sending ACC packets...\n");

    bool end = false;
    

    while (!end) {
        ACC_packet acc_packet;
        acc_packet.packet_type = acc_packet_type;
        acc_packet.packet_number = htobe64(packet_number);
        acc_packet.session_id = session_active->session_id;

        int attempts = 0;
        bool data_received = false;

        while (attempts < MAX_RETRANSMITS && !data_received) {
            printf("Sending ACC packet: type %u, session ID %lu, packet number: %lu.\n", acc_packet.packet_type, acc_packet.session_id, packet_number);

            if (sendto(socket_fd, &acc_packet, sizeof(acc_packet), 0,
                        (struct sockaddr *)&session_active->client_addr, 
                        session_active->addr_len) < 0) {

                    error("sendto ACC packet");
                    return EXIT_COMMUNICATION;
            }

            packet_number++;

            printf("siemamamama::::%lu\n", data_len);

            if (read_size == data_len) {
                data_received = true;
                end = true;
                break;
            }
            else {
                int result_recv_data = recv_data_packet_retr(socket_fd,
                                    &session_active->client_addr, 
                                    &session_active->addr_len, 
                                    session_active->session_id, 
                                    packet_number, &read_size, data_len);
                                    
                if (result_recv_data == OK_COMMUNICATION) {
                    data_received = true;
                    printf("DATA PACKET RECEIVED %lu.\n", packet_number);
                }
                else if (result_recv_data == TIMEOUT_COMMUNICATION) {
                    printf("Timeout or error occurred, retrasmissing, do not received DATA packet number %lu.\n", packet_number);
                    packet_number--;
                    attempts++;
                }
                else {
                    return EXIT_COMMUNICATION;
                }
            }
        }
        if (!data_received) {
            error("recv DATA packet, tried MAX_RETR times");
            return EXIT_COMMUNICATION;
        }
    }
    printf("Sended last ACC packet\n");
    return OK_COMMUNICATION;
}
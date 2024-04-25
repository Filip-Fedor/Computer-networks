#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "server_tcp.h"
#include "common.h" 
#include "err.h"
#include "protconst.h"

#define QUEUE_LENGTH 5

int initialize_tcp_socket(uint16_t port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
        syserr("cannot create a socket");

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    if (bind(socket_fd, (struct sockaddr *)&server_address, 
            sizeof(server_address)) < 0)
        syserr("bind");
    if (listen(socket_fd, QUEUE_LENGTH) < 0)
        syserr("listen");

    return socket_fd;
}

int read_conn_packet(int client_fd, CONN_packet *conn_packet, 
                    uint8_t protocol_id) {
    printf("Starting to read CONN packet\n");

    if (readn(client_fd, conn_packet, sizeof(CONN_packet)) != 
                sizeof(CONN_packet)) {

        error("read CONN packet");
        return EXIT_COMMUNICATION;
    }

    if (conn_packet->packet_type != conn_packet_type) {
        error("bad CONN packet type");
        return EXIT_COMMUNICATION;
    }

    if (conn_packet->protocol_id != protocol_id) {
        error("bad CONN protocol id");
        return EXIT_COMMUNICATION;
    }

    printf("CONN PACKET: session_id: %lu, data_len: %lu\n", conn_packet->session_id, be64toh(conn_packet->data_len));

    printf("Ended reading CONN packet\n");

    return OK_COMMUNICATION;
}

int write_conacc_packet(int client_fd, uint64_t session_id) {
    CONACC_packet conacc_packet;
    conacc_packet.packet_type = conacc_packet_type;
    conacc_packet.session_id = session_id;


    printf("Starting to write CONACC packet\n");

    if (writen(client_fd, &conacc_packet, sizeof(CONACC_packet)) != 
                sizeof(CONACC_packet)) {

        error("write CONACC packet");
        return EXIT_COMMUNICATION;
    }


    printf("Ended writing CONACC packet\n");

    return OK_COMMUNICATION;
}

void write_rjt_packet(int client_fd, uint64_t session_id, 
                        uint64_t packet_number) {
    RJT_packet rjt_packet;
    rjt_packet.packet_type = rjt_packet_type;
    rjt_packet.session_id = session_id;
    rjt_packet.packet_number = htobe64(packet_number);

    printf("Starting to write RJT packet\n");

    if (writen(client_fd, &rjt_packet, sizeof(RJT_packet)) != 
                sizeof(RJT_packet)) {

        error("send RJT packet");
    }

    printf("Ended writing RJT packet\n");
}

int read_data_packets(int client_fd, uint64_t data_len, uint64_t session_id) {
    uint64_t packet_number = 0;
    uint64_t read_size = 0;

    printf("Starting to read DATA packets\n");

    while (read_size < data_len) {
        DATA_header header;

        if (readn(client_fd, &header, sizeof(DATA_header)) != 
                sizeof(DATA_header)) {

            error("read DATA packet");
            return EXIT_COMMUNICATION;
        }

        header.packet_number = be64toh(header.packet_number);
        header.data_bytes_len = ntohl(header.data_bytes_len);

        printf("READING header: type %u, session ID %" PRIu64 ", packet number %" PRIu64 ", data bytes length %u\n",
        header.packet_type, header.session_id, header.packet_number, header.data_bytes_len);

        if (header.session_id != session_id) {
            write_rjt_packet(client_fd, header.session_id, packet_number);
            error("bad DATA session id");
            return EXIT_COMMUNICATION;
        }
        
        if (header.packet_number != packet_number) {
            write_rjt_packet(client_fd, header.session_id, packet_number);
            error("bad DATA packet number");
            return EXIT_COMMUNICATION;
        }

        if (header.packet_type != data_packet_type) {
            write_rjt_packet(client_fd, header.session_id, packet_number);
            error("bad DATA packet type");
            return EXIT_COMMUNICATION;
        }

        if (header.data_bytes_len + read_size > data_len) {
            write_rjt_packet(client_fd, header.session_id, packet_number);
            error("bad DATA received length");
            return EXIT_COMMUNICATION;

        }

        if (header.data_bytes_len < MIN_DATA_LEN || 
                header.data_bytes_len > MAX_DATA_LEN) {

            write_rjt_packet(client_fd, header.session_id, packet_number);
            error("bad DATA data len");
            return EXIT_COMMUNICATION;
        }

        char *data = malloc(header.data_bytes_len + 1);
        if (!data) {
            fatal("bad alloc memory for DATA data");
        }

        if (readn(client_fd, data, header.data_bytes_len) != 
                    header.data_bytes_len) {

            error("read DATA data");
            free(data);
            return EXIT_COMMUNICATION;
        }

        data[header.data_bytes_len] = '\0';

        printf("Received packet %lu: %s\n", header.packet_number, data);


        read_size += header.data_bytes_len;
        packet_number++;

        free(data);
    }

    printf("Ended reading DATA packets\n");
    return OK_COMMUNICATION;
}

void write_rcvd_packet(int client_fd, uint64_t session_id) {
    RCVD_packet rcvd_packet;
    rcvd_packet.packet_type = rcvd_packet_type;
    rcvd_packet.session_id = session_id;

    printf("Starting to write RCVD packet\n");

    if (writen(client_fd, &rcvd_packet, sizeof(RCVD_packet)) != 
                sizeof(RCVD_packet)) {

        error("write RCVD packet");
    }

    printf("Ending reading RCVD packet\n");
}

void handle_tcp_connection(int socket_fd, uint8_t protocol_id) {
    while (1) {
        printf("WAITING FOR NEW CONNECTION\n\n");

        struct sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);
        int client_fd = accept(socket_fd, (struct sockaddr *)&client_address, 
                                &client_address_len);
        if (client_fd < 0) {
            error("accept failed");
            continue;
        }

        set_socket_timeout(client_fd, MAX_WAIT);

        CONN_packet conn_packet;
        if (read_conn_packet(client_fd, &conn_packet, protocol_id) ==
                EXIT_COMMUNICATION) {

            close(client_fd);
            continue;
        }

        printf("\n");

        if (write_conacc_packet(client_fd, conn_packet.session_id) ==
                EXIT_COMMUNICATION) {

            close(client_fd);
            continue;
        }

        printf("\n");

        if (read_data_packets(client_fd, be64toh(conn_packet.data_len), 
                                conn_packet.session_id)  == 
                EXIT_COMMUNICATION) {

            close(client_fd);
            continue;
        }

        printf("\n");

        write_rcvd_packet(client_fd, conn_packet.session_id);

        printf("\n");

        close(client_fd);

        printf("END OK\n =================================== \n");
    }
}

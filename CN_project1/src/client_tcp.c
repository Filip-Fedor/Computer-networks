#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#include "common.h"
#include "err.h"
#include "protconst.h"

int initialize_tcp_connection(const char* server_address, 
                                const char* port_str) {

    uint16_t port = read_port(port_str);

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
        syserr("cannot create a socket");

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_address, &server_addr.sin_addr) <= 0) {
        syserr("inet_pton error");
    }

    if (connect(socket_fd, (struct sockaddr *)&server_addr, 
                            sizeof(server_addr)) < 0) {
        syserr("connect");
    }

    return socket_fd;
}


int write_conn_packet(int socket_fd, uint8_t protocol_id, 
                        size_t data_len_read, uint64_t session_id) {

    CONN_packet conn_packet;
    conn_packet.packet_type = conn_packet_type;
    conn_packet.session_id = session_id;
    conn_packet.protocol_id = protocol_id;
    conn_packet.data_len = htobe64(data_len_read);

    if (writen(socket_fd, &conn_packet, sizeof(CONN_packet)) != 
                sizeof(CONN_packet)) {

        error("write CONN packet");
        return EXIT_COMMUNICATION;
    }

    return OK_COMMUNICATION;
}

int read_conacc_packet(int socket_fd, CONACC_packet *conacc_packet, 
                        uint64_t session_id) {

    if (readn(socket_fd, conacc_packet, sizeof(CONACC_packet)) != 
                sizeof(CONACC_packet)) {

        error("read CONACC packet");
        return EXIT_COMMUNICATION;
    }

    if (conacc_packet->session_id != session_id) {
        error("bad CONACC session id");
        return EXIT_COMMUNICATION;
    }

    if (conacc_packet->packet_type != conacc_packet_type) {
        error("bad CONACC packet type");
        return EXIT_COMMUNICATION;
    }

    return OK_COMMUNICATION;
}

int write_data_packets(int socket_fd, uint64_t session_id, 
                        size_t data_len_read, char *data) {

    uint64_t packet_number = 0;
    size_t offset = 0;

    while (offset < data_len_read) {
        ssize_t chunk_size = (data_len_read - offset > BUFFER_SIZE) ?
                                BUFFER_SIZE : (data_len_read - offset);
        DATA_header header;
        header.packet_type = data_packet_type;
        header.session_id = session_id;
        header.packet_number = htobe64(packet_number++);
        header.data_bytes_len = htonl(chunk_size);
        char *data_sending = data + offset;

        if (writen(socket_fd, &header, sizeof(DATA_header)) != 
                    sizeof(DATA_header)) {

            error("write DATA header");
            return EXIT_COMMUNICATION;
        }

       if (writen(socket_fd, data_sending, chunk_size) != chunk_size) {
            error("write DATA data");
            return EXIT_COMMUNICATION;
       }

        offset += chunk_size;
    }

    return OK_COMMUNICATION;
}

void read_rcvd_packet(int socket_fd, uint64_t session_id) {
    RCVD_packet rcvd_packet;

    if (readn(socket_fd, &rcvd_packet, sizeof(RCVD_packet)) != 
                sizeof(RCVD_packet)) {
        error("write RCVD packet");
    }

    if (rcvd_packet.packet_type != rcvd_packet_type) {
        error("bad RCVD packet type");
    }

    if (rcvd_packet.session_id != session_id) {
        error("bad RCVD session id");
    }
}

void handle_tcp_client(int socket_fd, uint8_t protocol_id) {
    size_t total_read = 0;
    char *data = read_data_from_stdin(&total_read);
    uint64_t session_id = rand_session_id_generate();
    
    if (write_conn_packet(socket_fd, protocol_id, total_read, session_id) == 
            EXIT_COMMUNICATION) {

        free(data);
        return;
    }

    set_socket_timeout(socket_fd, MAX_WAIT);

    CONACC_packet conacc_packet;
    if (read_conacc_packet(socket_fd, &conacc_packet, session_id) == 
            EXIT_COMMUNICATION) {

        free(data);
        return;
    }

    if (write_data_packets(socket_fd, session_id, total_read, data) == 
            EXIT_COMMUNICATION) {

        free(data);
        return;
    }

    read_rcvd_packet(socket_fd, session_id);

    free(data);
}

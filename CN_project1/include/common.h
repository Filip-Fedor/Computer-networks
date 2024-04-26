#ifndef COMMON_H
#define COMMON_H

#include <inttypes.h>
#include <stddef.h>
#include <sys/types.h>

#define tcp "tcp"
#define udp "udp"
#define udpr "udpr"

#define BUFFER_SIZE 64000
#define INITIAL_BUFFER_SIZE 4096
#define READ_SIZE 4096

#define MIN_DATA_LEN 1
#define MAX_DATA_LEN 64000

#define MAX_LEN 64100

#define first_packet_number 0

#define TIMEOUT_COMMUNICATION -2
#define EXIT_COMMUNICATION -1
#define OK_COMMUNICATION 0

#define BITS_PER_PART 32

#define conn_packet_type 1
#define conacc_packet_type 2
#define conrjt_packet_type 3
#define data_packet_type 4
#define acc_packet_type 5
#define rjt_packet_type 6
#define rcvd_packet_type 7

#define tcp_protocol_id 1
#define udp_protocol_id 2
#define udpr_protocol_id 3

typedef struct __attribute__((__packed__)) {
    uint8_t packet_type;
    uint64_t session_id;
    uint8_t protocol_id;
    uint64_t data_len;
} CONN_packet;

typedef struct __attribute__((__packed__)) {
    uint8_t packet_type;
    uint64_t session_id;
} CONACC_packet;

typedef CONACC_packet CONRJT_packet;

typedef struct __attribute__((__packed__)) {
    uint8_t packet_type;
    uint64_t session_id;
    uint64_t packet_number;
    uint32_t data_bytes_len;
    char data[];
} DATA_packet;

typedef struct __attribute__((__packed__)) {
    uint8_t packet_type;
    uint64_t session_id;
    uint64_t packet_number;
    uint32_t data_bytes_len;
} DATA_header;

typedef struct __attribute__((__packed__)) {
    uint8_t packet_type;
    uint64_t session_id;
    uint64_t packet_number;
} ACC_packet;

typedef ACC_packet RJT_packet;

typedef CONACC_packet RCVD_packet;

uint16_t read_port(char const *string);
size_t   read_size(char const *string);

struct sockaddr_in get_server_address(char const *host, uint16_t port);

ssize_t	readn(int fd, void *vptr, size_t n);
ssize_t	writen(int fd, const void *vptr, size_t n);

void set_socket_timeout(int socket_fd, int timeout_sec);

void remove_socket_timeout(int socket_fd);

char *read_data_from_stdin(size_t *total_read);

uint64_t rand_session_id_generate();

#endif // COMMON_H

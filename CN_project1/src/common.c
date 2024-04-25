#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include "err.h"
#include "common.h"

uint16_t read_port(char const *string) {
    char *endptr;
    unsigned long port = strtoul(string, &endptr, 10);
    if ((port == ULONG_MAX && errno == ERANGE) || *endptr != 0 ||
        port == 0 || port > UINT16_MAX) {
        fatal("%s is not a valid port number", string);
    }
    return (uint16_t) port;
}

size_t read_size(char const *string) {
    char *endptr;
    unsigned long long number = strtoull(string, &endptr, 10);
    if ((number == ULLONG_MAX && errno == ERANGE) || *endptr != 0 ||
        number > SIZE_MAX) {
        fatal("%s is not a valid number", string);
    }
    return number;
}

struct sockaddr_in get_server_address(char const *host, uint16_t port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *address_result;
    int errcode = getaddrinfo(host, NULL, &hints, &address_result);
    if (errcode != 0) {
        fatal("getaddrinfo: %s", gai_strerror(errcode));
    }

    struct sockaddr_in send_address;
    send_address.sin_family = AF_INET;   // IPv4
    send_address.sin_addr.s_addr =       // IP address
        ((struct sockaddr_in *) (address_result->ai_addr))->sin_addr.s_addr;
    send_address.sin_port = htons(port); // port from the command line

    freeaddrinfo(address_result);

    return send_address;
}

// Following two functions come from Stevens' "UNIX Network Programming" book.

// Read n bytes from a descriptor. 
// Use in place of read() when fd is a stream socket.
ssize_t readn(int fd, void *vptr, size_t n) {
    ssize_t nleft, nread;
    char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0)
            return nread;     // When error, return < 0.
        else if (nread == 0)
            break;            // EOF

        nleft -= nread;
        ptr += nread;
    }
    return n - nleft;         // return >= 0
}

// Write n bytes to a descriptor.
ssize_t writen(int fd, const void *vptr, size_t n){
    ssize_t nleft, nwritten;
    const char *ptr;

    ptr = vptr;               // Can't do pointer arithmetic on void*.
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
            return nwritten;  // error

        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;
}

void set_socket_timeout(int socket_fd, int timeout_sec) {
    struct timeval timeout;
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;

    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, 
        (char *)&timeout, sizeof(timeout)) < 0) {
        syserr("setsockopt failed");
    }
}

void remove_socket_timeout(int socket_fd) {
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, 
        (char *)&timeout, sizeof(timeout)) < 0) {
        syserr("setsockopt failed to remove SO_RCVTIMEO");
    }
}

char *read_data_from_stdin(size_t *total_read) {
    size_t current_size = INITIAL_BUFFER_SIZE;
    char *data = malloc(current_size);
    if (data == NULL) {
        syserr("bad alloc memory");
    }

    *total_read = 0;

    while (!feof(stdin)) {
        if (*total_read >= current_size) {
            current_size *= 2; 
            char *new_data = realloc(data, current_size);
            if (new_data == NULL) {
                free(data);
                syserr("failed to reallocate memory");
            }

            data = new_data;
        }

        size_t bytes_read = fread(data + *total_read, 1, READ_SIZE, stdin);
        if (ferror(stdin)) {
            free(data);
            syserr("failed reading from stdin");
        }
        *total_read += bytes_read;
    }

    data[*total_read] = '\0';
    return data;
}

uint64_t rand_session_id_generate() {
    srand(time(NULL));
    uint64_t high_part = rand(); 
    uint64_t low_part = rand();

    uint64_t session_id = (high_part << BITS_PER_PART) | low_part;

    return session_id;
}

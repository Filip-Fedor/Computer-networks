#ifndef SERVER_TCP_H
#define SERVER_TCP_H

#include <stdint.h>

int initialize_tcp_socket(uint16_t port);
void handle_tcp_connection(int socket_fd, uint8_t protocol_id);

#endif // SERVER_TCP_H
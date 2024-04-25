#ifndef CLIENT_TCP_H
#define CLIENT_TCP_H

#include <stdint.h>
#include <stddef.h>

int initialize_tcp_connection(const char* server_address, const char* port_str);
void handle_tcp_client(int socket_fd, uint8_t protocol_id);

#endif // CLIENT_TCP_H
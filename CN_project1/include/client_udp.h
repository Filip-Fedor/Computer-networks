#ifndef CLIENT_UDP_H
#define CLIENT_UDP_H

#include <stdint.h>
#include <netinet/in.h>

typedef struct {
    int socket_fd;
    struct sockaddr_in server_addr;
} udp_connection;

udp_connection initialize_udp_connection(const char *server_address, 
                                            const char *port_str);
                                            
void handle_udp_client(udp_connection udp_conn, uint8_t protocol_id);

#endif // CLIENT_UDP_H
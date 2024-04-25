#ifndef SERVER_UDP_UDPR_H
#define SERVER_UDP_UDPR_H

int initialize_udp_socket(uint16_t port);
void handle_udp_connection(int socket_fd, uint8_t protocol_id);

#endif // SERVER_UDP_UDPR_H
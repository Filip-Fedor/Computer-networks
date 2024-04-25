#ifndef SERVER_UDP_H
#define SERVER_UDP_H

#include <netinet/in.h>
#include <session.h>
#include <common.h>

int recv_conn_packet(int socket_fd, struct sockaddr_in *client_addr,
                        socklen_t *client_addr_len, CONN_packet *conn_packet);

int send_conrjt_packet(int socket_fd, struct sockaddr_in *client_addr, 
                        socklen_t *client_addr_len, uint64_t session_id);

int send_conacc_packet(int socket_fd, struct sockaddr_in *client_addr,
                        socklen_t *client_addr_len, uint64_t session_id);

void send_rjt_packet(int socket_fd, struct sockaddr_in *client_addr, 
                        socklen_t *client_addr_len, uint64_t session_id, 
                        uint64_t packet_number);

int recv_data_packets(int socket_fd, Session *session_active, 
                        uint64_t data_len);

void send_rcvd_packet(int socket_fd, struct sockaddr_in *client_addr, 
                        socklen_t *client_addr_len, uint64_t session_id);

#endif // SERVER_UDP_H
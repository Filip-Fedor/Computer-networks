#ifndef SERVER_UDPR_H
#define SERVER_UDPR_H

#include <netinet/in.h>
#include <session.h>

int recv_data_packet_retr(int socket_fd, struct sockaddr_in *client_addr,
                            socklen_t *client_addr_len, uint64_t session_id,
                            uint64_t expected_packet_number, 
                            size_t *first_data_packet_len_read, 
                            uint64_t data_len);
                            
int send_conacc_packet_and_recv_data_packet(int socket_fd, 
                            struct sockaddr_in *client_addr, 
                            socklen_t *client_addr_len, uint64_t session_id, 
                            size_t *first_data_packet_len_read, 
                            uint64_t data_len);

int send_acc_packet_and_recv_data_packet(int socket_fd, Session *session_active,
                            uint64_t data_len);

#endif // SERVER_UDPR_H
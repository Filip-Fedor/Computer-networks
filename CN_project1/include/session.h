#ifndef SESSION_H
#define SESSION_H

#include <netinet/in.h>

typedef struct {
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    uint64_t session_id;
} Session;

#endif // SESSION_H
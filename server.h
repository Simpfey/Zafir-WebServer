#ifndef SERVER_H
#define SERVER_H

#include <openssl/ssl.h>

typedef struct {
    int client_fd;
    SSL *ssl;
} client_conn_t;

void run_server(int port);

#endif

#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "handler.h"
#include "https.h"
#include "server.h"
#include "config.h"

SSL_CTX *ssl_ctx = NULL;

void run_server(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 128) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        struct timeval timeout = { .tv_sec = 5, .tv_usec = 0 };
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        SSL *ssl = NULL;
        if (enable_https) {
            ssl = SSL_new(ssl_ctx);
            if (!ssl) {
                fprintf(stderr, "SSL_new failed\n");
                close(client_fd);
                continue;
            }

            SSL_set_fd(ssl, client_fd);

            if (SSL_accept(ssl) <= 0) {
                ERR_print_errors_fp(stderr);
                SSL_free(ssl);
                close(client_fd);
                continue;
            }
        }

        client_conn_t *conn = malloc(sizeof(client_conn_t));
        if (!conn) {
            perror("malloc failed");
            if (ssl) {
                SSL_shutdown(ssl);
                SSL_free(ssl);
            }
            close(client_fd);
            continue;
        }

        conn->client_fd = client_fd;
        conn->ssl = ssl;

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, conn) != 0) {
            perror("pthread_create failed");
            if (ssl) {
                SSL_shutdown(ssl);
                SSL_free(ssl);
            }
            close(client_fd);
            free(conn);
            continue;
        }

        pthread_detach(thread);
    }

    close(server_fd);
}

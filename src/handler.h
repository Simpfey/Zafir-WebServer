#ifndef HANDLER_H
#define HANDLER_H

ssize_t recv_data(int fd, SSL *ssl, void *buf, size_t len);
ssize_t send_data(int fd, SSL *ssl, const void *buf, size_t len);
void* handle_client(void* arg);

#endif

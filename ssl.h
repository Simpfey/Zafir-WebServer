#ifndef SSL_H
#define SSL_H

#include <openssl/ssl.h>

extern SSL_CTX *ssl_ctx;

int init_ssl_context(SSL_CTX **ctx, const char *cert_file, const char *key_file);

#endif
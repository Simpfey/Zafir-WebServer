#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>
#include "log.h"

int init_ssl_context(SSL_CTX **ctx, const char *cert_file, const char *key_file) {
    *ctx = SSL_CTX_new(TLS_server_method());
    if (*ctx == NULL) {
        log_message(LOG_ERROR, "Failed to create SSL context");
        ERR_print_errors_fp(stderr);
        return 0;
    }

    if (access(cert_file, R_OK) != 0) {
        log_message(LOG_ERROR, "Certificate file '%s' not found or not readable", cert_file);
        SSL_CTX_free(*ctx);
        *ctx = NULL;
        return 0;
    }

    if (SSL_CTX_use_certificate_file(*ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
        log_message(LOG_ERROR, "Failed to load certificate file '%s'", cert_file);
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(*ctx);
        *ctx = NULL;
        return 0;
    }

    if (access(key_file, R_OK) != 0) {
        log_message(LOG_ERROR, "Key file '%s' not found or not readable", key_file);
        SSL_CTX_free(*ctx);
        *ctx = NULL;
        return 0;
    }

    if (SSL_CTX_use_PrivateKey_file(*ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        log_message(LOG_ERROR, "Failed to load private key file '%s'", key_file);
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(*ctx);
        *ctx = NULL;
        return 0;
    }

    if (!SSL_CTX_check_private_key(*ctx)) {
        log_message(LOG_ERROR, "Private key does not match the certificate public key");
        SSL_CTX_free(*ctx);
        *ctx = NULL;
        return 0;
    }

    return 1;
}
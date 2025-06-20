#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>
#include <strings.h>
#include <ctype.h>
#include "cache.h"
#include "mime.h"
#include "ratelimit.h"
#include "log.h"
#include "header.h"
#include "path.h"
#include "gzip.h"
#include "config.h"
#include "server.h"
#include "error.h"

#define MAX_BUFFER 8192
#define MAX_HEADER 1024
#define MAX_PATH 512

static ssize_t recv_data(int fd, SSL *ssl, void *buf, size_t len) {
    return ssl ? SSL_read(ssl, buf, len) : recv(fd, buf, len, 0);
}

static ssize_t send_data(int fd, SSL *ssl, const void *buf, size_t len) {
    return ssl ? SSL_write(ssl, buf, len) : send(fd, buf, len, 0);
}

void* handle_client(void* arg) {
    client_conn_t *conn = (client_conn_t *)arg;
    int client_fd = conn->client_fd;
    SSL *ssl = conn->ssl;
    free(conn);

    int printed = 0;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getpeername(client_fd, (struct sockaddr*)&addr, &len);
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));

    if (is_rate_limited(ip)) {
        log_message(LOG_WARN, "Rate-limited %s", ip);
        send_error_response(client_fd, 429, "Too Many Requests");
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
        close(client_fd);
        return NULL;
    }

    char buffer[MAX_BUFFER];
    ssize_t received;
    size_t buf_used = 0;
    int max_requests = 100, requests_handled = 0;

    while (requests_handled++ < max_requests) {
        received = recv_data(client_fd, ssl, buffer + buf_used, sizeof(buffer) - buf_used - 1);
        if (received <= 0) break;

        printed = 0;
        buf_used += received;
        buffer[buf_used] = '\0';

        char* header_end;
        while ((header_end = strstr(buffer, "\r\n\r\n"))) {
            size_t header_len = header_end - buffer + 4;

            http_request_t req;
            if (parse_http_request(buffer, &req) != 0) {
                send_error_response(client_fd, 400, "Bad Request");
                if (ssl) {
                    SSL_shutdown(ssl);
                    SSL_free(ssl);
                }
                close(client_fd);
                return NULL;
            }

            if (strcmp(req.method, "GET") != 0 && strcmp(req.method, "HEAD") != 0) {
                send_error_response(client_fd, 405, "Method Not Allowed");
                if (ssl) {
                    SSL_shutdown(ssl);
                    SSL_free(ssl);
                }
                close(client_fd);
                return NULL;
            }

            int is_head = strcmp(req.method, "HEAD") == 0;

            if (!is_valid_path(req.path)) {
                log_message(LOG_WARN, "Rejected unsafe path from %s: %s", ip, req.path);
                send_error_response(client_fd, 400, "Bad Request");
                if (ssl) {
                    SSL_shutdown(ssl);
                    SSL_free(ssl);
                }
                close(client_fd);
                return NULL;
            }

            const char* conn_hdr = get_header(&req, "Connection");
            int keep_alive = (conn_hdr && strcasecmp(conn_hdr, "keep-alive") == 0) || strcmp(req.version, "HTTP/1.1") == 0;

            const char* ae_hdr = get_header(&req, "Accept-Encoding");
            int accepts_gzip = ae_hdr && strstr(ae_hdr, "gzip");

            char filepath[MAX_PATH];
            const char* mapped = resolve_rewrite(req.path);
            const char *relative_path = mapped ? mapped : (strcmp(req.path, "/") == 0 ? "index.html" : req.path + 1);

            if (is_protected(relative_path)) {
                send_error_response(client_fd, 403, "Forbidden");
                close(client_fd);
                return NULL;
            }

            snprintf(filepath, sizeof(filepath), "public/%s", relative_path);

            CacheNode* cached = get_from_cache(filepath);
            char* content = NULL;
            size_t content_length = 0;
            int status_code = 200;
            struct stat st;

            if (cached) {
                content = malloc(cached->size);
                if (!content) {
                    cached = NULL;
                } else {
                    memcpy(content, cached->content, cached->size);
                    content_length = cached->size;
                    stat(filepath, &st);
                }
            }

            if (!cached) {
                int file_fd = open(filepath, O_RDONLY);
                if (file_fd < 0 || fstat(file_fd, &st) < 0) {
                    file_fd = open("public/404.html", O_RDONLY);
                    fstat(file_fd, &st);
                    status_code = 404;
                }

                off_t range_start = 0, range_end = st.st_size - 1;
                int has_range = parse_range_header(get_header(&req, "Range"), st.st_size, &range_start, &range_end);
                content_length = range_end - range_start + 1;

                content = malloc(content_length);
                if (!content || pread(file_fd, content, content_length, range_start) != content_length) {
                    close(file_fd);
                    if (content) free(content);
                    if (ssl) {
                        SSL_shutdown(ssl);
                        SSL_free(ssl);
                    }
                    close(client_fd);
                    return NULL;
                }
                close(file_fd);
                if (status_code == 200) add_to_cache(filepath, content, content_length);
                if (has_range) status_code = 206;
            }

            const char* mime = get_mime_type(filepath);

            char* compressed_content = NULL;
            size_t compressed_length = 0;

            if (accepts_gzip && content_length > 512 && gzip_compress(content, content_length, &compressed_content, &compressed_length)) {
                free(content);
                content = compressed_content;
                content_length = compressed_length;
            }

            char header[MAX_HEADER];
            snprintf(header, sizeof(header),
                "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nServer: Zafir/1.0\r\nContent-Length: %zu\r\n\r\n",
                status_code, status_code == 200 ? "OK" : "Partial Content",
                mime, content_length);

            send_data(client_fd, ssl, header, strlen(header));

            if (!is_head) {
                send_data(client_fd, ssl, content, content_length);
            }

            free(content);

            if (printed == 0) {
                log_message(LOG_INFO, "Served %s %d to %s%s", filepath, status_code, ip, keep_alive ? " (keep-alive)" : "");
                printed++;
            }

            memmove(buffer, buffer + header_len, buf_used - header_len);
            buf_used -= header_len;

            if (!keep_alive) {
                if (ssl) {
                    SSL_shutdown(ssl);
                    SSL_free(ssl);
                }
                close(client_fd);
                return NULL;
            }
        }
    }

    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }
    close(client_fd);
    return NULL;
}

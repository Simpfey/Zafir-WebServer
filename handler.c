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
#include <zlib.h>
#include "cache.h"
#include "mime.h"
#include "ratelimit.h"
#include "log.h"

#define MAX_BUFFER 8192
#define MAX_HEADER 1024
#define MAX_PATH 512


int is_valid_path(const char* path) {
    if (strstr(path, "..")) return 0;
    for (int i = 0; path[i]; i++) {
        if ((unsigned char)path[i] < 32 || (unsigned char)path[i] > 126) return 0;
    }
    return 1;
}

int parse_range_header(const char* header, off_t file_size, off_t* start, off_t* end) {
    if (!header) return 0;
    const char* range_str = strstr(header, "bytes=");
    if (!range_str) return 0;
    range_str += strlen("bytes=");

    char* dash = strchr(range_str, '-');
    if (!dash) return 0;

    char start_str[32] = {0};
    char end_str[32] = {0};

    strncpy(start_str, range_str, dash - range_str);
    strcpy(end_str, dash + 1);

    *start = atoll(start_str);
    if (strlen(end_str) > 0) {
        *end = atoll(end_str);
    } else {
        *end = file_size - 1;
    }

    if (*start > *end || *end >= file_size) return 0;
    return 1;
}

int client_accepts_gzip(const char* headers) {
    const char* accept_enc = strcasestr(headers, "Accept-Encoding:");
    if (!accept_enc) return 0;
    const char* val = accept_enc + strlen("Accept-Encoding:");
    while (*val == ' ' || *val == '\t') val++;
    if (strstr(val, "gzip")) return 1;
    return 0;
}

int gzip_compress(const char* input, size_t input_len, char** output, size_t* output_len) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return 0;
    }

    zs.next_in = (Bytef*)input;
    zs.avail_in = input_len;

    size_t buf_size = input_len + 1024;
    *output = malloc(buf_size);
    if (!*output) {
        deflateEnd(&zs);
        return 0;
    }

    zs.next_out = (Bytef*)*output;
    zs.avail_out = buf_size;

    int ret = deflate(&zs, Z_FINISH);
    if (ret != Z_STREAM_END) {
        free(*output);
        deflateEnd(&zs);
        return 0;
    }

    *output_len = buf_size - zs.avail_out;
    deflateEnd(&zs);
    return 1;
}

void* handle_client(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getpeername(client_fd, (struct sockaddr*)&addr, &len);
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));

    if (is_rate_limited(ip)) {
        log_message(LOG_WARN, "Rate-limited %s", ip);
        const char* msg = "HTTP/1.1 429 Too Many Requests\r\nContent-Length: 0\r\nServer: Zafir/1.0\r\nConnection: close\r\n\r\n";
        send(client_fd, msg, strlen(msg), 0);
        close(client_fd);
        return NULL;
    }

    char buffer[MAX_BUFFER] = {0};
    ssize_t received;
    size_t buf_used = 0;

    int max_requests = 100;
    int requests_handled = 0;

    while (requests_handled < max_requests) {
        received = recv(client_fd, buffer + buf_used, sizeof(buffer) - buf_used - 1, 0);
        if (received <= 0) {
            break;
        }
        buf_used += received;
        buffer[buf_used] = '\0';

        while (1) {
            char* header_end = strstr(buffer, "\r\n\r\n");
            if (!header_end) break;

            size_t header_len = header_end - buffer + 4;

            char* method_end = strchr(buffer, ' ');
            if (!method_end || method_end >= header_end) break;

            size_t method_len = method_end - buffer;
            if (strncmp(buffer, "GET", method_len) != 0) {
                const char* resp = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length:0\r\nConnection: close\r\n\r\n";
                send(client_fd, resp, strlen(resp), 0);
                close(client_fd);
                return NULL;
            }

            char* path_start = method_end + 1;
            char* path_end = strchr(path_start, ' ');
            if (!path_end || path_end >= header_end) break;
            size_t path_len = path_end - path_start;
            char path[MAX_PATH];
            if (path_len >= MAX_PATH) path_len = MAX_PATH - 1;
            memcpy(path, path_start, path_len);
            path[path_len] = '\0';

            if (!is_valid_path(path)) {
                log_message(LOG_WARN, "Rejected unsafe path from %s: %s", ip, path);
                const char* msg = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                send(client_fd, msg, strlen(msg), 0);
                close(client_fd);
                return NULL;
            }

            int keep_alive = 0;
            char* conn_hdr = strcasestr(buffer, "Connection:");
            if (conn_hdr && conn_hdr < header_end) {
                char* val = conn_hdr + strlen("Connection:");
                while (*val == ' ' || *val == '\t') val++;
                if (strncasecmp(val, "keep-alive", 10) == 0) {
                    keep_alive = 1;
                }
            } else {
                if (strstr(buffer, "HTTP/1.1")) {
                    keep_alive = 1;
                }
            }

            int accepts_gzip = client_accepts_gzip(buffer);

            off_t range_start = 0, range_end = -1;
            int has_range = 0;
            char* range_hdr = strcasestr(buffer, "Range:");
            if (range_hdr && range_hdr < header_end) {
                has_range = parse_range_header(range_hdr, 0, &range_start, &range_end);
            }

            char filepath[MAX_PATH];
            if (strcmp(path, "/") == 0) {
                snprintf(filepath, sizeof(filepath), "public/index.html");
            } else {
                snprintf(filepath, sizeof(filepath), "public/%s", path + 1);
            }

            int file_fd = open(filepath, O_RDONLY);
            struct stat st;
            if (file_fd == -1 || fstat(file_fd, &st) == -1) {
                log_message(LOG_ERROR, "File %s not found, serving 404", filepath);
                file_fd = open("public/404.html", O_RDONLY);
                fstat(file_fd, &st);
                const char* mime = "text/html";

                char header[MAX_HEADER];
                snprintf(header, sizeof(header),
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: %ld\r\n"
                    "Server: Zafir/1.0\r\n"
                    "Connection: %s\r\n\r\n",
                    mime, (long)st.st_size,
                    keep_alive ? "keep-alive" : "close");

                send(client_fd, header, strlen(header), 0);
                sendfile(client_fd, file_fd, NULL, st.st_size);
                close(file_fd);

                memmove(buffer, buffer + header_len, buf_used - header_len);
                buf_used -= header_len;

                if (!keep_alive) {
                    close(client_fd);
                    return NULL;
                } else {
                    requests_handled++;
                    continue;
                }
            }

            if (has_range) {
                if (!parse_range_header(range_hdr, st.st_size, &range_start, &range_end)) {
                    has_range = 0;
                }
            }

            size_t content_length = st.st_size;
            off_t send_start = 0;
            off_t send_end = st.st_size - 1;
            int status_code = 200;

            if (has_range) {
                send_start = range_start;
                send_end = range_end;
                content_length = (send_end - send_start) + 1;
                status_code = 206;
            }

            char* content = malloc(content_length);
            if (!content) {
                close(file_fd);
                close(client_fd);
                return NULL;
            }
            pread(file_fd, content, content_length, send_start);
            close(file_fd);

            const char* mime = get_mime_type(filepath);

            char* compressed_content = NULL;
            size_t compressed_length = 0;
            int compressed = 0;

            if (accepts_gzip && content_length > 512) {
                if (gzip_compress(content, content_length, &compressed_content, &compressed_length)) {
                    compressed = 1;
                    free(content);
                    content = compressed_content;
                    content_length = compressed_length;
                }
            }

            char header[MAX_HEADER];
            if (status_code == 206) {
                snprintf(header, sizeof(header),
                    "HTTP/1.1 206 Partial Content\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: %zu\r\n"
                    "Content-Range: bytes %ld-%ld/%ld\r\n"
                    "Server: Zafir/1.0\r\n"
                    "Connection: %s\r\n"
                    "%s\r\n",
                    mime, content_length,
                    (long)send_start, (long)send_end, (long)st.st_size,
                    keep_alive ? "keep-alive" : "close",
                    compressed ? "Content-Encoding: gzip" : "");
            } else {
                snprintf(header, sizeof(header),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: %zu\r\n"
                    "Server: Zafir/1.0\r\n"
                    "Connection: %s\r\n"
                    "%s\r\n",
                    mime, content_length,
                    keep_alive ? "keep-alive" : "close",
                    compressed ? "Content-Encoding: gzip" : "");
            }

            send(client_fd, header, strlen(header), 0);
            send(client_fd, content, content_length, 0);

            if (compressed) free(compressed_content);
            else free(content);

            log_message(LOG_INFO, "%s %s %d to %s%s",
                status_code == 200 ? "Served" : "Partial",
                filepath, status_code, ip, keep_alive ? " (keep-alive)" : "");

            memmove(buffer, buffer + header_len, buf_used - header_len);
            buf_used -= header_len;

            requests_handled++;

            if (!keep_alive) {
                close(client_fd);
                return NULL;
            }
        }
    }

    close(client_fd);
    return NULL;
}

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
#include <ctype.h>
#include "cache.h"
#include "mime.h"
#include "ratelimit.h"
#include "log.h"

#define MAX_BUFFER 8192
#define MAX_HEADER 1024
#define MAX_PATH 512


static int is_valid_path(const char* path) {
    if (strstr(path, "..")) return 0;
    for (int i = 0; path[i]; i++) {
        if (!isprint((unsigned char)path[i])) return 0;
    }
    return 1;
}

static int parse_range_header(const char* header, off_t file_size, off_t* start, off_t* end) {
    if (!header) return 0;
    const char* range_str = strstr(header, "bytes=");
    if (!range_str) return 0;
    range_str += strlen("bytes=");

    char* dash = strchr(range_str, '-');
    if (!dash) return 0;

    char start_str[32] = {0};
    char end_str[32] = {0};
    
    strncpy(start_str, range_str, dash - range_str);
    strncpy(end_str, dash + 1, sizeof(end_str) - 1);

    *start = atoll(start_str);
    *end = *end_str ? atoll(end_str) : file_size - 1;

    return (*start <= *end && *end < file_size);
}

static int client_accepts_gzip(const char* headers) {
    const char* p = strcasestr(headers, "Accept-Encoding:");
    return (p && strstr(p, "gzip")) ? 1 : 0;
}

static int gzip_compress(const char* input, size_t input_len, char** output, size_t* output_len) {
    z_stream zs = {0};
    if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) return 0;

    size_t buf_size = input_len + 1024;
    *output = malloc(buf_size);
    if (!*output) {
        deflateEnd(&zs);
        return 0;
    }

    zs.next_in = (Bytef*)input;
    zs.avail_in = input_len;
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
        const char* msg = "HTTP/1.1 429 Too Many Requests\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(client_fd, msg, strlen(msg), 0);
        close(client_fd);
        return NULL;
    }

    char buffer[MAX_BUFFER];
    ssize_t received;
    size_t buf_used = 0;
    int max_requests = 100, requests_handled = 0;

    while (requests_handled++ < max_requests) {
        received = recv(client_fd, buffer + buf_used, sizeof(buffer) - buf_used - 1, 0);
        if (received <= 0) break;

        buf_used += received;
        buffer[buf_used] = '\0';

        char* header_end;
        while ((header_end = strstr(buffer, "\r\n\r\n"))) {
            size_t header_len = header_end - buffer + 4;

            char* method_end = strchr(buffer, ' ');
            if (!method_end || method_end >= header_end) {
                const char* resp = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                send(client_fd, resp, strlen(resp), 0);
                close(client_fd);
                return NULL;
            }

            char method[8] = {0};
            size_t method_len = method_end - buffer;
            if (method_len >= sizeof(method)) method_len = sizeof(method) - 1;
            strncpy(method, buffer, method_len);

            if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
                const char* resp = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                send(client_fd, resp, strlen(resp), 0);
                close(client_fd);
                return NULL;
            }

            int is_head = strcmp(method, "HEAD") == 0;

            char* path_start = method_end + 1;
            char* path_end = strchr(path_start, ' ');
            if (!path_end || path_end >= header_end) break;
  
            char path[MAX_PATH];
            size_t path_len = path_end - path_start;
            strncpy(path, path_start, path_len);
            path[path_len] = '\0';

            if (!is_valid_path(path)) {
                log_message(LOG_WARN, "Rejected unsafe path from %s: %s", ip, path);
                const char* msg = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                send(client_fd, msg, strlen(msg), 0);
                close(client_fd);
                return NULL;
            }

            int keep_alive = strstr(buffer, "Connection: keep-alive") || strstr(buffer, "HTTP/1.1");
            int accepts_gzip = client_accepts_gzip(buffer);

            char filepath[MAX_PATH];
            snprintf(filepath, sizeof(filepath), strcmp(path, "/") == 0 ? "public/index.html" : "public/%s", path + 1);

            int file_fd = open(filepath, O_RDONLY);
            struct stat st;
            if (file_fd < 0 || fstat(file_fd, &st) < 0) {
                file_fd = open("public/404.html", O_RDONLY);
                fstat(file_fd, &st);
            }

            off_t range_start = 0, range_end = st.st_size - 1;
            int has_range = parse_range_header(strstr(buffer, "Range:"), st.st_size, &range_start, &range_end);
            size_t content_length = range_end - range_start + 1;
            int status_code = has_range ? 206 : 200;

            char* content = malloc(content_length);
            if (!content || pread(file_fd, content, content_length, range_start) != content_length) {
                close(file_fd);
                close(client_fd);
                free(content);
                return NULL;
            }
            close(file_fd);

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
                "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n%s%s\r\n\r\n",
                status_code, status_code == 200 ? "OK" : "Partial Content",
                mime, content_length,
                has_range ? "Content-Range: bytes " : "",
                has_range ? "...\r\n" : "");

            send(client_fd, header, strlen(header), 0);

            if (!is_head) {
                send(client_fd, content, content_length, 0);
            }
            
            free(content);

            log_message(LOG_INFO, "Served %s %d to %s%s", filepath, status_code, ip, keep_alive ? " (keep-alive)" : "");

            memmove(buffer, buffer + header_len, buf_used - header_len);
            buf_used -= header_len;

            if (!keep_alive) {
                close(client_fd);
                return NULL;
            }
        }
    }

    close(client_fd);
    return NULL;
}

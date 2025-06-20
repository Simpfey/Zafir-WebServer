#include <sys/sendfile.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "config.h"
#include "log.h"
#include "mime.h"
#include "cache.h"

#define MAX_PATH 512

static const char* get_error_page_path(int status_code) {
    static char key[16];
    snprintf(key, sizeof(key), "%dError", status_code);
    
    const char* page = resolve_rewrite(key);
    return page ? page : NULL;
}


void send_error_response(int client_fd, int status_code, const char* reason) {

    const char* error_page = get_error_page_path(status_code);
    char* content = NULL;
    size_t content_length = 0;
    int found_page = 0;
    char filepath[MAX_PATH];

    if (error_page) {
        snprintf(filepath, sizeof(filepath), "public/%s", error_page);
        int fd = open(filepath, O_RDONLY);
        if (fd >= 0) {
            struct stat st;
            if (fstat(fd, &st) == 0) {
                content_length = st.st_size;
                content = malloc(content_length);
                if (read(fd, content, content_length) == content_length) {
                    found_page = 1;
                } else {
                    free(content);
                    content = NULL;
                    content_length = 0;
                }
            }
            close(fd);
        }
    }

    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\nContent-Type: text/html\r\nContent-Length: %zu\r\nServer: Zafir/1.0\r\nConnection: close\r\n\r\n",
             status_code, reason, content_length);

    log_message(LOG_INFO, "Served %s %d", filepath, status_code);

    send(client_fd, header, strlen(header), 0);
    if (found_page) {
        send(client_fd, content, content_length, 0);
        free(content);
    }
}
#ifndef HEADER_H
#define HEADER_H

#define MAX_HEADERS 32
#define MAX_KEY 64
#define MAX_VALUE 512
#define MAX_PATH 512

typedef struct {
    char method[8];
    char path[MAX_PATH];
    char version[16];
    struct {
        char key[MAX_KEY];
        char value[MAX_VALUE];
    } headers[MAX_HEADERS];
    int header_count;
} http_request_t;

void trim_whitespace(char* str);
int parse_http_request(const char* raw, http_request_t* req);
const char* get_header(const http_request_t* req, const char* key);
int parse_range_header(const char* header, off_t file_size, off_t* start, off_t* end);

#endif

#ifndef HEADER_H
#define HEADER_H

#define MAX_HEADERS 32
#define MAX_KEY 64
#define MAX_VALUE 512
#define MAX_PATH 512

#define MAX_FORM_FIELDS 32
#define MAX_FORM_KEY 64
#define MAX_FORM_VALUE 512

typedef struct {
    char method[8];
    char path[MAX_PATH];
    char version[16];
    struct {
        char key[MAX_KEY];
        char value[MAX_VALUE];
    } headers[MAX_HEADERS];
    int header_count;
    
    char body[8192];
    size_t body_length;

    struct {
        char key[MAX_FORM_KEY];
        char value[MAX_FORM_VALUE];
    } form_fields[MAX_FORM_FIELDS];
    int form_field_count;
} http_request_t;

void trim_whitespace(char* str);
int parse_form_urlencoded(http_request_t* req);
int parse_http_request(const char* raw, http_request_t* req);
const char* get_header(const http_request_t* req, const char* key);
int parse_range_header(const char* header, off_t file_size, off_t* start, off_t* end);

#endif

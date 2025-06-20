#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "header.h"

#define MAX_HEADERS 32
#define MAX_KEY 64
#define MAX_VALUE 512
#define MAX_PATH 512

#define MAX_FORM_FIELDS 32
#define MAX_FORM_KEY 64
#define MAX_FORM_VALUE 512

void trim_whitespace(char* str) {
    while (isspace((unsigned char)*str)) str++;
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) *end-- = '\0';
}

int parse_form_urlencoded(http_request_t* req) {
    char* body = req->body;
    req->form_field_count = 0;
    
    while (*body && req->form_field_count < MAX_FORM_FIELDS) {
        char* key = body;
        char* val = strchr(body, '=');
        if (!val) break;
        *val++ = '\0';
        char* next = strchr(val, '&');
        if (next) *next++ = '\0';

        strncpy(req->form_fields[req->form_field_count].key, key, MAX_FORM_KEY - 1);
        strncpy(req->form_fields[req->form_field_count].value, val, MAX_FORM_VALUE - 1);
        req->form_field_count++;

        body = next ? next : val + strlen(val);
    }

    return req->form_field_count;
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
    strncpy(end_str, dash + 1, sizeof(end_str) - 1);

    *start = atoll(start_str);
    *end = *end_str ? atoll(end_str) : file_size - 1;

    return (*start <= *end && *end < file_size);
}

int parse_http_request(const char* raw, http_request_t* req) {
    const char* pos = raw;
    const char* line_end = strstr(pos, "\r\n");
    if (!line_end) return -1;

    if (sscanf(pos, "%7s %511s %15s", req->method, req->path, req->version) != 3) return -1;
    req->header_count = 0;

    pos = line_end + 2;

    while ((line_end = strstr(pos, "\r\n")) && line_end != pos && req->header_count < MAX_HEADERS) {
        char line[1024];
        size_t len = line_end - pos;
        if (len >= sizeof(line)) return -1;

        strncpy(line, pos, len);
        line[len] = '\0';

        char* colon = strchr(line, ':');
        if (!colon) return -1;

        *colon = '\0';
        strncpy(req->headers[req->header_count].key, line, MAX_KEY - 1);
        strncpy(req->headers[req->header_count].value, colon + 1, MAX_VALUE - 1);

        trim_whitespace(req->headers[req->header_count].key);
        trim_whitespace(req->headers[req->header_count].value);

        req->header_count++;
        pos = line_end + 2;
    }

    return 0;
}

const char* get_header(const http_request_t* req, const char* key) {
    for (int i = 0; i < req->header_count; ++i) {
        if (strcasecmp(req->headers[i].key, key) == 0) {
            return req->headers[i].value;
        }
    }

    return NULL;
}
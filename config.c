#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "https.h"
#include "log.h"

#define MAX_FILE 256
#define MAX_PROTECTED_FILES 128
#define MAX_REWRITES 128
#define MAX_NAME 64

int enable_https;
const char *https_cert_path = "cert.pem";
const char *https_key_path = "key.pem";

char *protected_files_list[MAX_PROTECTED_FILES];
int protected_files_count = 0;

typedef struct {
    char name[MAX_NAME];
    char file[MAX_FILE];
} rewrite_rule_t;

static rewrite_rule_t rewrites[MAX_REWRITES];
static int rewrite_count = 0;

void parse_protected_files(const char *protected_str) {
    char *copy = strdup(protected_str);
    char *token = strtok(copy, " ");
    while (token && protected_files_count < MAX_PROTECTED_FILES) {
        protected_files_list[protected_files_count++] = strdup(token);
        token = strtok(NULL, " ");
    }
    free(copy);
}

int is_protected(const char *filename) {
    for (int i = 0; i < protected_files_count; i++) {
        if (strcmp(filename, protected_files_list[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

const char* resolve_rewrite(const char* request_path) {
    const char* path = request_path[0] == '/' ? request_path + 1 : request_path;

    for (int i = 0; i < rewrite_count; ++i) {
        if (strcmp(rewrites[i].name, path) == 0) {
            return rewrites[i].file;
        }
    }
    return NULL;
}


void load_rewrite_rules(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == ';' || line[0] == '[' || strlen(line) < 3) continue;

        char* equals = strchr(line, '=');
        if (!equals) continue;

        *equals = '\0';
        char* key = line;
        char* value = equals + 1;

        key[strcspn(key, "\r\n\t ")] = 0;
        value[strcspn(value, "\r\n\t ")] = 0;

        if (rewrite_count < MAX_REWRITES) {
            strncpy(rewrites[rewrite_count].name, key, MAX_NAME - 1);
            strncpy(rewrites[rewrite_count].file, value, MAX_FILE - 1);
            rewrite_count++;
        }
    }

    enable_https = is_https_enabled();

    const char* protected_files = resolve_rewrite("PROTECTED");

    parse_protected_files(protected_files);

    fclose(f);
}
#ifndef CONFIG_H
#define CONFIG_H

extern int enable_https;
extern const char *https_cert_path;
extern const char *https_key_path;

extern int rewrite_count;

void parse_protected_files(const char *protected_str);
int is_protected(const char *filename);
const char* resolve_rewrite(const char* request_path);
void load_rewrite_rules(const char* filename);

#endif
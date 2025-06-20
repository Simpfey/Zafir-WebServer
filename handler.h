#ifndef HANDLER_H
#define HANDLER_H

int is_valid_path(const char* path);
int client_accepts_gzip(const char* headers);
int gzip_compress(const char* input, size_t input_len, char** output, size_t* output_len);
void* handle_client(void* arg);

#endif

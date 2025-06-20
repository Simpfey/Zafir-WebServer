#ifndef GZIP_H
#define GZIP_H

int gzip_compress(const char* input, size_t input_len, char** output, size_t* output_len);

#endif

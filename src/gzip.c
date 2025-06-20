#include <zlib.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

int gzip_compress(const char* input, size_t input_len, char** output, size_t* output_len) {
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
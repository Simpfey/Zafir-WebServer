#include <string.h>
#include "mime.h"

const char* get_mime_type(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    ext++;
    if (strcmp(ext, "html") == 0 || strcmp(ext, "zafir") == 0) return "text/html";
    if (strcmp(ext, "css") == 0) return "text/css";
    if (strcmp(ext, "js") == 0) return "application/javascript";
    if (strcmp(ext, "png") == 0) return "image/png";
    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, "gif") == 0) return "image/gif";
    if (strcmp(ext, "txt") == 0) return "text/plain";
    return "application/octet-stream";
}

#include <string.h>
#include <ctype.h>

int is_valid_path(const char* path) {
    if (strstr(path, "..")) return 0;
    for (int i = 0; path[i]; i++) {
        if (!isprint((unsigned char)path[i])) return 0;
    }
    return 1;
}
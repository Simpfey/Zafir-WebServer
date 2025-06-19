#ifndef CACHE_H
#define CACHE_H

typedef struct {
    char path[512];
    char* content;
    size_t size;
    time_t mtime;
} FileCacheEntry;

FileCacheEntry* get_from_cache(const char* path);
void add_to_cache(const char* path, const char* content, size_t size);

#endif

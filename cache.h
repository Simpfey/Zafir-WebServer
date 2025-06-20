#ifndef CACHE_H
#define CACHE_H

#define MAX_PATH 512

#include <time.h>


typedef struct CacheNode {
    char path[MAX_PATH];
    char* content;
    size_t size;
    struct CacheNode* prev;
    struct CacheNode* next;
} CacheNode;

void remove_tail();
void move_to_head(CacheNode* node);
CacheNode* get_from_cache(const char* path);
void add_to_cache(const char* path, const char* content, size_t size);

#endif

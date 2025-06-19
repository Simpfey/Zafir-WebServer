#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "cache.h"

#define CACHE_SIZE 512

static FileCacheEntry cache[CACHE_SIZE];
static int cache_count = 0;
static pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

FileCacheEntry* get_from_cache(const char* path) {
    pthread_mutex_lock(&cache_mutex);
    for (int i = 0; i < cache_count; i++) {
        if (strcmp(cache[i].path, path) == 0) {
            pthread_mutex_unlock(&cache_mutex);
            return &cache[i];
        }
    }
    pthread_mutex_unlock(&cache_mutex);
    return NULL;
}

void add_to_cache(const char* path, const char* content, size_t size) {
    pthread_mutex_lock(&cache_mutex);
    if (cache_count < CACHE_SIZE) {
        FileCacheEntry* entry = &cache[cache_count++];
        strncpy(entry->path, path, sizeof(entry->path));
        entry->content = malloc(size);
        memcpy(entry->content, content, size);
        entry->size = size;
    }
    pthread_mutex_unlock(&cache_mutex);
}

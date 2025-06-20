#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "cache.h"

#define CACHE_SIZE 512
#define CACHE_EXPIRATION 120
#define MAX_CACHE_ENTRIES 100

static CacheNode* cache_head = NULL;
static CacheNode* cache_tail = NULL;
static int cache_count = 0;

void remove_tail() {
    if (!cache_tail) return;
    CacheNode* to_remove = cache_tail;
    if (cache_tail->prev) {
        cache_tail = cache_tail->prev;
        cache_tail->next = NULL;
    } else {
        cache_head = cache_tail = NULL;
    }
    free(to_remove->content);
    free(to_remove);
    cache_count--;
}

void move_to_head(CacheNode* node) {
    if (node == cache_head) return;
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
    if (node == cache_tail) cache_tail = node->prev;

    node->prev = NULL;
    node->next = cache_head;
    if (cache_head) cache_head->prev = node;
    cache_head = node;
    if (!cache_tail) cache_tail = node;
}

CacheNode* get_from_cache(const char* path) {
    CacheNode* curr = cache_head;
    while (curr) {
        if (strcmp(curr->path, path) == 0) {
            move_to_head(curr);
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

void add_to_cache(const char* path, const char* content, size_t size) {
    CacheNode* node = malloc(sizeof(CacheNode));
    strncpy(node->path, path, MAX_PATH - 1);
    node->path[MAX_PATH - 1] = '\0';
    node->content = malloc(size);
    memcpy(node->content, content, size);
    node->size = size;
    node->prev = NULL;
    node->next = cache_head;
    if (cache_head) cache_head->prev = node;
    cache_head = node;
    if (!cache_tail) cache_tail = node;
    cache_count++;

    while (cache_count > MAX_CACHE_ENTRIES) {
        remove_tail();
    }
}
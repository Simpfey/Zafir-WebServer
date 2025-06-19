#include <string.h>
#include <time.h>
#include <pthread.h>
#include "ratelimit.h"

#define MAX_CLIENTS 1024
#define IP_LEN 46

typedef struct {
    char ip[IP_LEN];
    time_t last_time;
    int count;
} RateEntry;

static RateEntry rate_table[MAX_CLIENTS];
static int RATE_LIMIT = 100;
static pthread_mutex_t rate_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_rate_limit(int limit) {
    RATE_LIMIT = limit;
}

int is_rate_limited(const char* ip) {
    time_t now = time(NULL);
    pthread_mutex_lock(&rate_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(rate_table[i].ip, ip) == 0) {
            if (rate_table[i].last_time == now) {
                rate_table[i].count++;
                if (rate_table[i].count > RATE_LIMIT) {
                    pthread_mutex_unlock(&rate_mutex);
                    return 1;
                }
            } else {
                rate_table[i].last_time = now;
                rate_table[i].count = 1;
            }
            pthread_mutex_unlock(&rate_mutex);
            return 0;
        }
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (rate_table[i].ip[0] == '\0') {
            strncpy(rate_table[i].ip, ip, IP_LEN);
            rate_table[i].last_time = now;
            rate_table[i].count = 1;
            break;
        }
    }
    pthread_mutex_unlock(&rate_mutex);
    return 0;
}

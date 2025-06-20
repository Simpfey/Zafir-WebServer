#include "metrics.h"

atomic_ulong total_requests = 0;
atomic_ulong cache_hits = 0;
atomic_ulong cache_misses = 0;
time_t server_start_time = 0;

void init_metrics() {
    server_start_time = time(NULL);
    atomic_store(&total_requests, 0);
    atomic_store(&cache_hits, 0);
    atomic_store(&cache_misses, 0);
}
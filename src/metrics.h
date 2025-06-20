#ifndef METRICS_H
#define METRICS_H

#include <stdatomic.h>
#include <time.h>

extern atomic_ulong total_requests;
extern atomic_ulong cache_hits;
extern atomic_ulong cache_misses;
extern time_t server_start_time;

void init_metrics();

#endif
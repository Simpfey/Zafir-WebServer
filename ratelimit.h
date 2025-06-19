#ifndef RATELIMIT_H
#define RATELIMIT_H

void init_rate_limit(int limit);
int is_rate_limited(const char* ip);

#endif

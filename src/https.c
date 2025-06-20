#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "log.h"

int is_https_enabled() {
    const char* cHTTPS = resolve_rewrite("HTTPS");

    if (cHTTPS)
        return atoi(cHTTPS);

    return 0;
}

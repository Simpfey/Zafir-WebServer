#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "server.h"
#include "ratelimit.h"
#include "log.h"
#include "config.h"
#include "ssl.h"
#include "https.h"
#include "metrics.h"

void print_usage(const char* prog) {
    printf("Usage: %s --port <PORT> [--rate-limit <N>]\n", prog);
}

int main(int argc, char* argv[]) {
    int port = 8080;
    int rate_limit = 100;

    signal(SIGPIPE, SIG_IGN);

    load_rewrite_rules("htconfig.ini");
    init_metrics();

    const char* cPort = resolve_rewrite("PORT");
    const char* cRateLimit = resolve_rewrite("RATE-LIMIT");
    
    if (cPort)
        port = atoi(cPort);
    if (cRateLimit)
        rate_limit = atoi(cRateLimit);

    if (enable_https) {
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();

        if (!init_ssl_context(&ssl_ctx, https_cert_path, https_key_path)) {
            log_message(LOG_ERROR, "SSL initialization failed. Running without HTTPS.");
            enable_https = 0;
        }
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--rate-limit") == 0 && i + 1 < argc) {
            rate_limit = atoi(argv[++i]);
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }
    log_message(LOG_INFO, "Rate Limit Set to %d", rate_limit);
    log_message(LOG_INFO, "Server starting on port %d", port);

    init_rate_limit(rate_limit);
    run_server(port);
    log_close();
    return 0;
}

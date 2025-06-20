#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char* LEVEL_STRINGS[] = {"INFO", "WARN", "ERROR"};
static const char* LEVEL_COLORS[] = {
    "\033[0;32m", // Green  (INFO)
    "\033[0;33m", // Yellow (WARN)
    "\033[0;31m"  // Red    (ERROR)
};
static const char* COLOR_RESET = "\033[0m";

void log_message(LogLevel level, const char* format, ...) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", t);

    pthread_mutex_lock(&log_mutex);

    fprintf(stderr, "%s[%s] [%s] ", LEVEL_COLORS[level], time_buf, LEVEL_STRINGS[level]);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "%s\n", COLOR_RESET);

    pthread_mutex_unlock(&log_mutex);
}

void log_close() {
    pthread_mutex_lock(&log_mutex);
    pthread_mutex_unlock(&log_mutex);
}

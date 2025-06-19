#ifndef LOG_H
#define LOG_H

typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

void log_init();
void log_message(LogLevel level, const char* format, ...);
void log_close();

#endif

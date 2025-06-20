#include <stdio.h>
#include <time.h>

void format_uptime(time_t seconds, char* buffer, size_t buffer_size) {
    int days = seconds / 86400;
    seconds %= 86400;
    int hours = seconds / 3600;
    seconds %= 3600;
    int minutes = seconds / 60;
    int secs = seconds % 60;

    snprintf(buffer, buffer_size, "%dd %dh %dm %ds", days, hours, minutes, secs);
}
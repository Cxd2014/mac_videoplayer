#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>

#include "log.h"

static const char *log_level[] = {
    "",
    "error",
    "info",
    "debug",
};

int64_t get_now_millisecond() {
    struct timespec current_time;
    clock_gettime(CLOCK_REALTIME, &current_time);
    return (int64_t)current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000;
}

static int log_fd;
int log_init(const char *file, int log_level) {
    log_fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd <= 0) {
        printf("opening log file error:%d\n", log_fd);
        return -1;
    }
    return 0;
}

void log_uninit() {
    close(log_fd);
    log_fd = 0;
}

void log_core(int level, int line, const char *func, const char *fmt, ...) {
    char log_time[64] = {0};
    int64_t now_millisecond = get_now_millisecond();
    int millisecond = now_millisecond % 1000;
    time_t now = now_millisecond / 1000;
    strftime(log_time, sizeof(log_time), "%Y-%m-%d %H:%M:%S", localtime(&now));

    char log_prefix[128] = {0};
    snprintf(log_prefix, sizeof(log_prefix), "%s.%03d [%s]%s:%d ", log_time, millisecond, log_level[level], func, line);

    char str[1024] = {0};
    va_list args;
    va_start(args, fmt);
    vsnprintf(str, sizeof(str), fmt, args);
    va_end(args);

    char log_content[1280] = {0};
    strcpy(log_content, log_prefix);
    strcat(log_content, str);

    size_t log_len = strlen(log_content);
    log_content[log_len] = '\n';
    log_len++;

    if (write(log_fd, log_content, log_len) <= 0) {
        printf("write log file error:%d\n", log_fd);
    }
}

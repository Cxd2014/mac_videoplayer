#ifndef LOG_H_INCLUDED_
#define LOG_H_INCLUDED_

#define LOG_ERROR 1
#define LOG_INFO 2
#define LOG_DEBUG 3


#define log_error(fmt, ...)                                        \
    log_core(LOG_ERROR, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#define log_info(fmt, ...)                                        \
    log_core(LOG_INFO, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#define log_debug(fmt, ...)                                        \
    log_core(LOG_DEBUG, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

void log_core(int level, int line, const char *func, const char *fmt, ...);

int log_init(const char *file, int log_level);
void log_uninit();
#endif
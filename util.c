#include "util.h"
#include <stdint.h>



int64_t get_now_millisecond() {
    struct timespec current_time;
    clock_gettime(CLOCK_REALTIME, &current_time);
    return (int64_t)current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000;
}

int64_t calc_duration(int64_t start) {
    return (get_now_millisecond() - start);
}

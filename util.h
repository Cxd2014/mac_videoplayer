#ifndef UTIL_H_INCLUDED_
#define UTIL_H_INCLUDED_

#include <time.h>
#include <stdint.h>
#include "context.h"

int64_t calc_duration(int64_t start);
int64_t get_now_millisecond();
void clean_all_queue(Context *ctx);
#endif

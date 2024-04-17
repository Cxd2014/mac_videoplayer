#include "util.h"
#include <stdint.h>

uint32_t duration(clock_t start)
{
    clock_t end = clock();
    return (int)(end - start) * 1000 / CLOCKS_PER_SEC;
}
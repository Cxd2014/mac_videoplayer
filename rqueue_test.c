
#include <SDL.h>
#include<stdlib.h>

#include "rqueue.h"
#include "log.h"

rqueue_t *test_queue;

int do_write_queue(void *arg) {
    int count = 0;
    int ret = 0;
    while (1) {
        count++;

        char* result = (char*)malloc(100);
        if (result == NULL) {
            log_error("Memory allocation failed");
            break;
        }
        
        snprintf(result, 100, "%d", count);

        do {
            ret = rqueue_write(test_queue, result);
            if (ret != 0) {
                //log_error("rqueue_write error %d", ret);
                //SDL_Delay(10);
            }
        } while (ret != 0);

        //log_info("write = %s", result);

        if (count > 10000) {
            log_info("write done");
            break;
        }
    }
    return 0;
}

int do_read_queue(void *arg) {
    while (1) {
        char* result = rqueue_read(test_queue);
        if (result == NULL) {
            //log_error("rqueue_read error");
            continue;
        }
        log_info("%s result = %s", arg, result);
        free(result);
    }
    return 0;
}

int main() {

    log_init("debug.log", 1);

    test_queue = rqueue_create(16, RQUEUE_MODE_BLOCKING);
    if (test_queue == NULL) {
        log_debug("rqueue_create error");
        return __LINE__;
    }


    SDL_Thread *writeThread;
    SDL_Thread *readThread_a;
    SDL_Thread *readThread_b;

    writeThread = SDL_CreateThread(do_write_queue, "writeThread", NULL);
    if (!writeThread) {
        log_debug("SDL_CreateThread(): %s", SDL_GetError());
        return __LINE__;
    }

    readThread_a = SDL_CreateThread(do_read_queue, "readThread_a", "a");
    if (!readThread_a) {
        log_debug("SDL_CreateThread(): %s", SDL_GetError());
        return __LINE__;
    }

    readThread_b = SDL_CreateThread(do_read_queue, "readThread_b", "b");
    if (!readThread_b) {
        log_debug("SDL_CreateThread(): %s", SDL_GetError());
        return __LINE__;
    }

    SDL_Delay(1000);

    log_uninit();
    return 0;
}
#include<stdlib.h>

#include "sdl_window.h"
#include "log.h"
#include "video.h"
#include "audio.h"
#include "context.h"

#define QUEUE_SIZE (32)

static int init_context(Context *ctx) {
    ctx->quit = false;
    ctx->pause = false;
    ctx->sdl.width = 1920;
    ctx->sdl.higth = 1080;
    ctx->ffmpeg.filename = "/Users/chengxiaodong/Desktop/doc/player/new_player/test.mp4";

    ctx->sdl.render = NULL;
    ctx->sdl.texture = NULL;

    ctx->apacket_queue = rqueue_create(QUEUE_SIZE, RQUEUE_MODE_BLOCKING);
    if (ctx->apacket_queue == NULL) {
        log_error("rqueue_create error");
        return -1;
    }

    ctx->vpacket_queue = rqueue_create(QUEUE_SIZE, RQUEUE_MODE_BLOCKING);
    if (ctx->vpacket_queue == NULL) {
        log_error("rqueue_create error");
        return -2;
    }

    ctx->vframe_queue = rqueue_create(QUEUE_SIZE, RQUEUE_MODE_BLOCKING);
    if (ctx->vframe_queue == NULL) {
        log_error("rqueue_create error");
        return -3;
    }

    ctx->aframe_queue = rqueue_create(QUEUE_SIZE, RQUEUE_MODE_BLOCKING);
    if (ctx->aframe_queue == NULL) {
        log_error("rqueue_create error");
        return -4;
    }

    if (create_window(ctx) < 0) {
        log_error("create_window error");
        return -5;
    }

    if (init_ffmpeg(ctx) < 0) {
        log_error("init_ffmpeg error");
        return -6;
    }

    return 0;
}

int main() {
    log_init("debug.log", LOG_INFO);

    Context *ctx = calloc(1, sizeof(Context));
    if (!ctx) {
        log_error("mallocz error");
        return -1;
    }
    init_context(ctx);

    ctx->read_tid = SDL_CreateThread(read_thread, "read_thread", ctx);
    ctx->video_tid = SDL_CreateThread(decode_video, "decode_video", ctx);
    ctx->audio_tid = SDL_CreateThread(decode_audio, "decode_audio", ctx);

    sdl_event_loop(ctx);

    SDL_WaitThread(ctx->read_tid, NULL);
    SDL_WaitThread(ctx->video_tid, NULL);
    SDL_WaitThread(ctx->audio_tid, NULL);

    log_uninit();
    return 0;
}
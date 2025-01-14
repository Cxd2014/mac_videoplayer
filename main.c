#include<stdlib.h>

#include "sdl_window.h"
#include "log.h"
#include "video.h"
#include "audio.h"
#include "context.h"

#define QUEUE_SIZE (12)

static int init_context(Context *ctx, char *filename) {
    ctx->quit = false;
    ctx->pause = false;
    ctx->ffmpeg.filename = filename;

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

    if (init_ffmpeg(ctx) < 0) {
        log_error("init_ffmpeg error");
        return -6;
    }

    if (create_window(ctx) < 0) {
        log_error("create_window error");
        return -5;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    log_init("debug.log", LOG_INFO);

    if (argc != 2) {
        log_error("./vplayer filename");
        return -2;
    }

    Context *ctx = calloc(1, sizeof(Context));
    if (!ctx) {
        log_error("mallocz error");
        return -1;
    }
    init_context(ctx, argv[1]);

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
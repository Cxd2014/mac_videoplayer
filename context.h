#ifndef CONTEXT_H_INCLUDED_
#define CONTEXT_H_INCLUDED_

#include <stdbool.h>

#include <SDL.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>

#include "rqueue.h"

typedef struct SDLInfo {
    uint32_t width;
    uint32_t higth;

    SDL_Window *window;
    SDL_Renderer *render;
    SDL_Texture *texture;

} SDLInfo;

typedef struct FFmpegInfo {
    char *filename;

    AVFormatContext *format;

    AVCodecContext *video_codec;
    AVCodecContext *audio_codec;

    struct SwsContext *sub_convert_ctx;

} FFmpegInfo;

typedef struct Context {
    SDL_Thread *read_tid;
    SDL_Thread *video_tid;
    SDL_Thread *audio_tid;

    bool quit;

    int video_index;
    int audio_index;

    rqueue_t *vpacket_queue;
    rqueue_t *apacket_queue;

    rqueue_t *vframe_queue;
    rqueue_t *aframe_queue;

    FFmpegInfo ffmpeg;
    SDLInfo sdl;
} Context;

#endif

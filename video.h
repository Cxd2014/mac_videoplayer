#ifndef VIDEO_H_INCLUDED_
#define VIDEO_H_INCLUDED_

#include "context.h"

int init_ffmpeg(Context *ctx);

int read_thread(void *arg);

int decode_video(void *arg);

#endif
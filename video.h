#ifndef PLAYER_H_INCLUDED_
#define PLAYER_H_INCLUDED_

#include "context.h"

int init_ffmpeg(Context *ctx);

int read_thread(void *arg);

int decode_video(void *arg);
int decode_audio(void *arg);

#endif
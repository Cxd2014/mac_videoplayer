#ifndef AUDIO_H_INCLUDED_
#define AUDIO_H_INCLUDED_

#include "context.h"

int decode_audio(void *arg);
int64_t get_audio_playtime(Context *ctx);

#endif
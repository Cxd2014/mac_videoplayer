#ifndef SDL_H_INCLUDED_
#define SDL_H_INCLUDED_

#include "context.h"

int create_window(Context *ctx);
int sdl_event_loop(Context *ctx);

#endif
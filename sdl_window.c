#include <SDL.h>

#include "sdl_window.h"
#include "log.h"

int sdl_init() {

    log_info("sdl_init...");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        log_error("could not initialize sdl2: %s", SDL_GetError());
        return -1;
    }

    // 创建窗口
    SDL_Window *pWindow = SDL_CreateWindow(
            "video player",
            0, 0,
            800, 400,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (pWindow == NULL) {
        log_error("could not create window: %s\n", SDL_GetError());
        return -2;
    }

    return 0;
}

int sdl_event_loop() {
    SDL_Event event;
    while (1)
    {
        SDL_PumpEvents();
        // 查看键盘，鼠标事件
        while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) != 0) {
            switch (event.type) {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_SPACE:
                    
                    break;
                case SDLK_RIGHT:
                    
                    break;
                }
                break;
            case SDL_MOUSEBUTTONDOWN: {
                break;
            }
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                    break;
                }
                break;
            case SDL_QUIT:
                log_info("exit...");
                return 0;
            }
        }
    }

    return 0;
}
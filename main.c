#include "sdl_window.h"
#include "log.h"
#include "video.h"

int main() {

    log_init("debug.log", 1);
    sdl_init();

    init_ffmpeg("/Users/chengxiaodong/Desktop/doc/player/new_player/test.mp4");

    sdl_event_loop();

    log_uninit();
    return 0;
}
#include "util.h"
#include <stdint.h>
#include "log.h"

int64_t get_now_millisecond() {
    struct timespec current_time;
    clock_gettime(CLOCK_REALTIME, &current_time);
    return (int64_t)current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000;
}

int64_t calc_duration(int64_t start) {
    return (get_now_millisecond() - start);
}


int clean_frame_queue(rqueue_t *q) {
    int count = 0;
    while (1) {
        AVFrame *frame = rqueue_read(q);
        if (frame == NULL) {
            break;
        }
        count++;
        av_frame_unref(frame);
        av_frame_free(&frame);
    }
    return count;
}

int clean_packet_queue(rqueue_t *q) {
    int count = 0;
     while (1) {
        AVPacket *packet = rqueue_read(q);
        if (packet == NULL) {
            break;
        }
        count++;
        av_packet_unref(packet);
        av_packet_free(&packet);
    }
    return count;
}

void clean_all_queue(Context *ctx) {
    int af = clean_frame_queue(ctx->aframe_queue);
    int vf = clean_frame_queue(ctx->vframe_queue);

    int ap = clean_packet_queue(ctx->apacket_queue);
    int vp = clean_packet_queue(ctx->vpacket_queue);

    SDL_ClearQueuedAudio(ctx->sdl.audio_device);

    log_info("af:%d vf:%d ap:%d vp:%d", af, vf, ap, vp);
}

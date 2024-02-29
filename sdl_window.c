#include "sdl_window.h"
#include "log.h"

int create_window(Context *ctx) {

    log_debug("create_window...");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        log_error("could not initialize sdl2: %s", SDL_GetError());
        return -1;
    }

    // 创建窗口
    ctx->sdl.window = SDL_CreateWindow(
            "vplayer",
            0, 0,
            ctx->sdl.width, ctx->sdl.higth,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (ctx->sdl.window == NULL) {
        log_error("could not create window: %s", SDL_GetError());
        return -2;
    }

    // 创建渲染器
    ctx->sdl.render = SDL_CreateRenderer(ctx->sdl.window, -1, 0);
    if (ctx->sdl.render == NULL) {
        log_error("SDL_CreateRenderer error: %s\n", SDL_GetError());
        return -3;
    }

    return 0;
}

/** 在窗口中绘制一帧画面 **/
static int render_video_frame(Context *ctx, AVFrame *frame) {
    if (frame == NULL) {
        log_error("frame is null error");
        return -1;
    }

    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = ctx->sdl.width;
    rect.h = ctx->sdl.higth;

    if (ctx->sdl.texture == NULL) {
        // 窗口大小调整之后需要重新创建纹理
        ctx->sdl.texture = SDL_CreateTexture(ctx->sdl.render, SDL_PIXELFORMAT_IYUV,
                        SDL_TEXTUREACCESS_STREAMING, rect.w, rect.h);
        if (ctx->sdl.texture == NULL) {
            log_error("SDL_CreateTexture error: %s", SDL_GetError());
            return -2;
        }
    }

    log_debug("Frame %c pts %d dts %d", av_get_picture_type_char(frame->pict_type), frame->pts, frame->pkt_dts);

    ctx->ffmpeg.sub_convert_ctx = sws_getCachedContext(ctx->ffmpeg.sub_convert_ctx,
            frame->width, frame->height, frame->format, rect.w, rect.h,
            frame->format, SWS_BICUBIC, NULL, NULL, NULL);
    if (ctx->ffmpeg.sub_convert_ctx == NULL) {
        log_error("sws_getCachedContext error");
        return -3;
    }

    // 根据窗口大小,调整视频帧的大小
    // 分配新视频帧的内存空间
    uint8_t *pixels[4] = {NULL};
    int linesize[4];
    int ret = av_image_alloc(pixels, linesize, rect.w, rect.h, frame->format, 1);
    if (ret < 0) {
        log_error("av_image_alloc error %d %d", ret, frame->format);
        return -4;
    }
    
    // 调整帧大小
    sws_scale(ctx->ffmpeg.sub_convert_ctx, (const uint8_t * const *)frame->data, frame->linesize,
                            0, frame->height, pixels, linesize);

    //上传YUV数据到Texture
    SDL_UpdateYUVTexture(ctx->sdl.texture, &rect,
                         pixels[0], linesize[0],
                         pixels[1], linesize[1],
                         pixels[2], linesize[2]);

    SDL_RenderClear(ctx->sdl.render); // 先清空渲染器
    SDL_RenderCopy(ctx->sdl.render, ctx->sdl.texture, NULL, &rect); // 将视频纹理复制到渲染器


    SDL_RenderPresent(ctx->sdl.render); // 渲染画面

    av_frame_unref(frame);
    av_frame_free(&frame);
    av_freep(&pixels[0]);
    return 0;
}

int sdl_event_loop(Context *ctx) {
    SDL_Event event;
    int count = 0;
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
                ctx->quit = true;
                log_info("render viode:%d exit...", count);
                return 0;
            }
        }

        AVFrame *frame = rqueue_read(ctx->vframe_queue);
        if (frame == NULL) {
            SDL_Delay(10);
        } else {
            count++;
            render_video_frame(ctx, frame);
            //SDL_Delay(10);
        }
    }

    return 0;
}
#include "sdl_window.h"
#include "log.h"
#include "util.h"

void get_window_size(Context *ctx) {
    SDL_GL_GetDrawableSize(ctx->sdl.window, &(ctx->sdl.width), &(ctx->sdl.higth));
    log_info("width:%d hight:%d", ctx->sdl.width, ctx->sdl.higth);
}

int create_window(Context *ctx) {

    log_debug("create_window...");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        log_error("could not initialize sdl2: %s", SDL_GetError());
        return -1;
    }
    
    // 遍历所有显示器并获取它们的分辨率
    int num_displays = SDL_GetNumVideoDisplays();
    for (int i = 0; i < num_displays; ++i) {
        SDL_DisplayMode display_mode;
        SDL_GetCurrentDisplayMode(i, &display_mode);

        float ddpi, hdpi, vdpi;
        SDL_GetDisplayDPI(i, &ddpi, &hdpi, &vdpi);

        log_info("Display %d:", i);
        log_info("  Resolution: %dx%d Refresh rate: %d Hz", display_mode.w, display_mode.h, display_mode.refresh_rate);
        log_info("  Diagonal DPI: %.2f Horizontal DPI: %.2f Vertical DPI: %.2f", ddpi,hdpi,vdpi);
    }

    // 创建窗口
    ctx->sdl.window = SDL_CreateWindow(
            "vplayer",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            ctx->sdl.width, ctx->sdl.higth,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE| SDL_WINDOW_ALLOW_HIGHDPI);
    if (ctx->sdl.window == NULL) {
        log_error("could not create window: %s", SDL_GetError());
        return -2;
    }
    get_window_size(ctx);

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
    clock_t start = clock();

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

    if (ctx->ffmpeg.sub_convert_ctx == NULL) {
        ctx->ffmpeg.sub_convert_ctx = sws_getCachedContext(ctx->ffmpeg.sub_convert_ctx,
                frame->width, frame->height, frame->format, rect.w, rect.h,
                AV_PIX_FMT_YUV420P, 0, NULL, NULL, NULL);
        if (ctx->ffmpeg.sub_convert_ctx == NULL) {
            log_error("sws_getCachedContext error");
            return -3;
        }
    }

    // 根据窗口大小,调整视频帧的大小
    // 分配新视频帧的内存空间
    uint8_t *pixels[4] = {NULL};
    int linesize[4];
    int ret = av_image_alloc(pixels, linesize, rect.w, rect.h, AV_PIX_FMT_YUV420P, 1);
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

    log_debug("format %d duration %d %d*%d", frame->format, calc_duration(start), rect.w, rect.h);
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
                    ctx->pause = !(ctx->pause); // 暂停，开启播放

                    if (ctx->pause) {
                        SDL_PauseAudioDevice(ctx->sdl.audio_device, 1);
                    } else {
                        SDL_PauseAudioDevice(ctx->sdl.audio_device, 0);
                    }
                    log_info("space key kickdown pause:%d", ctx->pause);
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
                    get_window_size(ctx);
                    if (ctx->sdl.texture) {
                        SDL_DestroyTexture(ctx->sdl.texture);
                        ctx->sdl.texture = NULL;
                    }

                    if (ctx->ffmpeg.sub_convert_ctx) {
                        sws_freeContext(ctx->ffmpeg.sub_convert_ctx);
                        ctx->ffmpeg.sub_convert_ctx = NULL;
                    }
                    break;
                }
                break;
            case SDL_QUIT:
                ctx->quit = true;
                log_info("render viode:%d exit...", count);
                return 0;
            }
        }

        if (ctx->pause) {
            SDL_Delay(10 * SELLP_MS);
        } else {
            AVFrame *frame = rqueue_read(ctx->vframe_queue);
            if (frame == NULL) {
                SDL_Delay(SELLP_MS);
                log_debug("rqueue_read vframe_queue null");
            } else {
                count++;
                render_video_frame(ctx, frame);
                SDL_Delay(SELLP_MS*2);
            }
        }
    }

    return 0;
}
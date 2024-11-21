#include "sdl_window.h"
#include "log.h"
#include "util.h"
#include "audio.h"

static void get_render_rect(Context *ctx) {

    int width = 0, height = 0;
    SDL_GL_GetDrawableSize(ctx->sdl.window, &width, &height);

    double video_scale = ctx->ffmpeg.width/(double)ctx->ffmpeg.height;
    double window_scale = width/(double)height;

    if (video_scale > window_scale) {
        ctx->sdl.rect.w = width;
        ctx->sdl.rect.h = (width*ctx->ffmpeg.height)/ctx->ffmpeg.width;
        ctx->sdl.rect.x = 0;
        ctx->sdl.rect.y = (height - ctx->sdl.rect.h)/2;
        ctx->sdl.rect.y = ctx->sdl.rect.y - (ctx->sdl.rect.y%2);
    } else {
        ctx->sdl.rect.w = (height*ctx->ffmpeg.width)/ctx->ffmpeg.height;
        ctx->sdl.rect.h = height;
        ctx->sdl.rect.x = (width - ctx->sdl.rect.w)/2;
        ctx->sdl.rect.y = 0;
        ctx->sdl.rect.x = ctx->sdl.rect.x - (ctx->sdl.rect.x%2);
    }

    log_info("width:%d hight:%d x:%d y:%d w:%d h:%d", width, height,
        ctx->sdl.rect.x, ctx->sdl.rect.y, ctx->sdl.rect.w, ctx->sdl.rect.h);
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
        log_info("  Diagonal DPI: %.2f Horizontal DPI: %.2f Vertical DPI: %.2f", ddpi, hdpi, vdpi);
    }

    // 创建窗口
    ctx->sdl.window = SDL_CreateWindow(
            "vplayer",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1920, 1080,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE| SDL_WINDOW_ALLOW_HIGHDPI);
    if (ctx->sdl.window == NULL) {
        log_error("could not create window: %s", SDL_GetError());
        return -2;
    }

    // 创建渲染器
    ctx->sdl.render = SDL_CreateRenderer(ctx->sdl.window, -1, SDL_RENDERER_ACCELERATED);
    if (ctx->sdl.render == NULL) {
        log_error("SDL_CreateRenderer error: %s\n", SDL_GetError());
        return -3;
    }

    get_render_rect(ctx);
    return 0;
}

/** 在窗口中绘制一帧画面 **/
static int render_video_frame(Context *ctx, AVFrame *frame) {
    if (frame == NULL) {
        log_error("frame is null error");
        return -1;
    }
    int64_t start = get_now_millisecond();
    SDL_Rect *rect = &(ctx->sdl.rect);

    if (ctx->sdl.texture == NULL) {
        // 窗口大小调整之后需要重新创建纹理
        ctx->sdl.texture = SDL_CreateTexture(ctx->sdl.render, SDL_PIXELFORMAT_IYUV,
                        SDL_TEXTUREACCESS_STREAMING, rect->w+rect->x, rect->h+rect->y);
        if (ctx->sdl.texture == NULL) {
            log_error("SDL_CreateTexture error: %s", SDL_GetError());
            return -2;
        }
    }

    if (ctx->ffmpeg.sub_convert_ctx == NULL) {
        ctx->ffmpeg.sub_convert_ctx = sws_getCachedContext(ctx->ffmpeg.sub_convert_ctx,
                frame->width, frame->height, frame->format, rect->w, rect->h,
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
    int ret = av_image_alloc(pixels, linesize, rect->w, rect->h, AV_PIX_FMT_YUV420P, 1);
    if (ret < 0) {
        log_error("av_image_alloc error %d %d", ret, frame->format);
        return -4;
    }
    
    // 调整帧大小
    sws_scale(ctx->ffmpeg.sub_convert_ctx, (const uint8_t * const *)frame->data, frame->linesize,
                            0, frame->height, pixels, linesize);

    //log_info("format %d duration %d %d*%d", frame->format, calc_duration(start), rect->w, rect->h);
    //上传YUV数据到Texture
    SDL_UpdateYUVTexture(ctx->sdl.texture, rect,
                         pixels[0], linesize[0],
                         pixels[1], linesize[1],
                         pixels[2], linesize[2]);

    SDL_RenderClear(ctx->sdl.render); // 先清空渲染器
    SDL_RenderCopy(ctx->sdl.render, ctx->sdl.texture, rect, rect); // 将视频纹理复制到渲染器
    SDL_RenderPresent(ctx->sdl.render); // 渲染画面

    av_freep(&pixels[0]);

    return calc_duration(start);
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

                int width = 0, height = 0;
                SDL_GetWindowSize(ctx->sdl.window, &width, &height);

                SDL_Rect rect;
                rect.x = 0;
                rect.y = height-50;
                rect.w = width;
                rect.h = 50;

                SDL_Point pot;
                pot.x = event.motion.x;
                pot.y = event.motion.y;
                log_info("x:%d y:%d rect x y w h %d:%d:%d:%d", pot.x, pot.y, rect.x, rect.y, rect.w, rect.h);

                if (rect.y > 0 && SDL_PointInRect(&pot, &rect) && ctx->pause == false) {
                    ctx->pause = true;
                   
                    AVRational base = ctx->ffmpeg.format->streams[ctx->video_index]->time_base;
                    int64_t vseek_point = (pot.x * ctx->ffmpeg.format->duration * base.den) / (int64_t)(width * AV_TIME_BASE * base.num);
                    if (av_seek_frame(ctx->ffmpeg.format, ctx->video_index, vseek_point, AVSEEK_FLAG_BACKWARD) < 0) {
                        log_error("av_seek_frame video error");
                    }


                    base = ctx->ffmpeg.format->streams[ctx->audio_index]->time_base;
                    int64_t aseek_point = (pot.x * ctx->ffmpeg.format->duration * base.den) / (int64_t)(width * AV_TIME_BASE * base.num);
                    if (av_seek_frame(ctx->ffmpeg.format, ctx->audio_index, aseek_point, AVSEEK_FLAG_BACKWARD) < 0) {
                        log_error("av_seek_frame audio error");
                    }

                    log_info("x:%d y:%d aseek_point:%ld vseek_point:%ld duration:%ld", 
                        pot.x, pot.y, aseek_point, vseek_point, ctx->ffmpeg.format->duration);

                    clean_all_queue(ctx);
                    ctx->pause = false;
                }
                break;
            }
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_SIZE_CHANGED:

                    if (ctx->sdl.texture) {
                        SDL_DestroyTexture(ctx->sdl.texture);
                        ctx->sdl.texture = NULL;
                    }

                    if (ctx->ffmpeg.sub_convert_ctx) {
                        sws_freeContext(ctx->ffmpeg.sub_convert_ctx);
                        ctx->ffmpeg.sub_convert_ctx = NULL;
                    }
                    get_render_rect(ctx);
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
                AVRational base = ctx->ffmpeg.format->streams[ctx->video_index]->time_base;

                int64_t audio_time = get_audio_playtime(ctx);
                int64_t video_time = frame->pts*1000*base.num/base.den;
                int diff_time = audio_time - video_time;
                int show_time = (frame->pts - ctx->video_pts)*1000*base.num/base.den;

                int render_time = render_video_frame(ctx, frame);

                int sleep_time = show_time - render_time - diff_time;
                log_info("audio_time:%ld video_time:%ld render_time:%d diff_time:%d show_time:%d sleep_time:%d", 
                    audio_time, video_time, render_time, diff_time, show_time, sleep_time);

                if (sleep_time > 0 && sleep_time < 2000)
                    SDL_Delay(sleep_time);
                    
                ctx->video_pts = frame->pts;
                av_frame_unref(frame);
                av_frame_free(&frame);
            }
        }
    }

    return 0;
}
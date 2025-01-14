#include "audio.h"
#include "log.h"
#include "util.h"

int init_audio(void *arg) {
    Context *ctx = arg;

    // 设置音频参数
    SDL_AudioSpec wanted_spec, device_spec;
    wanted_spec.freq = ctx->ffmpeg.audio_codec->sample_rate;
    wanted_spec.channels = ctx->ffmpeg.audio_codec->ch_layout.nb_channels;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.samples = 1024;
    wanted_spec.callback = NULL;
    wanted_spec.userdata = NULL;

    // 打开音频设备
    ctx->sdl.audio_device = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &device_spec, 0);
    if (ctx->sdl.audio_device == 0) {
        log_error("Failed to open audio device:%s", SDL_GetError());
        return -1;
    }

    log_info("device freq:%d channels:%d format:%d samples:%d hw_bufsize:%d time_base:%f", 
        device_spec.freq, device_spec.channels, device_spec.format, device_spec.samples, device_spec.size, 
        av_q2d(ctx->ffmpeg.format->streams[ctx->audio_index]->time_base));

    // 开始播放音频
    SDL_PauseAudioDevice(ctx->sdl.audio_device, 0);
    log_info("begin play audio");
    return 0;
}

int64_t get_audio_playtime(Context *ctx) {
    AVRational base = ctx->ffmpeg.format->streams[ctx->audio_index]->time_base;
    int queue_size = SDL_GetQueuedAudioSize(ctx->sdl.audio_device);

    // 计算缓存的音频可以播放的时间(单位毫秒)，需要减去当前帧的时间
    int64_t buf_time = (int64_t)(queue_size - ctx->audio_buff_size)*ctx->audio_nb_samples*1000/(ctx->audio_buff_size*ctx->audio_sample_rate);

    //计算当前音频的播放时间(单位毫秒)，当前帧的pts乘以时间基得到当前帧的播放时间，然后减去缓存的时间
    int64_t play_time = ((ctx->audio_pts*1000*base.num)/base.den) - buf_time;
    log_debug("playtime:%ld buf_time:%ld  pts:%ld", play_time, buf_time, ctx->audio_pts);
    return play_time;
}

int decode_audio(void *arg) {
    Context *ctx = arg;
    int pcount = 0;
    int err = 0;
    AVCodecContext *codec = NULL;
    const AVCodec *decoder = NULL;

    // 音频初始化
    codec = avcodec_alloc_context3(NULL);
    if (!codec) {
        log_error("avcodec_alloc_context3 error!");
        return -1;
    }

    err = avcodec_parameters_to_context(codec, ctx->ffmpeg.format->streams[ctx->audio_index]->codecpar);
    if (err != 0) {
        log_error("avcodec_parameters_to_context error!");
        goto fail;
    }

    //查找解码器
    decoder = avcodec_find_decoder(codec->codec_id);
    if (!decoder) {
        log_error("avcodec_find_decoder error!");
        goto fail;
    }

    //打开解码器
    err = avcodec_open2(codec, decoder, NULL);
    if (err != 0) {
        log_error("avcodec_open2 error!");
        goto fail;
    }
    ctx->ffmpeg.audio_codec = codec;

    init_audio(ctx);

    // 配置音频格式转换器
    struct SwrContext *swr_ctx = NULL;
    swr_alloc_set_opts2(&swr_ctx, 
                        &(codec->ch_layout),
                        AV_SAMPLE_FMT_S16,
                        codec->sample_rate,
                        &(codec->ch_layout),
                        codec->sample_fmt,
                        codec->sample_rate,
                        0, 
                        NULL);
    swr_init(swr_ctx);

    while (false == ctx->quit) {
        if (ctx->pause) {
            SDL_Delay(10*SELLP_MS);
            continue;
        }

        AVPacket *packet = rqueue_read(ctx->apacket_queue);
        if (packet == NULL) {
            SDL_Delay(SELLP_MS);
            log_debug("rqueue_read apacket_queue null");
            continue;
        }

        err = avcodec_send_packet(codec, packet);
        if (err < 0) {
            log_error("avcodec_send_packet error:%d packet:%x", err, packet);
        }
        av_packet_unref(packet);
        av_packet_free(&packet);

        while (err == 0) {

            AVFrame *frame = av_frame_alloc();
            err = avcodec_receive_frame(codec, frame);
            if (err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
                av_frame_free(&frame);
                break;
            } else if (err < 0) {
                log_error("Failed to receive frame from audio decoder");
                 goto fail;
            }

            // 计算输出 采样数
            int out_samples = av_rescale_rnd(swr_get_delay(swr_ctx, frame->sample_rate) + frame->nb_samples,
                                            frame->sample_rate,
                                            frame->sample_rate,               
                                            AV_ROUND_UP);


            int buf_size = av_samples_get_buffer_size(NULL, frame->ch_layout.nb_channels, out_samples, AV_SAMPLE_FMT_S16, 0);
            uint8_t *out_buf = av_malloc(buf_size);
            if (!out_buf) {
                log_error("av_malloc failed");
                 goto fail;
            }

            // 执行格式转换
            swr_convert(swr_ctx, &out_buf, out_samples, (const uint8_t **)frame->data, frame->nb_samples);

            // 将音频数据写入 SDL 音频缓冲区
            if (SDL_QueueAudio(ctx->sdl.audio_device, out_buf, buf_size) < 0) {
                log_error("SDL_QueueAudio Failed");
                 goto fail;
            }

            int queue_size = SDL_GetQueuedAudioSize(ctx->sdl.audio_device);

            ctx->audio_pts = frame->pts;
            ctx->audio_nb_samples = frame->nb_samples;
            ctx->audio_sample_rate = frame->sample_rate;
            ctx->audio_buff_size = buf_size;

            // nb_samples 这个frame包含的采样数
            // sample_rate 音频每秒钟的采样数
            log_debug("%d %d %d %d %d %d %d pts %d dts %d", 
                frame->nb_samples, frame->ch_layout.nb_channels, frame->sample_rate, frame->format, out_samples, buf_size, queue_size, 
                frame->pts, frame->pkt_dts);

            // 缓存一秒钟的音频数据
            if (queue_size > (frame->sample_rate*buf_size)/frame->nb_samples) {
                SDL_Delay(50*SELLP_MS);
            }

            av_frame_unref(frame);
            av_frame_free(&frame);
            av_free(out_buf);
        }

        pcount++;
    }

    log_info("decode audio frame count:%d", pcount);

fail:
    if (codec) avcodec_free_context(&codec);
    if (swr_ctx) swr_free(&swr_ctx);
    SDL_CloseAudioDevice(ctx->sdl.audio_device);

    return err;
}


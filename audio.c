#include "audio.h"
#include "log.h"
#include "util.h"

int init_audio(void *arg) {
    Context *ctx = arg;

    // 设置音频参数
    SDL_AudioSpec audio_spec;
    audio_spec.freq = ctx->ffmpeg.audio_codec->sample_rate;
    audio_spec.channels = ctx->ffmpeg.audio_codec->ch_layout.nb_channels;
    audio_spec.format = AUDIO_S16SYS;
    audio_spec.samples = 1024;
    audio_spec.callback = NULL;
    audio_spec.userdata = NULL;

    // 打开音频设备
    ctx->sdl.audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
    if (ctx->sdl.audio_device == 0) {
        fprintf(stderr, "Failed to open audio device");
        return -1;
    }

    // 开始播放音频
    SDL_PauseAudioDevice(ctx->sdl.audio_device, 0);
    return 0;
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
        AVPacket *packet = rqueue_read(ctx->apacket_queue);
        if (packet == NULL) {
            SDL_Delay(SELLP_MS);
            log_debug("rqueue_read apacket_queue null");
            continue;
        }

        err = avcodec_send_packet(codec, packet);
        if (err < 0) {
            log_error("Failed to send packet to audio decoder");
            return -1;
        }

        while (err >= 0) {

            AVFrame *frame = av_frame_alloc();
            err = avcodec_receive_frame(codec, frame);
            if (err == AVERROR(EAGAIN) || err == AVERROR_EOF) {
                break;
            } else if (err < 0) {
                log_error("Failed to receive frame from audio decoder");
                return -1;
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
                return -1;
            }

            // 执行格式转换
            swr_convert(swr_ctx, &out_buf, out_samples, (const uint8_t **)frame->data, frame->nb_samples);

            // 将音频数据写入 SDL 音频缓冲区
            if (SDL_QueueAudio(ctx->sdl.audio_device, out_buf, buf_size) < 0) {
                log_error("SDL_QueueAudio Failed");
                return -1;
            }

            log_info("%d %d %d %d %d %d", frame->nb_samples, frame->ch_layout.nb_channels, frame->sample_rate, frame->format, out_samples, buf_size);

            av_frame_unref(frame);
            av_frame_free(&frame);
            av_free(out_buf);

            SDL_Delay(10);
        }

        pcount++;
        av_packet_unref(packet);
        av_packet_free(&packet);
    }
    log_info("decode audio frame count:%d", pcount);

fail:
    if (codec) {
        avcodec_free_context(&codec);
    }
    return err;
}


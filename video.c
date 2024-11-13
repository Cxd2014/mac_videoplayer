


#include "video.h"
#include "log.h"
#include "util.h"

int init_ffmpeg(Context *ctx) {
    int ret = -1;
    AVFormatContext *format = NULL;
    log_debug("init_ffmpeg...");

    //初始化FormatContext
    format = avformat_alloc_context();
    if (!format) {
        log_error("alloc format context error");
        goto fail;
    }

    // 打开视频文件
    ret = avformat_open_input(&format, ctx->ffmpeg.filename, NULL, NULL);
    if (ret < 0) {
        log_error("avformat_open_input error:%d", ret);
        goto fail;
    }

    //读取媒体文件信息
    ret = avformat_find_stream_info(format, NULL);
    if (ret != 0) {
        log_error("avformat_find_stream_info error:%d", ret);
        goto fail;
    }

    // 输出视频文件信息
    av_dump_format(format, 0, ctx->ffmpeg.filename, 0);

    //寻找到视频流的下标
    ctx->video_index = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    ctx->audio_index = av_find_best_stream(format, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

    ctx->ffmpeg.format = format;
    ctx->ffmpeg.width = format->streams[ctx->video_index]->codecpar->width;
    ctx->ffmpeg.height = format->streams[ctx->video_index]->codecpar->height;
    log_info("video pix %dx%d", ctx->ffmpeg.width, ctx->ffmpeg.height);
    
    return 0;

fail:
    if (format) {
        avformat_close_input(&format);
    }
    
    return ret;
}

int read_thread(void *arg) {
    Context *ctx = arg;
    AVFormatContext *format = ctx->ffmpeg.format;
    int ret = 0;

    int vcount = 0;
    int acount = 0;

    log_debug("read_thread...");
    while(false == ctx->quit) {
        AVPacket *packet = av_packet_alloc();
        if (!packet) {
            log_error("av_packet_alloc error");
            return -1;
        }

        ret = av_read_frame(format, packet);
        if (ret < 0) {
            // 文件读完
            log_debug("av_read_frame error:%d",ret);
            av_packet_free(&packet);
            break;
        } else {
            // 视频数据
            if (packet->stream_index == ctx->video_index) {
                do { // 写入环形队列
                    ret = rqueue_write(ctx->vpacket_queue, packet);
                    if (ret != 0) { // 队列满了
                        SDL_Delay(SELLP_MS);
                        log_debug("rqueue_write vpacket_queue error:%d",ret);
                    }
                } while (ret != 0 && ctx->quit == false);
                vcount++;
            } else if (packet->stream_index == ctx->audio_index) {
                do {
                    ret = rqueue_write(ctx->apacket_queue, packet);
                    if (ret != 0) {
                        SDL_Delay(SELLP_MS);
                        log_debug("rqueue_write apacket_queue error:%d",ret);
                    }
                } while (ret != 0 && ctx->quit == false);
                acount++;
            }
        }
    }

    log_info("read video packet:%d audio packet:%d", vcount, acount);
    return 0;
}

static enum AVPixelFormat hw_pix_fmt;
static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }

    log_error("Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}

int hw_decoder_init(AVCodecContext *codec, const AVCodec *decoder) {
    int i = 0;
    AVBufferRef *hw_device_ctx = NULL;
    enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;

    while((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
        log_info("support DeviceType:%s", av_hwdevice_get_type_name(type));

    type = av_hwdevice_find_type_by_name("videotoolbox");
    for (i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
        if (!config) {
            log_error("Decoder %s does not support device type %s.\n",
                    decoder->name, av_hwdevice_get_type_name(type));
            return -1;
        }

        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == type) {
            hw_pix_fmt = config->pix_fmt;
            log_info("hw_pix_fmt:%d", config->pix_fmt);
            break;
        }
    }
    codec->get_format  = get_hw_format;

    if ((i = av_hwdevice_ctx_create(&hw_device_ctx, type,
                                      NULL, NULL, 0)) < 0) {
        log_error("Failed to create specified HW device. %d\n", i);
        return i;
    }
    codec->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    return 0;
}

int decode_video(void *arg) {
    Context *ctx = arg;
    int ret = 0;
    int fcount = 0;
    int pcount = 0;
    AVCodecContext *codec;

    log_debug("decode_video...");
    // 视频初始化
    codec = avcodec_alloc_context3(NULL);
    if (!codec) {
        log_error("avcodec_alloc_context3 error");
        goto fail;
    }

    //将视频流的编解码信息拷贝到codec中
    ret = avcodec_parameters_to_context(codec, ctx->ffmpeg.format->streams[ctx->video_index]->codecpar);
    if (ret != 0) {
        log_error("avcodec_parameters_to_context error:%d", ret);
        goto fail;
    }

    //查找解码器
    const AVCodec *decoder = avcodec_find_decoder(codec->codec_id);
    if (!decoder) {
        log_error("avcodec_find_decoder error");
        goto fail;
    }

    //初始化硬件解码
    if (hw_decoder_init(codec, decoder) < 0) {
        log_error("hw_decoder_init error");
        goto fail;
    }

    //打开解码器
    ret = avcodec_open2(codec, decoder, NULL);
    if (ret != 0) {
        log_error("avcodec_open2 error:%d", ret);
        goto fail;
    }
    ctx->ffmpeg.video_codec = codec;
    
    while (false == ctx->quit) {
        int64_t start = get_now_millisecond();
        AVPacket *packet = rqueue_read(ctx->vpacket_queue);
        if (packet == NULL) {
            SDL_Delay(SELLP_MS);
            log_debug("rqueue_read vpacket_queue null");
            continue;
        }
        pcount++;

        // 发送给解码器
        ret = avcodec_send_packet(codec, packet);
        if (ret < 0) {
            log_error("avcodec_send_packet error:%d", ret);
            goto fail;
        } else {
            av_packet_unref(packet);
            av_packet_free(&packet);
        }

        while (false == ctx->quit) {
            // 从解码器中循环读取帧数据,直到读取失败
            AVFrame *frame = NULL, *sw_frame = NULL;
            if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc())) {
                log_error("Can not alloc frame\n");
                ret = AVERROR(ENOMEM);
                goto fail;
            }

            ret = avcodec_receive_frame(codec, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_frame_free(&frame);
                av_frame_free(&sw_frame);
                break;
            } else if (ret < 0) {
                log_error("Error while decoding:%d\n", ret);
                goto fail;
            }
            
            log_debug("Frame %c pts %d dts %d", av_get_picture_type_char(frame->pict_type), frame->pts, frame->pkt_dts);
            AVFrame *final_frame = NULL;
            if (frame->format == hw_pix_fmt) {
                /* retrieve data from GPU to CPU */
                if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
                    log_error("Error transferring the data to system memory\n");
                    goto fail;
                }
                final_frame = sw_frame;
                final_frame->pts = frame->pts;
                av_frame_unref(frame);
                av_frame_free(&frame);
            } else {
                final_frame = frame;
                av_frame_unref(sw_frame);
                av_frame_free(&sw_frame);
            }

            fcount++;
            do { // 写入环形队列
                ret = rqueue_write(ctx->vframe_queue, final_frame);
                if (ret != 0) {
                    SDL_Delay(SELLP_MS);
                    log_debug("rqueue_write vframe_queue error:%d",ret);
                }
            } while (ret != 0 && ctx->quit == false);
            log_debug("decode video packet:%d frame:%d duration:%d", pcount, fcount, calc_duration(start));
        }
    }

    log_info("decode video packet:%d frame:%d", pcount, fcount);
fail:
    if (codec) {
        av_buffer_unref(&(codec->hw_device_ctx));
        avcodec_free_context(&codec);
    }
    return ret;
}

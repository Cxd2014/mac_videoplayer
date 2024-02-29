


#include "video.h"
#include "log.h"


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
                        SDL_Delay(10);
                    }
                } while (ret != 0 && ctx->quit == false);
                vcount++;
            } else if (packet->stream_index == ctx->audio_index) {
                do {
                    ret = rqueue_write(ctx->apacket_queue, packet);
                    if (ret != 0) {
                        SDL_Delay(10);
                    }
                } while (ret != 0 && ctx->quit == false);
                acount++;
            }
        }
    }

    log_info("read video packet:%d audio packet:%d", vcount, acount);
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

    //将视频流的的编解码信息拷贝到codec中
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

    //打开解码器
    ret = avcodec_open2(codec, decoder, NULL);
    if (ret != 0) {
        log_error("avcodec_open2 error:%d", ret);
        goto fail;
    }
    ctx->ffmpeg.video_codec = codec;
    
    while (false == ctx->quit) {

        AVPacket *packet = rqueue_read(ctx->vpacket_queue);
        if (packet == NULL) {
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

        // 从解码器中循环读取帧数据,直到读取失败
        while (ctx->quit == false) {
            
            AVFrame *frame = av_frame_alloc();
            if (!frame) {
                log_error("av_frame_alloc error");
                goto fail;
            }

            ret = avcodec_receive_frame(codec, frame);
            if (ret < 0) {
                av_frame_free(&frame);
                break;
            }

            fcount++;
            do { // 写入环形队列
                ret = rqueue_write(ctx->vframe_queue, frame);
                if (ret != 0) {
                    log_debug("rqueue_write error:%d",ret);
                }
            } while (ret != 0 && ctx->quit == false);
        }
    }

    log_info("decode video packet:%d frame:%d", pcount, fcount);
fail:
    if (codec) {
        avcodec_free_context(&codec);
    }
    return ret;
}


int decode_audio(void *arg) {
    Context *ctx = arg;
    int pcount = 0;

    while (false == ctx->quit) {
        AVPacket *packet = rqueue_read(ctx->apacket_queue);
        if (packet == NULL) {
            SDL_Delay(10);
            continue;
        }

        pcount++;
        av_packet_unref(packet);
        av_packet_free(&packet);
    }

    log_info("decode audio frame count:%d", pcount);
    return 0;
}

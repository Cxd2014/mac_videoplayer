
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>

#include "video.h"
#include "log.h"


int init_ffmpeg(const char *fileName) {
    log_info("init_ffmpeg...");
    
    //初始化FormatContext
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    if (!pFormatCtx) {
        printf("alloc format context error!\n");
        return __LINE__;
    }

    // 打开视频文件
    int err = avformat_open_input(&pFormatCtx, fileName, NULL, NULL);
    if (err < 0) {
        printf("avformat_open_input error!\n");
        return __LINE__;
    }

    //读取媒体文件信息
    err = avformat_find_stream_info(pFormatCtx, NULL);
    if (err != 0) {
        printf("[error] find stream error!");
        return __LINE__;
    }

    // 输出视频文件信息
    av_dump_format(pFormatCtx, 0, fileName, 0);

    return 0;
}
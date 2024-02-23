cmake_minimum_required(VERSION 3.10)
project(video_player)

set(CMAKE_C_STANDARD 11)

find_package(SDL2 REQUIRED)

set(FFMPEG_INCLUDE_DIR "/opt/homebrew/opt/ffmpeg/include")
set(FFMPEG_LIBRARY_DIR "/opt/homebrew/opt/ffmpeg/lib")

# 查找FFmpeg库
find_library(AVCODEC_LIBRARY avcodec HINTS ${FFMPEG_LIBRARY_DIR})
find_library(AVFORMAT_LIBRARY avformat HINTS ${FFMPEG_LIBRARY_DIR})
find_library(AVUTIL_LIBRARY avutil HINTS ${FFMPEG_LIBRARY_DIR})
find_library(SWSCALE_LIBRARY swscale HINTS ${FFMPEG_LIBRARY_DIR})
find_library(AVFILTER_LIBRARY avfilter HINTS ${FFMPEG_LIBRARY_DIR})
find_library(AVDEVICE_LIBRARY avdevice HINTS ${FFMPEG_LIBRARY_DIR})
find_library(SWRESAMPLE_LIBRARY swresample HINTS ${FFMPEG_LIBRARY_DIR})

add_executable(video_player 
    main.c
    sdl_window.c
    video.c
    rqueue.c
    log.c
)

target_link_libraries(video_player
    SDL2::SDL2
    ${AVCODEC_LIBRARY} 
    ${AVFORMAT_LIBRARY} 
    ${AVUTIL_LIBRARY} 
    ${SWSCALE_LIBRARY}
    ${AVFILTER_LIBRARY} 
    ${AVDEVICE_LIBRARY} 
    ${SWRESAMPLE_LIBRARY}
)

target_include_directories(video_player PRIVATE ${FFMPEG_INCLUDE_DIR})

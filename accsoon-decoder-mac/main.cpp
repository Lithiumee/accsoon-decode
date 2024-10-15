#include "common.hpp"
#include "accsoon_tcp_control.h"
#include "accsoon_udp_stream.h"

const int SHM_SIZE = 1024 * 1024 * 2;
const char *SHM_NAME = "video_frame_shared_mem";
void *SHM_ADDR = NULL;

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

int ffmpegInitDecoder();
int SDLInitLiveView();
void ffmpegUpdateFrame(char *frame, int frame_size);

// 创建信号量
boost::interprocess::interprocess_semaphore frame_sem(0);

const char *streaming_path = "udp://localhost:8554";

// Decoder input
AVCodecContext *dec_ctx = NULL;
AVPacket *dec_input_packet = NULL;
AVFrame *dec_output_frame = NULL; // Frame 为解码后的音视频数据
// Streamsink
AVFormatContext *streaming_ctx = NULL;
AVOutputFormat *streaming_fmt = NULL;
AVStream *streaming_video_stream = NULL;
AVCodecParameters *streaming_video_codecpar = NULL;
// SDL
SDL_Event e;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;

uint32_t first_frame_flag = 1;
std::atomic<bool> stop_threads(false);

int main()
{
    spdlog::set_level(spdlog::level::info);

    try
    {
        boost::interprocess::shared_memory_object::remove(SHM_NAME);
    }
    catch (const std::exception &e)
    {
        spdlog::info("no old shm");
    }

    boost::interprocess::shared_memory_object shm(boost::interprocess::create_only, SHM_NAME, boost::interprocess::read_write);
    shm.truncate(SHM_SIZE);
    boost::interprocess::mapped_region region(shm, boost::interprocess::read_write);
    SHM_ADDR = region.get_address();

    std::thread thread_tcp(tcpThread);
    std::thread thread_udp(udpThread);

    av_log_set_level(AV_LOG_ERROR);
    avdevice_register_all();

    if (ffmpegInitDecoder() < 0)
    {
        spdlog::error("Failed to initialize decoder");
        return -1;
    }

    if (SDLInitLiveView() < 0)
    {
        spdlog::error("Failed to initialize SDL");
        return -1;
    }

    uint8_t buffer_flag = 0;
    while (!stop_threads)
    {
        frame_sem.wait();
        spdlog::debug("sem triggered");
        if (buffer_flag)
            ffmpegUpdateFrame((char *)SHM_ADDR + SHM_SIZE / 2 + 4, *(int *)((char *)SHM_ADDR + SHM_SIZE / 2));
        else
            ffmpegUpdateFrame((char *)SHM_ADDR + 4, *(int *)((char *)SHM_ADDR));
        buffer_flag = !buffer_flag;
    }

    thread_tcp.join();
    thread_udp.join();
    return 0;
}

void ffmpegUpdateFrame(char *frame, int frame_size)
{
    if (first_frame_flag)
    {
        if (frame_size < 5)
        {
            spdlog::warn("wrong frame size");
            return;
        }
        if (frame[4] != 0x67)
        {
            spdlog::info("wait for first frame");
            return;
        }
        first_frame_flag = 0;
    }

    spdlog::debug("size {} ", frame_size);
    int ret = av_packet_from_data(dec_input_packet, (uint8_t *)frame, frame_size);
    if (ret < 0)
    {
        spdlog::error("av_packet_from_data failed");
        return;
    }

    ret = avcodec_send_packet(dec_ctx, dec_input_packet);
    if (ret < 0)
    {
        spdlog::error("avcodec_send_packet failed");
        return;
    }

    ret = avcodec_receive_frame(dec_ctx, dec_output_frame);
    if (ret < 0)
    {
        spdlog::error("avcodec_receive_frame failed");
        return;
    }

    spdlog::debug("IN width {} height {} pixfmt {} pts{}", dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt, dec_output_frame->pts);

    SDL_UpdateYUVTexture(texture, NULL, dec_output_frame->data[0], dec_output_frame->linesize[0], dec_output_frame->data[1], dec_output_frame->linesize[1], dec_output_frame->data[2], dec_output_frame->linesize[2]);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    av_frame_unref(dec_output_frame);
    SDL_PollEvent(&e);
}

int ffmpegInitDecoder()
{
    spdlog::info("ffmpegInitDecoder");
    int ret = 0;
    AVCodec *idec = (AVCodec *)avcodec_find_decoder(AV_CODEC_ID_H264);
    dec_ctx = avcodec_alloc_context3(idec);
    if (!dec_ctx)
    {
        spdlog::error("avcodec_alloc_context3 failed");
        return -1;
    }

    ret = avcodec_open2(dec_ctx, idec, NULL);
    if (ret < 0)
    {
        spdlog::error("avcodec_open2 failed");
        avcodec_free_context(&dec_ctx);
        return -1;
    }

    dec_input_packet = av_packet_alloc();
    if (!dec_input_packet)
    {
        spdlog::error("av_packet_alloc failed");
        avcodec_free_context(&dec_ctx);
        return -1;
    }

    dec_output_frame = av_frame_alloc();
    if (!dec_output_frame)
    {
        spdlog::error("av_frame_alloc failed");
        av_packet_free(&dec_input_packet);
        avcodec_free_context(&dec_ctx);
        return -1;
    }

    return 0;
}

int SDLInitLiveView()
{
    spdlog::info("SDLInitLiveView");
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        spdlog::error("SDL_Init failed: {}", SDL_GetError());
        return -1;
    }

    window = SDL_CreateWindow("LiveView", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window)
    {
        spdlog::error("SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer)
    {
        spdlog::error("SDL_CreateRenderer failed: {}", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!texture)
    {
        spdlog::error("SDL_CreateTexture failed: {}", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    return 0;
}
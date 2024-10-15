#include <iostream>
#include <stdint.h>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
/**
 * @dependencies:
 *  boost
 *  spdlog
 */
#include <boost/asio.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>

#include "spdlog/spdlog.h"

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include "libavutil/log.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
#include "libavutil/pixfmt.h"
#ifdef __cplusplus
};
#endif

extern std::atomic<bool> stop_threads;
#define BUFFER_SIZE (1024 * 1024)
#define IP_ADDRESS "10.0.0.1"

#define USE_SPDLOG

#ifdef USE_BLOG
#define LOG(level, fmt, ...) blog(level, fmt, ##__VA_ARGS__)
#elif defined(USE_SPDLOG)
#define LOG(level, fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif
extern const int SHM_SIZE;
extern const char *SHM_NAME;
extern void *SHM_ADDR;

using namespace boost::asio;
extern boost::interprocess::interprocess_semaphore frame_sem;
#include <iostream>
#include <stdint.h>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "gst/gst.h"
#include "gst/app/gstappsrc.h"

extern std::atomic<bool> stop_threads;
#define BUFFER_SIZE (1024*1024)
#define IP_ADDRESS "10.0.0.1"

#define USE_SPDLOG

#ifdef USE_BLOG
    #define LOG(level, fmt, ...) blog(level, fmt, ##__VA_ARGS__)
#elif defined(USE_SPDLOG)
    #define LOG(level, fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif
extern GstElement *pipeline, *appsrc, *decoder, *sink;
extern GstBus *bus;
extern GstMessage *msg;
extern GstStateChangeReturn ret;
extern GMainLoop *loop;
extern GstBuffer *gst_buffer;

#include <iostream>
#include <stdint.h>
#include "stdio.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
/**
 * @dependencies: 
 *  gstreamer
 *  obs
*/

//gsst
#include "gst/gst.h"
#include "gst/app/gstappsrc.h"
#include "gst/app/gstappsink.h"

//obs
#include <obs-module.h>
#include <util/platform.h>


extern std::atomic<bool> stop_threads;
#define BUFFER_SIZE (1024*1024)
#define IP_ADDRESS "10.0.0.1"
#define USE_BLOG

#ifdef USE_BLOG
    #define LOG(level, fmt, ...) blog(level, fmt, ##__VA_ARGS__)
#else
    #define LOG(level, fmt, ...) ()
#endif

extern GstElement *pipeline, *appsrc, *decoder, *sink;
extern GstBus *bus;
extern GstMessage *msg;
extern GstStateChangeReturn ret;
extern GMainLoop *loop;
extern GstBuffer *gst_buffer;

#include "stdio.h"

#include "accsoon_tcp_control.h"
#include "accsoon_udp_stream.h"
#include "common.hpp"

GstElement *pipeline, *appsrc, *decoder, *sink, *parse;
GstBus *bus;
GstMessage *msg;
GstStateChangeReturn ret;
GMainLoop *loop;

GstBuffer *gst_buffer;

#define VIDEO_WIDTH 1920
#define VIDEO_HEIGHT 1080
#define VIDEO_FORMAT "RGB16"
#define PIXEL_SIZE 2
std::atomic<bool> stop_threads(false);

void signalHandler(int signum)
{
    stop_threads.store(true);
    g_main_loop_quit(loop);
}

void cb_need_data(GstElement *appsrc, guint unused_size, gpointer user_data)
{
    // spdlog::info( "need data\n" );
}
void cb_enough_data(GstElement *appsrc, gpointer user_data)
{
    // spdlog::info( "enough data\n" );
}
int gstInit()
{
    /* Initialize GStreamer */
    gst_init(NULL, NULL);
    loop = g_main_loop_new(NULL, FALSE);

    /* Create the elements */
    appsrc = gst_element_factory_make("appsrc", "source");
    if (!appsrc)
    {
        g_print("appsrc could not be created. Exiting.\n");
        return -1;
    }
    parse = gst_element_factory_make("h264parse", "parse");
    if (!parse)
    {
        g_print("decoder could not be created. Exiting.\n");
        return -1;
    }
    decoder = gst_element_factory_make("omxh264dec", "decoder");
    g_object_set(decoder, "latency-mode", 2, NULL);
    // g_object_set ( decoder, "qos", FALSE, NULL);
    if (!decoder)
    {
        g_print("decoder could not be created. Exiting.\n");
        return -1;
    }
    sink = gst_element_factory_make("sunxifbsink", "videosink");
    if (!sink)
    {
        g_print("sink could not be created. Exiting.\n");
        return -1;
    }
    g_object_set(G_OBJECT(appsrc), "max-bytes", BUFFER_SIZE, NULL);
    g_object_set(G_OBJECT(appsrc), "is-live", TRUE, NULL);
    // g_object_set(G_OBJECT(sink), "video-memory", 16, NULL);
    g_object_set(G_OBJECT(sink), "sync", FALSE, NULL);
    g_object_set(G_OBJECT(sink), "qos", FALSE, NULL);

    // g_object_set(G_OBJECT(sink), "pan-does-vsync", TRUE, NULL);
    // g_object_set(G_OBJECT(sink), "full-screen", TRUE, NULL);
    // g_object_set(G_OBJECT(sink), "fps", 30, NULL);
    g_object_set(G_OBJECT(sink), "buffer-pool", TRUE, NULL);

    /* Create the pipeline */
    pipeline = gst_pipeline_new("test-pipeline");
    if (!pipeline)
    {
        g_print("pipeline could not be created. Exiting.\n");
        return -1;
    }
    /* Build the pipeline */
    gst_bin_add_many(GST_BIN(pipeline), appsrc, parse, decoder, sink, NULL);
    if (!gst_element_link_many(appsrc, parse, decoder, sink, NULL))
    {
        g_print("Failed to link elements. Exiting.\n");
        return -1;
    }

    /* Set appsrc */
    GstCaps *caps = gst_caps_new_simple("video/x-h264",
                                        "width", G_TYPE_INT, 1920,
                                        "height", G_TYPE_INT, 1080,
                                        "framerate", GST_TYPE_FRACTION, 30, 1,
                                        "alignment", G_TYPE_STRING, "nal",
                                        "stream-format", G_TYPE_STRING, "byte-stream",
                                        NULL);
    g_object_set(appsrc, "caps", caps, NULL);
    g_object_set(appsrc, "format", GST_FORMAT_TIME, NULL);
    g_object_set(appsrc, "stream-type", GST_APP_STREAM_TYPE_STREAM, "format", GST_FORMAT_BYTES, NULL);
    g_object_set(appsrc, "min-latency", (GstClockTime)0, NULL);
    g_object_set(appsrc, "is-live", TRUE, "do-timestamp", TRUE, NULL);
    // g_object_set ( appsrc, "do-timestamp", TRUE, NULL );
    // g_object_set ( appsrc, "stream-type", 0, "format", GST_FORMAT_TIME, NULL );
    g_signal_connect(appsrc, "need-data", G_CALLBACK(cb_need_data), NULL);
    g_signal_connect(appsrc, "enough-data", G_CALLBACK(cb_enough_data), NULL);

    return 0;
}
int main(int argc, char *argv[])
{
    signal(SIGINT, signalHandler);
    if(gstInit() < 0)
    {
        return -1;
    }
    stop_threads.store(false);

    std::thread thread_tcp(tcpThread);
    std::thread thread_udp(udpThread);

    GstClock *clock = gst_system_clock_obtain();
    g_object_set(clock, "clock-type", GST_CLOCK_TYPE_MONOTONIC, NULL);
    gst_pipeline_use_clock(GST_PIPELINE(pipeline), clock);

    /* play */
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    // g_print("init time %lu %lu",gst_element_get_base_time(appsrc),gst_clock_get_time (gst_system_clock_obtain ()));
    g_main_loop_run(loop);

    /* clean up */
    LOG(LOG_INFO, "accsoon stopping");
    stop_threads.store(true);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));
    g_main_loop_unref(loop);

    thread_tcp.join();
    thread_udp.join();

    return 0;
}
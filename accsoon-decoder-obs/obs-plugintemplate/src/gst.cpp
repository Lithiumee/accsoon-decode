#include "accsoon_tcp_control.h"
#include "accsoon_udp_stream.h"
#include "common.hpp"

GstElement *pipeline, *appsrc, *decoder, *sink, *conv, *filt;
GstBus *bus;
GstMessage *msg;
GstStateChangeReturn ret;
GMainLoop *loop;
GstBuffer *gst_buffer;

void cb_need_data(GstElement *appsrc, guint unused_size, gpointer user_data)
{
    UNUSED_PARAMETER(unused_size);
    UNUSED_PARAMETER(user_data);
    UNUSED_PARAMETER(appsrc);
}
void cb_enough_data(GstElement *appsrc, gpointer user_data)
{
    UNUSED_PARAMETER(user_data);
    UNUSED_PARAMETER(appsrc);
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
    decoder = gst_element_factory_make("vaapih264dec", "decoder");
    if (!decoder)
    {
        g_print("decoder could not be created. Exiting.\n");
        return -1;
    }
    LOG(LOG_INFO, "decoder: vaapih264dec");
    // decoder = gst_element_factory_make ( "videoconvert", "conv" );
    // for avdec_h264
    // g_object_set ( decoder, "std-compliance", 0, NULL );
    g_object_set(decoder, "qos", FALSE, NULL);
    // for vaapih264dec
    g_object_set(decoder, "low-latency", TRUE, NULL);

    // conv = gst_element_factory_make("videoconvert", "conv");
    // filt = gst_element_factory_make("capsfilter", "filt");
    sink = gst_element_factory_make("appsink", "videosink");
    if (!sink)
    {
        g_print("sink could not be created. Exiting.\n");
        // return -1;
    }
    g_object_set(G_OBJECT(appsrc), "max-bytes", BUFFER_SIZE, NULL);
    g_object_set(G_OBJECT(appsrc), "is-live", TRUE, NULL);
    g_object_set(G_OBJECT(sink), "sync", FALSE, NULL);

    /* Create the pipeline */
    pipeline = gst_pipeline_new("test-pipeline");
    if (!pipeline)
    {
        g_print("pipeline could not be created. Exiting.\n");
        return -1;
    }

    /* Set appsrc */
    GstCaps *caps = gst_caps_new_simple("video/x-h264",
                                        "format", G_TYPE_STRING, "I420",
                                        "width", G_TYPE_INT, 1920,
                                        "height", G_TYPE_INT, 1080,
                                        "framerate", GST_TYPE_FRACTION, 30, 1,
                                        "alignment", G_TYPE_STRING, "nal",
                                        "stream-format", G_TYPE_STRING, "byte-stream",
                                        NULL);
    GstCaps *sink_caps = gst_caps_new_simple("video/x-raw",
                                             "format", G_TYPE_STRING, "I420",
                                             NULL);
    g_object_set(appsrc, "caps", caps, NULL);
    g_object_set(sink, "caps", sink_caps, NULL);
    // g_object_set(filt, "caps", sink_caps, NULL);
    g_object_set(appsrc, "stream-type", GST_APP_STREAM_TYPE_STREAM, "format", GST_FORMAT_BYTES, NULL);
    g_object_set(appsrc, "is-live", TRUE, "do-timestamp", TRUE, NULL);
    g_object_set(appsrc, "min-latency", (GstClockTime)0, NULL);
    g_signal_connect(appsrc, "need-data", G_CALLBACK(cb_need_data), NULL);
    g_signal_connect(appsrc, "enough-data", G_CALLBACK(cb_enough_data), NULL);

    /* Build the pipeline */
    // gst_bin_add_many(GST_BIN(pipeline), appsrc, decoder, conv, filt, sink, NULL);
    gst_bin_add_many(GST_BIN(pipeline), appsrc, decoder, sink, NULL);
    // if (!gst_element_link_many(appsrc, decoder, conv, filt, sink, NULL))
    if (!gst_element_link_many(appsrc, decoder, sink, NULL))
    {
        g_print("Elements could not be linked. Exiting.\n");
        return -1;
    }

    GstClock *clock = gst_system_clock_obtain();
    g_object_set(clock, "clock-type", GST_CLOCK_TYPE_MONOTONIC, NULL);
    gst_pipeline_use_clock(GST_PIPELINE(pipeline), clock);

    LOG(LOG_INFO, "gst init done");

    /* play */
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(loop);

    return 0;
}
extern obs_source_t *g_source;
void gstThread()
{
    LOG(LOG_INFO, "gst thread begin");
    uint64_t start_time = os_gettime_ns();
    uint32_t first_sample_flag = 1;
    GstSample *sample, *sample_old = NULL;
    GstMapInfo map;
    GstBuffer *recved_buffer;
    struct obs_source_frame frame;
    frame.width = 1920;
    frame.height = 1088;
    frame.format = VIDEO_FORMAT_I420;
    frame.linesize[0] = 1920;     // Y plane
    frame.linesize[1] = 1920 / 2; // U plane
    frame.linesize[2] = 1920 / 2; // V plane
    video_format_get_parameters_for_format(VIDEO_CS_709, VIDEO_RANGE_PARTIAL, frame.format, (float *)&frame.color_matrix, (float *)&frame.color_range_min, (float *)&frame.color_range_max);
    while (!stop_threads.load())
    {
        if (first_sample_flag)
        {
            first_sample_flag = 0;
        }
        else
        {
            sample_old = sample;
        }
        sample = gst_app_sink_pull_sample((GstAppSink *)sink);
        if (sample_old)
        {
            gst_buffer_unmap(recved_buffer, &map);
            gst_sample_unref(sample_old);
        }
        if (!sample)
        {
            // LOG(LOG_INFO, "sample null");
            continue;
        }
        recved_buffer = gst_sample_get_buffer(sample);
        gst_buffer_map(recved_buffer, &map, GST_MAP_READ);
        // LOG(LOG_INFO, "size %lu", map.size);
        frame.data[0] = map.data;                                                               // Y plane
        frame.data[1] = map.data + frame.width * frame.height;                                  // U plane
        frame.data[2] = map.data + frame.width * frame.height + frame.width * frame.height / 4; // V plane
        // LOG(LOG_INFO, "data %lu %02x %02x ", sizeof(map.data), frame.data[0][10], frame.data[0][11]);
        frame.timestamp = os_gettime_ns() - start_time;
        obs_source_output_video(g_source, &frame);
    }
    LOG(LOG_INFO, "gst thread exit");
}
std::thread thread_gst, thread_tcp, thread_udp, thread_gst2;
std::atomic<bool> stop_threads(false);
extern "C" int accsoon_gst_init()
{
    LOG(LOG_INFO, "accsoon thread begin");
    stop_threads.store(false);
    thread_gst = std::thread(gstInit);
    thread_tcp = std::thread(tcpThread);
    thread_udp = std::thread(udpThread);
    thread_gst2 = std::thread(gstThread);
    thread_gst.detach();
    thread_tcp.detach();
    thread_udp.detach();
    thread_gst2.detach();

    LOG(LOG_INFO, "accsoon thread started");

    return 0;
}
extern "C" int accsoon_gst_deinit()
{
    LOG(LOG_INFO, "accsoon stopping");
    stop_threads.store(true);
    /* clean up */
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));
    g_main_loop_unref(loop);

    return 0;
}
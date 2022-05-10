
// started with
// https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html?gi-language=c#appsrc-example
// used ideas from
// https://gist.github.com/nzjrs/725122/16ceee88aafae389bab207818e0661328921e1ab

#include <stdio.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

static GMainLoop *loop;

static void
cb_need_data(GstElement *appsrc,
             guint unused_size,
             gpointer user_data)
{
    static gboolean white = FALSE;
    static GstClockTime timestamp = 0;
    GstBuffer *buffer;
    guint size;
    GstFlowReturn ret;

    size = 385 * 288 * 2;

    buffer = gst_buffer_new_allocate(NULL, size, NULL);

    /* this makes the image black/white */
    gst_buffer_memset(buffer, 0, white ? 0xff : 0x0, size);

    white = !white;

    GST_BUFFER_PTS(buffer) = timestamp;
    //  GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, 2); //half second
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, 15); // fifteenth

    timestamp += GST_BUFFER_DURATION(buffer);

    g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
    gst_buffer_unref(buffer);
    printf("push\n");
    if (ret != GST_FLOW_OK)
    {
        printf("fail\n");

        /* something wrong, stop pushing */
        g_main_loop_quit(loop);
    }
}

gint main(gint argc,
          gchar *argv[])
{
    GstElement *pipeline, *appsrc;
    GstCaps *caps;
    char *p;

    /* init GStreamer */
    gst_init(&argc, &argv);
    loop = g_main_loop_new(NULL, FALSE);

    // nope
    p = "appsrc name=mysource ! video/x-raw,width=384,height=288,bpp=24,depth=16 ! x264enc ! rtph264pay ! udpsink host=127.0.0.1 port=1234";

    // no appsrc testing
    p = "videotestsrc ! \
video/x-raw,width=320,height=240,framerate=15/1 ! \
videoscale ! videorate ! videoconvert ! timeoverlay ! \
vp8enc error-resilient=1 ! \
rtpvp8pay ! udpsink host=127.0.0.1 port=5004";

    p = "appsrc name=mysource ! \
video/x-raw,width=384,height=288,bpp=16,depth=16,framerate=15/1 ! ";

    p = "appsrc name=mysource ! \
video/x-raw,width=384,height=288,bpp=16,depth=16,framerate=15/1 ! \
videorate ! videoconvert ! timeoverlay ! \
vp8enc error-resilient=1 ! \
rtpvp8pay ! udpsink host=127.0.0.1 port=5004";

    pipeline = gst_parse_launch(p, NULL);
    g_assert(pipeline);

    appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "mysource");
    g_assert(appsrc);
    g_assert(GST_IS_APP_SRC(appsrc));

    /* set the caps on the source */
    caps = gst_caps_new_simple("video/x-raw",
                               "bpp", G_TYPE_INT, 16,
                               "depth", G_TYPE_INT, 16,
                               "width", G_TYPE_INT, 384,
                               "height", G_TYPE_INT, 288,
                               "framerate", GST_TYPE_FRACTION, 15, 1,
                               NULL);
    gst_app_src_set_caps(GST_APP_SRC(appsrc), caps);

#if 0
    /* setup pipeline */
    pipeline = gst_pipeline_new("pipeline");
    appsrc = gst_element_factory_make("appsrc", "source");
    conv = gst_element_factory_make("videoconvert", "conv");
    videosink = gst_element_factory_make("xvimagesink", "videosink");

    /* setup */
    g_object_set(G_OBJECT(appsrc), "caps",
                 gst_caps_new_simple("video/x-raw",
                                     "format", G_TYPE_STRING, "RGB16",
                                     "width", G_TYPE_INT, 384,
                                     "height", G_TYPE_INT, 288,
                                     "framerate", GST_TYPE_FRACTION, 0, 1,
                                     NULL),
                 NULL);
    gst_bin_add_many(GST_BIN(pipeline), appsrc, conv, videosink, NULL);
    gst_element_link_many(appsrc, conv, videosink, NULL);
#endif

    /* setup appsrc */
    g_object_set(G_OBJECT(appsrc),
                 "stream-type", 0,
                 "format", GST_FORMAT_TIME, NULL);
    g_signal_connect(appsrc, "need-data", G_CALLBACK(cb_need_data), NULL);

    /* play */
    printf("playing\n");
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(loop);

    /* clean up */
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));
    g_main_loop_unref(loop);

    return 0;
}
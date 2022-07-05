
// started with
// https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html?gi-language=c#appsrc-example
// used ideas from
// https://gist.github.com/nzjrs/725122/16ceee88aafae389bab207818e0661328921e1ab

#include <stdio.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

// socket stuff
//  inspired by https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpclient.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

//#define BUFSIZE 2000

int width = 1280;
int height = 720;

static GMainLoop *loop;

int sockfd;                    // globals are cool
struct sockaddr_in serveraddr; // more globals are even more cool

/*
 * error - wrapper for perror
 */
void error(char *msg);
void error(char *msg)
{
    perror(msg);
    exit(0);
}

static void
cb_need_data(GstElement *appsrc,
             guint unused_size,
             gpointer user_data)
{
    static gboolean white = FALSE;
    static GstClockTime timestamp = 0;
    static uint32_t I = 0;
    GstBuffer *buffer;
    guint size;
    GstFlowReturn ret;

    size = width * height * 2;

    buffer = gst_buffer_new_allocate(NULL, size, NULL);

    /* this makes the image black/white */

    // gst_buffer_memset(buffer, 0, white ? 0xff : 0x0, size);

    /* this makes the image black/white */
    // for (size_t i = 0; i < height; i++)
    // {
    //     I = 1664525 * I + 1013904223;
    //     white = I <= 0x7FFFFFFF;
    //     // white = ((i + z) / 16) % 2;
    //     gst_buffer_memset(buffer, i * width * 2, white ? 0xff : 0x0, width * 2);
    //     for (size_t j= 0; j < width; j++)
    //     {
    //         buffer[i * width * 2 + j * 2] = white;
    //         buffer[i * width * 2 + j * 2] = white;
    //     }

    //     // z++;
    //     white = !white;
    // }

    GstMapInfo map;
    guint16 *ptr, t;

    if (gst_buffer_map(buffer, &map, GST_MAP_WRITE))
    {
        ptr = (guint16 *)map.data;
        /* invert data */
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                if (x%4==0){
  I = 1664525 * I + 1013904223;
                white = I <= 0x7FFFFFFF;
                }
              

                // t = ptr[384 - 1 - x];
                // ptr[384 - 1 - x] = ptr[x];
                // ptr[x] = t;
                ptr[x] = white ? 0xffff : 0x0;
            }
            ptr += width;
        }
        gst_buffer_unmap(buffer, &map);
    }

    white = !white;

    GST_BUFFER_PTS(buffer) = timestamp;
    //  GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, 2); //half second
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, 15); // fifteenth

    timestamp += GST_BUFFER_DURATION(buffer);

    printf("push\n");
#if 1
    g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
    gst_buffer_unref(buffer); // original example included unref call
#else
    // this is an alternative, but not sure why would need this approach
    ret = gst_app_src_push_buffer((GstAppSrc *)appsrc, buffer);
    // this seems to work better without unref call
#endif

    if (ret != GST_FLOW_OK)
    {
        printf("fail\n");

        /* something wrong, stop pushing */
        g_main_loop_quit(loop);
    }
}

void *frame_thread(void *p);
void *frame_thread(void *p)
{
    while (1)
    {
        printf("push frame, dchan\n");
        cb_need_data(p, 0, 0);
        // gst_webrtc_data_channel_send_string(p, "xx");

        socklen_t serverlen = sizeof(serveraddr);
        ssize_t n = sendto(sockfd, "wow", strlen("wow"), 0, &serveraddr, serverlen);
        if (n < 0)
            error("ERROR in sendto");

        // sleep(1);
        usleep((useconds_t)(33 * 1000));
    }
}

gint main(gint argc,
          gchar *argv[])
{

    // int serverlen;

    struct hostent *server;
    char *hostname = "127.0.0.1";
    int portno = 5007;

    /* check command line arguments */
    if (argc == 3)
    {
        // fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        // exit(0);
        hostname = argv[1];
        portno = atoi(argv[2]);
    }
    fprintf(stdout, "network  <hostname>:%s <port>:%d\n", hostname, portno);
    // exit(0);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* get a message from the user */
    // bzero(buf, BUFSIZE);
    // printf("Please enter msg: ");
    // fgets(buf, BUFSIZE, stdin);

    /* send the message to the server */
    // serverlen = sizeof(serveraddr);
    // n = sendto(sockfd, "wow", strlen("wow"), 0, &serveraddr, serverlen);
    // if (n < 0)
    //     error("ERROR in sendto");

    // end of networking
    // start of GSTREAMER stuff

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
rtpvp8pay ! udpsink host=127.0.0.1 port=5006";

    p = "appsrc name=mysource ! \
videorate ! videoconvert ! timeoverlay ! \
vp8enc threads=6 deadline=2 error-resilient=1 ! \
rtpvp8pay pt=96 ssrc=2 ! queue ! application/x-rtp,media=video,encoding-name=VP8,payload=96 ! udpsink host=127.0.0.1 port=5006";

    pipeline = gst_parse_launch(p, NULL);
    g_assert(pipeline);

    appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "mysource");
    g_assert(appsrc);
    g_assert(GST_IS_APP_SRC(appsrc));

    /* set the caps on the source */
    caps = gst_caps_new_simple("video/x-raw",
                               "bpp", G_TYPE_INT, 16,
                               "format", G_TYPE_STRING, "RGB16",
                               "depth", G_TYPE_INT, 16,
                               "width", G_TYPE_INT, width,
                               "height", G_TYPE_INT, height,
                               "framerate", GST_TYPE_FRACTION, 30, 1,
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
    //  g_signal_connect(appsrc, "need-data", G_CALLBACK(cb_need_data), NULL);

    pthread_t id;
    pthread_create(&id, NULL, frame_thread, appsrc);

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
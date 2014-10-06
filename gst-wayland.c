/* gstreamer-wayland sample program to crop and scale the video
 * Copyright (C) <2013> Intel Corporation
 * Copyright (C) <2013> Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*compile: gcc gst-wayland.c -o gst-wayland `pkg-config --cflags --libs gstreamer-1.0 gstreamer-video-1.0 wayland-client` */

#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include <wayland-client.h>
#include <gst/video/videooverlay.h>

typedef struct App {
  GstElement *playbin;
  GstElement *video_sink;
  GstVideoOverlay *video_overlay;
  GMainLoop *loop;
  gchar *uri;
  guint resize_width;
  guint resize_height;
  guint x;
  guint y;
}App;

static gboolean
resize (gpointer user_data)
{
    App *app = (App *)user_data;
    app->resize_width += 10;
    app->resize_height += 10;
    app->x += 10;
    app->y += 10;
    gst_video_overlay_set_render_rectangle (app->video_overlay,
          app->x, app->y,
          app->resize_width, app->resize_height);
    gst_video_overlay_expose (app->video_overlay);
   
}

static GstBusSyncReply
sync_handler (GstBus *bus, GstMessage *message, gpointer user_data)
{
    App *app = (App *)user_data;

    if (!gst_is_video_overlay_prepare_window_handle_message (message))
        return GST_BUS_PASS;

    app->video_overlay = GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (message));
    gst_message_unref (message);
    return GST_BUS_DROP;   
}

static void
app_destroy (App *app)
{
    gst_element_set_state (app->playbin, GST_STATE_NULL);
    gst_object_unref (GST_OBJECT(app->playbin));
    g_main_loop_unref (app->loop);
    g_free (app->uri);
    g_free (app);
}
 
static void
app_create (App *app, char *uri)
{
    app->playbin = gst_element_factory_make ("playbin", "play-bin");
    app->video_sink = gst_element_factory_make ("vaapisink", "vaapi-sink");
    g_object_set (G_OBJECT (app->playbin), "video-sink", app->video_sink, NULL);

    app->loop = g_main_loop_new (NULL, FALSE);
    app->uri = g_strdup_printf ("file://%s", uri);
    g_object_set (G_OBJECT (app->playbin), "uri", app->uri, NULL);

    app->resize_width = 320;
    app->resize_width = 240;
}

int main (int argc, char *argv[])
{
    GstBus *bus;
    App *app;

    if (argc < 2) {
      g_error ("specifiy a filename to play \n");
      return;
    }
      
    gst_init (NULL, NULL);
    
    app = (App *)malloc (sizeof (App));
    app_create (app, argv[1]);

    bus = gst_pipeline_get_bus (GST_PIPELINE (app->playbin));
    gst_bus_set_sync_handler (GST_BUS (bus), sync_handler, app, NULL);
    gst_object_unref (bus);

    g_timeout_add (1500, (GSourceFunc)resize, app);

    gst_element_set_state (app->playbin, GST_STATE_PLAYING);
    g_main_loop_run (app->loop);

    app_destroy (app);
}

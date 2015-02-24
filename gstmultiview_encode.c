/* multiview_encode
 *
 * Copyright (C) <2013> Sreerenj Balachandran <sreerenjb@gnome.org>
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

/*compiling: gcc gstmulitview_encode.c -o gstmultiview_encode `pkg-config --cflags --libs gstreamer-1.0` */

#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
static GstElement *pipeline = NULL;
static GMainLoop *loop = NULL;
static GstBus *bus;

static void do_encode(void)
{
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
}


static gboolean message_handler(GstBus *bus,
                                  GstMessage *msg,
                                  gpointer data)
{

    switch (GST_MESSAGE_TYPE(msg))
    {
        case GST_MESSAGE_ERROR:
        {
            gchar *debug;
            GError *err;

            debug = NULL;
            gst_message_parse_error(msg, &err, &debug);
            g_message("gst error: domain = %d, code = %d, "
                    "message = '%s', debug = '%s'",
                    err->domain, err->code, err->message, debug);
            g_main_loop_quit(loop);
            break;
        }
        case GST_MESSAGE_EOS:
            g_message("got EOS");
            gst_element_set_state(pipeline, GST_STATE_NULL);
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_BUFFERING:
            g_message("got BUFFERING");
            break;
        case GST_MESSAGE_ELEMENT:
            g_message ("element: %s",gst_element_get_name (GST_MESSAGE_SRC(msg)));
            break;
        case GST_MESSAGE_STATE_CHANGED:
            break;
        default:
            break;
    }

    return TRUE;
}

int main(int argc, char *argv[])
{
    
    GstElement *src, *enc, *parse, *sink;
    gst_init(&argc, &argv);

    src = gst_element_factory_make ("videotestsrc", "src");
    enc = gst_element_factory_make ("vaapiencode_h264", "enc");
    parse = gst_element_factory_make ("vaapiparse_h264", "parse");
    sink = gst_element_factory_make ("filesink", "sink");
    pipeline = gst_pipeline_new ("pipeline");

    g_object_set (G_OBJECT(src), "num-buffers", 3000, NULL);
    g_object_set (G_OBJECT(enc), "num-views", 10, NULL);
    g_object_set (G_OBJECT(sink), "location", "sample.mvc", NULL);

    {
      guint i;
      GValueArray *view_ids = g_value_array_new (10);
      
      for (i = 0; i < 10; i++) {
        GValue val = {0, };
        g_value_init (&val, G_TYPE_UINT);
        g_value_set_uint (&val, (i*10));
        g_value_array_append (view_ids, &val);
      }
      
      g_object_set (G_OBJECT(enc), "view-ids", (GValueArray*)view_ids, NULL);
    }
    {
      guint i;
      GValueArray *array;
      g_message ("GetProperties:");
      g_object_get (G_OBJECT(enc), "view-ids", &array, NULL);
      for (i = 0; i < 10; i++) {
	GValue *a;
        guint m;
        a = g_value_array_get_nth (array, i);
        m = g_value_get_uint (a);
        printf (" %d",m);
      }
      printf ("\n");
    }
    gst_bin_add_many (GST_BIN(pipeline), src, enc, parse, sink, NULL);
    gst_element_link_many (src, enc, parse, sink, NULL); 
    
    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, message_handler, NULL);

    loop = g_main_loop_new(NULL, FALSE);
    
    do_encode();
    g_main_loop_run(loop);

    g_message("exiting...");

    return 0;

}

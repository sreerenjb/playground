/* gst-seek sample program to check seeking
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

/*compiling: gcc gst-seek.c -o gst-seek `pkg-config --cflags --libs gstreamer-1.0` */

#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static    GstElement *playbin = NULL;
static    GstElement *sink = NULL;
static    gint seek_position;
 
static GMainLoop *loop = NULL;

static void do_play(void)
{
    gst_element_set_state(playbin, GST_STATE_PLAYING);
}


static void do_seek(int *abs_position)
{
    seek_position += *abs_position;

    g_message("SEEKING TO %d seconds", seek_position);

    gst_element_seek(playbin,
                     1.0,
                     GST_FORMAT_TIME,
                     GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH,
                     GST_SEEK_TYPE_SET,
                     seek_position * GST_SECOND,
                     GST_SEEK_TYPE_NONE,
                     GST_CLOCK_TIME_NONE);

}


static void handle_state_changed(GstMessage *msg,
                                 gpointer data)
{
    GstState newstate, oldstate;

    gst_message_parse_state_changed(msg, &oldstate, &newstate, NULL);
    g_debug ("State changed: %d -> %d", oldstate, newstate);

    switch (GST_STATE_TRANSITION(oldstate, newstate))
    {
        case GST_STATE_CHANGE_READY_TO_PAUSED:
            break;
        case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
            break;
        case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
            break;
        case GST_STATE_CHANGE_PAUSED_TO_READY:
            break;
        default:
            break;
    }

}

static gboolean async_bus_handler(GstBus *bus,
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
            g_debug("gst error: domain = %d, code = %d, "
                    "message = '%s', debug = '%s'",
                    err->domain, err->code, err->message, debug);
            g_main_loop_quit(loop);
            break;
        }
        case GST_MESSAGE_EOS:
            g_debug("got EOS");
            gst_element_set_state(playbin, GST_STATE_NULL);
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_BUFFERING:
            g_debug("got BUFFERING");
            break;
        case GST_MESSAGE_ELEMENT:
            break;
        case GST_MESSAGE_STATE_CHANGED:
            if ((GstElement *)GST_MESSAGE_SRC(msg) == playbin)
            {
                handle_state_changed(msg, NULL);
            }
            break;
        default:
            break;
    }

    return TRUE;
}

static gboolean construct_pipeline(const gchar *uri)
{

       
    GstBus *bus = NULL;

    playbin = gst_element_factory_make ("playbin", "play-bin");

    if (!playbin)
    {
        g_critical("Failed to create pipeline");
        return FALSE;
    }

    g_object_set (playbin, "uri", uri, NULL);

    bus = gst_pipeline_get_bus(GST_PIPELINE(playbin));

    gst_bus_add_watch_full(bus,
                           G_PRIORITY_HIGH,
                           (GstBusFunc)async_bus_handler,
                           NULL, 
                           NULL);

    return TRUE;

}


int main(int argc, char *argv[])
{
    
    gchar *uri = NULL;
    gint position;

    gst_init(&argc, &argv);

    if (argc != 3)
    {
        g_warning("Give position to seek and exactly one playbale URI.");
        return 1;
    }

    position = atoi(argv[2]);
    uri = g_strdup_printf ("file://%s",argv[1]);

    g_debug("Using URI : %s", uri);

    if (!construct_pipeline(uri))
    {
        g_warning("Unable construct pipeline.");
        return 1;
    }

    do_play();

    g_timeout_add (3000,(GSourceFunc)do_seek, &position);

    loop = g_main_loop_new(NULL, FALSE);
    
    g_main_loop_run(loop);

    g_free (uri);
    g_debug("exiting...");

    return 0;

}

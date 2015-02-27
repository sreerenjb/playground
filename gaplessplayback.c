/* gaplessplayback: sample program to check gapless playback
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

/*compile: gcc gaplessplayback.c -o gaplessplayback `pkg-config --cflags --libs gstreamer-1.0` */

#include <stdio.h>
#include <gst/gst.h>
#include <string.h>

static GMainLoop *loop;
static GstElement *pipeline;
static GstBus *bus;
static gchar *uri_list[10];
static int count;
static int num_uri;

set_next_uri (GstElement * obj, gpointer userdata)
{
  if (count < num_uri) {
    g_message ("Setting uri %s", uri_list[++count]);
    g_object_set (G_OBJECT (pipeline), "uri", uri_list[count], NULL);
  }
  else
    gst_element_post_message (pipeline, gst_message_new_eos(GST_OBJECT(pipeline)));
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
            gst_element_set_state(pipeline, GST_STATE_NULL);
            g_main_loop_quit(loop);
            break;
        default:
            break;
    }

    return TRUE;
}

int
main (int argc, char *argv[])
{
  guint i;

  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);

  /* Check input arguments */
  if (argc < 2) {
    g_printerr ("Usage: %s <list of space separated URIs>\n",argv[0]);
    return -1;
  }

  for (i = 0; (i<argc  && i <10); i++)
    uri_list[i] = g_strdup (argv[i]);

  num_uri = i-1;
  count = 1;
  g_message ("Num of Input URIs %d", num_uri);

  pipeline = gst_element_factory_make ("playbin", "Playbin");

  g_signal_connect (pipeline, "about-to-finish",
      G_CALLBACK (set_next_uri), NULL);

  /* Set initial URI */
  g_object_set (G_OBJECT (pipeline), "uri", argv[1], NULL);

  bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));

  gst_bus_add_watch_full(bus,
                           G_PRIORITY_HIGH,
                           (GstBusFunc)async_bus_handler,
                           NULL,
                           NULL);

  g_print ("Playing: %s ", argv[1]);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_main_loop_run (loop);

  /* Out of the main loop, clean up nicely */
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  g_print ("Deleting pipeline\n");
  gst_object_unref (GST_OBJECT (pipeline));
  g_main_loop_unref (loop);

  for (i = 0; (i<argc-1  && i <10); i++)
    g_free (uri_list[i]);

  return 0;
}

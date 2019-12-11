#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/video/navigation.h>

typedef struct _CustomData
{
  GstElement *pipeline;
  GstElement *video_enc;
  GMainLoop *loop;
  guint32 bitrate;
} AppData;

static void
change_bitrate (AppData * data)
{
  gboolean res = FALSE;

  g_object_set (data->video_enc, "bitrate", data->bitrate, NULL);
  g_print ("Chaning the bitrate to : %d\n", data->bitrate);
}

static void
cb_msg_eos (GstBus * bus, GstMessage * msg, gpointer data)
{
  AppData *app = data;
  g_message ("got eos");
  g_main_loop_quit (app->loop);
}

static void
keyboard_cb (const gchar * key, AppData * data)
{
  switch (g_ascii_tolower (key[0])) {
    case 'r':
      // increase the bitrate by 50%
      data->bitrate = data->bitrate + (data->bitrate/2);
      change_bitrate (data);
      break;
    case 's':{
      // decrease the bitrate by 50%
      data->bitrate = data->bitrate - (data->bitrate/2);
      change_bitrate (data);
      break;
    }
    case 'q':
      {
        GstMessage *msg;

        gst_element_send_event (data->pipeline, gst_event_new_eos ());
        msg = gst_bus_timed_pop_filtered (GST_ELEMENT_BUS (data->pipeline),
          GST_CLOCK_TIME_NONE, GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
        gst_element_set_state (data->pipeline, GST_STATE_NULL); 
        g_main_loop_quit (data->loop);
        break;
      }
    default:
      break;
  }
}

/* Process keyboard input */
static gboolean
handle_keyboard (GIOChannel * source, GIOCondition cond, AppData * data)
{
  gchar *str = NULL;

  if (g_io_channel_read_line (source, &str, NULL, NULL,
          NULL) != G_IO_STATUS_NORMAL) {
    return TRUE;
  }

  keyboard_cb (str, data);
  g_free (str);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  AppData data;
  GstStateChangeReturn ret;
  GIOChannel *io_stdin;
  GError *err = NULL;
  guint srcid;
  GstBus *bus;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Print usage map */
  g_print ("USAGE: Choose one of the following options, then press enter:\n"
      " 'r' to increase the bit rate by 50 percentage \n"
      " 's' to decrease the bit rate by 50 percentage \n" " 'Q' to quit\n");

  data.bitrate = 1024;

  data.pipeline = gst_parse_launch ("videotestsrc num-buffers=1500 ! video/x-raw, format=NV12, width=1920, height=1080, framerate=30/1 ! vaapivp9enc rate-control=cbr bitrate=1024 name=enc ! webmmux !  filesink location=sample.webm", &err);
  if (err) {
    g_printerr ("failed to create pipeline: %s\n", err->message);
    g_error_free (err);
    return -1;
  }

  data.video_enc = gst_bin_get_by_name (GST_BIN (data.pipeline), "enc");

  /* Add a keyboard watch so we get notified of keystrokes */
  io_stdin = g_io_channel_unix_new (fileno (stdin));
  g_io_add_watch (io_stdin, G_IO_IN, (GIOFunc) handle_keyboard, &data);

  bus = gst_pipeline_get_bus (GST_PIPELINE (data.pipeline));
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);
  gst_bus_enable_sync_message_emission (bus);
  g_signal_connect (bus, "message::eos", G_CALLBACK (cb_msg_eos), &data);
  gst_object_unref (bus);

  /* Start playing */
  ret = gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    goto bail;
  }

  /* Create a GLib Main Loop and set it to run */
  data.loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (data.loop);

  gst_element_set_state (data.pipeline, GST_STATE_NULL);

bail:
  /* Free resources */
  //g_source_remove (srcid);
  g_main_loop_unref (data.loop);
  g_io_channel_unref (io_stdin);

  gst_object_unref (data.video_enc);
  gst_object_unref (data.pipeline);

  return 0;
}

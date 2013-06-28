/* gstreamer-clutter sample program to display video using ClutterTexture
 * Copyright (C) <2013> Sreerenj Balachandran <bsreerenj@gmail.com>
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

/* comiple: gcc clutter-gst.c -o clutter-gst `pkg-config --cflags --libs clutter-1.0 gstreamer-1.0 gstreamer-app-1.0` */

#include <stdio.h>
#include <stdlib.h>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#ifndef CLUTTER_DISABLE_DEPRECATION_WARNINGS
#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#endif

#include <clutter/clutter.h>

typedef struct {
    ClutterActor *stage;
    ClutterTimeline *timeline;
    CoglPixelFormat cogl_pixel_format;
    ClutterTextureFlags texture_flags;
    ClutterActor *texture[1000];

    guint width;
    guint height;
    guint row_stride;
} ClutterData;

typedef struct {
    GstElement *pipeline;
    GstElement *fsrc;
    GstElement *decbin;
    GstElement *dec_queue;
    GstElement *vconvert;
    GstElement *vscale;
    GstElement *vfilter;
    GstElement *app_queue;
    GstElement *vsink;
  
    GstBuffer *buf;
 
    GstBus *bus;
    GstCaps *caps;
    gchar *uri;
    
} GstData;

static GMutex g_mutex;
static GCond g_cond;
static gboolean is_rendered;

static ClutterData clut;
static GstData gst;

static gdouble rotation = 0;

static 
gboolean bus_watch (GstBus *bus, GstMessage *msg, gpointer data)
{
    return TRUE;
}

static void
pad_added_cb (GstElement *decbin, GstPad *pad, gpointer data)
{
    GstCaps *caps;
    GstPad *q_sink_pad;
    GstStructure *st;

    GstData *gst = (GstData *)data;

    q_sink_pad = gst_element_get_static_pad(gst->dec_queue,"sink");
    if(GST_PAD_IS_LINKED(q_sink_pad)) {
        g_object_unref (q_sink_pad);
  	return;
    }

    caps = gst_pad_query_caps (pad, NULL);
    st = gst_caps_get_structure (caps, 0);
    if(!g_strrstr(gst_structure_get_name (st), "video")) {
        gst_caps_unref (caps);
        g_object_unref (q_sink_pad);
        return;
    }

    gst_caps_unref (caps);

    gst_pad_link (pad, q_sink_pad);

    g_object_unref (q_sink_pad);
}

static void
render_buffer (gpointer user_data)
{
    GstBuffer *buffer = NULL;
    GstMapInfo map_info;
    
    g_mutex_lock (&g_mutex);
    
    rotation += 0.3;

    if (gst.buf) {
	int i;
	int w = 0;
        buffer = gst.buf;
	gst_buffer_map (buffer, &map_info, GST_MAP_READ);
	for (i=0; i<5; i++) {
 	    if (!clutter_texture_set_from_rgb_data ((ClutterTexture *)clut.texture[i],
			  			    (const guchar *)map_info.data,
					   	    FALSE,
					   	    clut.width,
					            clut.height,
	  					    clut.row_stride,
					   	    3,
 	 					    clut.texture_flags,
					   	    NULL
					  	    )) {
 	         fprintf (stderr, "Failed to set texture data \n");
	  	 exit(0);
	     }
   	 if (i%2 == 0)
             clutter_actor_set_rotation_angle (clut.texture[i], CLUTTER_Z_AXIS, rotation*4);
 	 else if (i == 1)
             clutter_actor_set_rotation_angle (clut.texture[i], CLUTTER_Y_AXIS, rotation*4);
	 else
             clutter_actor_set_rotation_angle (clut.texture[i], CLUTTER_X_AXIS, rotation*4);

	 clutter_actor_set_position(clut.texture[i], i+w, 100);
	 clutter_actor_set_pivot_point(clut.texture[i], 0.5, 0.5);
	
	 w += 250;
 	}
	gst_buffer_unmap (buffer, &map_info);
        is_rendered = TRUE;
        gst.buf = NULL;
        g_cond_signal (&g_cond);
    }
    g_mutex_unlock (&g_mutex);
}

static GstFlowReturn
new_sample_cb (GstElement *element, gpointer user_data)
{
    GstSample *sample;

    GstAppSink *app_sink = (GstAppSink *)element;

    g_mutex_lock (&g_mutex);
    sample= gst_app_sink_pull_sample (app_sink);
    gst.buf = gst_sample_get_buffer (sample);

    while (!is_rendered)
	g_cond_wait (&g_cond, &g_mutex);
    is_rendered = FALSE;

    g_mutex_unlock (&g_mutex);

    gst_sample_unref (sample);

    return GST_FLOW_OK;
}

static gboolean
setup_gst_pipeline (GstData *gst)
{
    gst->buf = NULL;

    gst->pipeline = gst_pipeline_new ("pipeline");
    gst->fsrc = gst_element_factory_make ("filesrc", "FileSrc-1.0");
    gst->decbin = gst_element_factory_make ("decodebin", "DecBin-1.0");
    gst->dec_queue = gst_element_factory_make ("queue", "DecBin-Queue-1.0");
    gst->vfilter = gst_element_factory_make ("capsfilter", "CapsFilter-1.0");
    gst->vscale = gst_element_factory_make ("videoscale", "VideoScale-1.0");
    gst->vconvert = gst_element_factory_make ("videoconvert", "VideoConvert-1.0");
    gst->app_queue = gst_element_factory_make ("queue", "Appsink-Queue-1.0");
    gst->vsink = gst_element_factory_make ("appsink", "AppSink-1.0");

    gst_bin_add_many (GST_BIN(gst->pipeline), gst->fsrc, gst->decbin, gst->dec_queue,
		      gst->vfilter, gst->vconvert, gst->vscale, gst->app_queue, 
		      gst->vsink, NULL);
    
    g_object_set (G_OBJECT (gst->fsrc), "location", gst->uri, NULL);

    gst->caps = gst_caps_new_simple ("video/x-raw",
                                "format", G_TYPE_STRING, "RGB",
                                "width", G_TYPE_INT, 200,
                                "height", G_TYPE_INT, 200,
                                NULL);
    g_object_set (G_OBJECT (gst->vfilter), "caps", gst->caps, NULL);

    gst->bus = gst_pipeline_get_bus (GST_PIPELINE(gst->pipeline));
    gst_bus_add_watch (gst->bus, bus_watch, NULL);

    if (!gst_element_link_many (gst->fsrc, gst->decbin, NULL))
	return FALSE;

    if (!gst_element_link_many (gst->dec_queue, gst->vscale, gst->vconvert,
			   gst->vfilter, gst->app_queue, gst->vsink, NULL))
	return FALSE;

    g_signal_connect (gst->decbin, "pad-added", G_CALLBACK(pad_added_cb), gst);

    gst_app_sink_set_caps ((GstAppSink *)gst->vsink, (const GstCaps *)gst->caps);
    gst_app_sink_set_emit_signals ((GstAppSink *)gst->vsink, TRUE);

    g_signal_connect (gst->vsink, "new-sample", G_CALLBACK(new_sample_cb), &gst); 
    
    return TRUE;
}

static gboolean
setup_clutter (ClutterData *clut)
{
    gint i;
    clut->stage = clutter_stage_new ();
    clutter_stage_set_title (CLUTTER_STAGE (clut->stage), "Clutter-Gst-Texture");
    clutter_actor_set_size (clut->stage, 1280, 450);
    clutter_actor_show(clut->stage);

    for (i=0; i<10; i++) {
        clut->texture[i] = clutter_texture_new ();
        clutter_group_add (CLUTTER_GROUP(clut->stage), clut->texture[i]);
    }

    clut->texture_flags = CLUTTER_TEXTURE_NONE; 
    clut->cogl_pixel_format = COGL_PIXEL_FORMAT_RGB_888;
    clut->width = 200;
    clut->height = 200;
    clut->row_stride = GST_ROUND_UP_4 (clut->width * 3);

    clut->timeline = clutter_timeline_new(40);

    return TRUE; 
}

int main (int argc, char *argv[])
{
    
    if (argc < 2) {
        fprintf(stderr, "oops, please give a file to play\n");
        return 1;
    }

    gst_init (&argc, &argv);

    if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS) {
        fprintf (stderr, "Failed to initialize clutter library: \n");
        return EXIT_FAILURE;
    }

    gst.uri = g_strdup (argv[1]);
    
    if (!setup_gst_pipeline (&gst)){
	fprintf (stderr, "Failed to setup the gstreamer pipeline \n");
	return -1;
    }

    if (!setup_clutter (&clut)) {
        fprintf (stderr, "Failed to setup teh clutter actors \n");
	return -1;
    }

    g_signal_connect(clut.timeline, "new-frame", G_CALLBACK(render_buffer), NULL);
    clutter_timeline_set_repeat_count (clut.timeline, -1);
    clutter_timeline_start(clut.timeline);
    
    gst_element_set_state (gst.pipeline, GST_STATE_PLAYING);

    clutter_main();
  
    gst_element_set_state (gst.pipeline, GST_STATE_NULL);
    gst_object_unref (gst.pipeline);
    g_object_unref (clut.timeline);

    return EXIT_SUCCESS; 
}

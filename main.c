/* This file is part of monkey-bubble
 *
 * AUTHORS
 *     Sven Herzberg  <herzi@gnome-de.org>
 *
 * Copyright (C) 2008  Sven Herzberg
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <gst/gst.h>
#include <signal.h>

static GMainLoop* loop = NULL;

static void
siginthandler (int        signal,
	       siginfo_t* info,
	       void     * compat)
{
	g_main_loop_quit (loop);
}

static gboolean
bus_watch (GstBus    * bus,
	   GstMessage* message,
	   gpointer    data)
{
	switch (GST_MESSAGE_TYPE (message)) {
	case GST_MESSAGE_EOS:
		g_print ("=> end of stream\n");
		break;
	case GST_MESSAGE_ERROR:
		{
			GError* error = NULL;
			gchar * msg = NULL;
			gst_message_parse_error (message, &error, &msg);
			g_print ("%s\n%s\n", error->message, msg);
		}
		break;
	default:
		g_print ("Got message %s\n",
			 GST_MESSAGE_TYPE_NAME (message));
		break;
	}

	return TRUE;
}

static void
new_decoded_pad (GstElement* element,
		 GstPad    * pad,
		 gboolean    last,
		 GstElement* sink)
{
	g_print ("=> new pad: %s\n"
		 "   Capabilities: %s\n",
		 gst_pad_get_name (pad),
		 gst_caps_to_string (gst_pad_get_caps (pad))); // FIXME: fix memory

	gst_pad_link (pad, gst_element_get_pad (sink, "sink"));
}

static gboolean
timeout_cb (gpointer data)
{
	GstFormat format = GST_FORMAT_BYTES;
	gint64 position = 0;
	gint64 duration = 0;

	if (!gst_element_query_position (data, &format, &position)) {
		g_printerr ("error getting position\n");
		return TRUE;
	}

	if (format != GST_FORMAT_BYTES) {
		g_printerr ("position format wasn't in bytes\n");
		return TRUE;
	}

	if (!gst_element_query_duration (data, &format, &duration)) {
		g_printerr ("error getting duration\n");
		return TRUE;
	}

	if (format != GST_FORMAT_BYTES) {
		g_printerr ("duration format wasn't in bytes\n");
		return TRUE;
	}

	g_print ("%" G_GINT64_FORMAT "/%" G_GINT64_FORMAT "\r",
		 position,
		 duration);

	return TRUE;
}

int
main (int argc, char** argv)
{
	struct sigaction intaction;
	GstElement* bin;
	GstElement* src;
	GstElement* dec;
	GstElement* conv;
	GstElement* sink;
	GstBus* bus;

	gst_init (&argc, &argv);

	bin  = gst_pipeline_new ("pipeline0");
	src  = gst_element_factory_make ("filesrc",      "filesrc0");
	dec  = gst_element_factory_make ("decodebin",    "decodebin0");
	conv = gst_element_factory_make ("audioconvert", "audioconvert0");
	sink = gst_element_factory_make ("autoaudiosink","audiosink0");

	bus = gst_pipeline_get_bus (GST_PIPELINE (bin));
	gst_bus_add_watch (bus,
			   bus_watch,
			   NULL);
	gst_object_unref (bus);

	g_object_set (src,
		      "location", "game.ogg",
		      NULL);

	gst_bin_add_many (GST_BIN (bin), src, dec, conv, sink, NULL);
	gst_element_link_many (src, dec, NULL);
	gst_element_link_many (conv, sink, NULL);

	g_signal_connect (dec, "new-decoded-pad",
			  G_CALLBACK (new_decoded_pad), conv);

	intaction.sa_sigaction = siginthandler;
	intaction.sa_flags     = SA_SIGINFO; // | SA_RESETHAND;
	if (sigaction(SIGINT, &intaction, NULL) < 0) {
		g_warning ("Couldn't set up SIGINT handler");
	}

	gst_element_set_state (bin, GST_STATE_PLAYING);

	g_timeout_add (50, timeout_cb, src);

	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	gst_element_set_state (bin, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (bin));
	bin = NULL;
	src = NULL;
	sink = NULL;

	g_print ("\n");

	return 0;
}


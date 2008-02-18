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
#include <term.h>
#include <signal.h>

static GMainLoop* loop = NULL;

static void
clear_line (void)
{
	static char* ce = NULL;

	if (G_UNLIKELY (!ce)) {
		ce = tgetstr ("ce", NULL);
	}

	tputs (ce, 1, putchar);
}

static void
carriage__return (void)
{
	static char* cr = NULL;

	if (G_UNLIKELY (!cr)) {
		cr = tgetstr ("cr", NULL);
	}

	tputs (cr, 1, putchar);
}

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
	GstElement* pipeline = GST_ELEMENT (data);

	switch (GST_MESSAGE_TYPE (message)) {
	case GST_MESSAGE_EOS:
		g_print ("\n%s: %s\n",
			 GST_MESSAGE_TYPE_NAME (message),
			 G_OBJECT_TYPE_NAME (GST_MESSAGE_SRC (message)));
		break;
	case GST_MESSAGE_ERROR:
		{
			GError* error = NULL;
			gchar * msg = NULL;
			gst_message_parse_error (message, &error, &msg);
			g_print ("\n%s\n%s\n", error->message, msg);
		}
		break;
	case GST_MESSAGE_SEGMENT_DONE:
		g_print ("\n%s: %s\n",
			 GST_MESSAGE_TYPE_NAME (message),
			 G_OBJECT_TYPE_NAME (GST_MESSAGE_SRC (message)));
	        GstElement* elem = gst_bin_get_by_name (GST_BIN (pipeline), "audiosink0");
	        g_assert(GST_IS_ELEMENT(elem));
        	if (!gst_element_seek (elem, 1.0, GST_FORMAT_TIME,
        	        GST_SEEK_FLAG_SEGMENT, 
        	        GST_SEEK_TYPE_SET, G_GINT64_CONSTANT (0),
        	        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
			g_printerr ("\nseek failed\n");
		} else {
			g_printerr ("\nseeked back\n");
		}
                gst_object_unref (elem);
	        break;
	case GST_MESSAGE_STATE_CHANGED:
	        if (GST_MESSAGE_SRC (message) == GST_OBJECT (pipeline)) {
	                GstState oldstate,newstate,pending;
	                GstStateChangeReturn result;

                        gst_message_parse_state_changed(message,&oldstate,&newstate,&pending);

        		g_print ("\n%s: %s : %s -> %s\n",
        			 GST_MESSAGE_TYPE_NAME (message),
        			 G_OBJECT_TYPE_NAME (GST_MESSAGE_SRC (message)),
        			 gst_element_state_get_name(oldstate),gst_element_state_get_name(newstate));

                        switch(GST_STATE_TRANSITION(oldstate,newstate)) {
                                case GST_STATE_CHANGE_READY_TO_PAUSED: {
                                        GstElement* elem = gst_bin_get_by_name (GST_BIN (pipeline), "audiosink0");
                                        g_assert(GST_IS_ELEMENT(elem));
                                	if (!gst_element_seek (elem, 1.0, GST_FORMAT_TIME,
                                	        GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SEGMENT, 
                                	        GST_SEEK_TYPE_SET, G_GINT64_CONSTANT (0),
                                	        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
                                		g_printerr ("\ninitial seek failed\n");
                                	} else {
                                		g_printerr ("\ninitial seek set\n");
                                	}
                                	gst_object_unref (elem);
	                               	result = gst_element_set_state (pipeline, GST_STATE_PLAYING);
                                	g_print ("state change to PLAYING => %d\n", result);
                                	} break;
                	}
        	}
	        break;
	case GST_MESSAGE_CLOCK_PROVIDE:
	case GST_MESSAGE_NEW_CLOCK:
	case GST_MESSAGE_ASYNC_DONE:
	case GST_MESSAGE_TAG:
		/* we don't care */
		break;
	default:
		g_print ("\nGot message: %s\n",
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
	GstPadLinkReturn result;

	clear_line ();

	g_print ("=> new pad: %s\n"
		 "   Capabilities: %s\n",
		 gst_pad_get_name (pad),
		 gst_caps_to_string (gst_pad_get_caps (pad))); // FIXME: fix memory

	result = gst_pad_link (pad, gst_element_get_pad (sink, "sink"));
	g_print ("%d\n", result);
}

static gboolean
timeout_cb (gpointer data)
{
	GstFormat format = GST_FORMAT_TIME;
	gint64 position = 0;
	gint64 duration = 0;

	if (!gst_element_query_position (data, &format, &position)) {
		return TRUE;
	}

	if (format != GST_FORMAT_TIME) {
		return TRUE;
	}

	if (!gst_element_query_duration (data, &format, &duration)) {
		return TRUE;
	}

	if (format != GST_FORMAT_TIME) {
		return TRUE;
	}

	clear_line ();

	g_print ("%" GST_TIME_FORMAT "/%" GST_TIME_FORMAT " (%5.1f%%)",
		 GST_TIME_ARGS (position),
		 GST_TIME_ARGS (duration),
		 100.0 * position / duration);

	carriage__return ();

	return TRUE;
}

int
main (int argc, char** argv)
{
	struct sigaction intaction;
	GstElement* bin;
	GstElement* pipe;
	GstElement* src;
	GstElement* dec;
	GstElement* conv;
	GstElement* add;
	GstElement* sink;
	GstBus* bus;
	GstPad* pad;

	gst_init (&argc, &argv);

	int status = tgetent (NULL, g_getenv ("TERM"));
	if (status < 0) {
		g_warning ("Couldn't access the termcap database");
		return 1;
	} else if (status == 0) {
		g_warning ("Couldn't find terminal info for type \"%s\"",
			   g_getenv ("TERM"));
		return 1;
	}

	bin  = gst_pipeline_new ("pipeline0");
	pipe = gst_bin_new ("bin1");
	src  = gst_element_factory_make ("filesrc",       "filesrc1");
	dec  = gst_element_factory_make ("decodebin",     "decodebin1");
	conv = gst_element_factory_make ("audioconvert",  "audioconvert1");
	add  = gst_element_factory_make ("adder",         "adder0");
	sink = gst_element_factory_make ("autoaudiosink", "audiosink0");

	bus = gst_pipeline_get_bus (GST_PIPELINE (bin));
	gst_bus_add_watch (bus, bus_watch, bin);
	gst_object_unref (bus);

	g_object_set (src, "location", "game.ogg", NULL);

	gst_bin_add_many (GST_BIN (pipe), src, NULL);
	pad = gst_ghost_pad_new ("src",
				 gst_element_get_pad (src, "src"));
	gst_element_add_pad (pipe, pad);

	gst_bin_add_many (GST_BIN (bin), pipe,  dec, conv, add, sink, NULL);
	gst_element_link_many (pipe, dec, NULL);
	gst_element_link_many (conv, add, sink, NULL);

	g_signal_connect (dec, "new-decoded-pad",
			  G_CALLBACK (new_decoded_pad), conv);

	intaction.sa_sigaction = siginthandler;
	intaction.sa_flags     = SA_SIGINFO; // | SA_RESETHAND;
	if (sigaction(SIGINT, &intaction, NULL) < 0) {
		g_warning ("Couldn't set up SIGINT handler");
	}

	GstStateChangeReturn result;
	result = gst_element_set_state (bin, GST_STATE_PAUSED);
	g_print ("state change to PAUSED => %d\n", result);

	g_timeout_add (50, timeout_cb, bin);

	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	gst_element_set_state (bin, GST_STATE_NULL);
	//g_object_unref (pad);
	gst_object_unref (GST_OBJECT (bin));
	bin = NULL;
	src = NULL;
	sink = NULL;

	g_print ("\n");

	return 0;
}


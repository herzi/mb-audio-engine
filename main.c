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

typedef struct {
        GMainLoop* loop;
        GstElement* pipeline;
        GstElement* player1;
        GstPad* player1_pad;
        GstElement* player2;  
        GstPad* player2_pad;
        GstPad* player2_peer;
        guint probe_id;
} PlayerContext;

static PlayerContext ctx;

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
	g_main_loop_quit (ctx.loop);
}

static void
pad_blocked_cb (GstPad  * pad,
                gboolean  blocked,
                gpointer  user_data)
{
        /* noop */
}

static gboolean
bus_watch (GstBus    * bus,
	   GstMessage* message,
	   gpointer    user_data)
{
        PlayerContext *ctx = (PlayerContext *)user_data;

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
	        // seek on last bin element
                GstElement* elem = gst_bin_get_by_name (GST_BIN (ctx->player1), "audioconvert1");
	        g_assert(GST_IS_ELEMENT(elem));
        	if (!gst_element_seek (elem, 1.0, GST_FORMAT_TIME,
        	        GST_SEEK_FLAG_SEGMENT, 
        	        GST_SEEK_TYPE_SET, G_GINT64_CONSTANT (0),
        	        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
			g_printerr ("\nseeking background music failed\n");
		} else {
			g_printerr ("\nseeked background music back\n");
		}
                gst_object_unref (elem);
	        break;
	case GST_MESSAGE_STATE_CHANGED:
	        if (GST_IS_BIN(GST_MESSAGE_SRC (message))) {
	                GstState oldstate,newstate,pending;

                        gst_message_parse_state_changed(message,&oldstate,&newstate,&pending);

        		g_print ("%s: %s : %s -> %s\n",
        			 GST_MESSAGE_TYPE_NAME (message),
        			 G_OBJECT_TYPE_NAME (GST_MESSAGE_SRC (message)),
        			 gst_element_state_get_name(oldstate),gst_element_state_get_name(newstate));

        	        if (GST_MESSAGE_SRC (message) == GST_OBJECT (ctx->pipeline)) {
                                switch(GST_STATE_TRANSITION(oldstate,newstate)) {
                                        case GST_STATE_CHANGE_READY_TO_PAUSED: {
        	                               	GstStateChangeReturn result = gst_element_set_state (ctx->pipeline, GST_STATE_PLAYING);
                                        	g_print ("state change to PLAYING => %d\n", result);
                                        	} break;
                        	}
        	        }
        	        else if (GST_MESSAGE_SRC (message) == GST_OBJECT (ctx->player1)) {
                                switch(GST_STATE_TRANSITION(oldstate,newstate)) {
                                        case GST_STATE_CHANGE_READY_TO_PAUSED: {
                                	        // seek on last bin element
                                                GstElement* elem = gst_bin_get_by_name (GST_BIN (ctx->player1), "audioconvert1");
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
                                	} break;
                        	}
                	}
        	        else if (GST_MESSAGE_SRC (message) == GST_OBJECT (ctx->player2)) {
                                switch(GST_STATE_TRANSITION(oldstate,newstate)) {
                                        case GST_STATE_CHANGE_READY_TO_PAUSED: {
                                                /* wavparse seems to have some reuse issue
                                                gst_wavparse_pad_query (pad=0x81a0b58, query=0x80f46c0) at gstwavparse.c:1977
                                                1977      if (wav->state != GST_WAVPARSE_DATA) {

                                                if (!gst_element_seek (ctx->player2, 1.0, GST_FORMAT_TIME,
                                                        GST_SEEK_FLAG_NONE, 
                                                        GST_SEEK_TYPE_SET, G_GINT64_CONSTANT (0),
                                                        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
                                                        g_printerr ("\nseeking fx failed\n");
                                                } else {
                                                        g_printerr ("\nseeked fx back\n");
                                                }
                                                //gst_object_unref(conv);
                                                /**/
                                                gst_pad_set_blocked_async (ctx->player2_pad,
                                                                           TRUE,
                                                                           pad_blocked_cb,
                                                                           NULL);
                                                gst_element_set_locked_state (ctx->player2, TRUE);
                                                } break;
                                }
        	        }
        	}
	        break;
	case GST_MESSAGE_ELEMENT: {
	        const GstStructure *s = gst_message_get_structure (message);
	        const gchar *name = gst_structure_get_name (s);
	        if (strcmp (name, "fxplayer::eos") == 0) {
	                GstElement* add = gst_bin_get_by_name (GST_BIN (ctx->pipeline), "mixer");
	                
	                g_print("\nStop player2\n");

                        gst_pad_remove_event_probe (ctx->player2_peer, ctx->probe_id);
                	gst_element_set_state (ctx->player2, GST_STATE_READY);
                	// don't block, there is no dataflow after EOS
                        //gst_pad_set_blocked (ctx->player2_pad, TRUE);
                	gst_pad_unlink (ctx->player2_pad, ctx->player2_peer);
                	gst_element_release_request_pad (add, ctx->player2_peer);
                	ctx->player2_peer = NULL;
                        gst_object_unref (add);
                	gst_element_set_state (ctx->player2, GST_STATE_PAUSED);
	        }
	}
	case GST_MESSAGE_CLOCK_PROVIDE:
	case GST_MESSAGE_NEW_CLOCK:
	case GST_MESSAGE_ASYNC_DONE:
	case GST_MESSAGE_TAG:
		/* we don't care */
		break;
	default:
		g_print ("\nGot message: %s from %s\n",
			 GST_MESSAGE_TYPE_NAME (message),
			 G_OBJECT_TYPE_NAME (GST_MESSAGE_SRC (message)));
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

	g_print ("=> new pad: %s:%s\n"
		 "   Capabilities: %s\n",
		 gst_element_get_name (element), gst_pad_get_name (pad),
		 gst_caps_to_string (gst_pad_get_caps (pad))); // FIXME: fix memory

        result = gst_element_link (element, sink);
	//result = gst_pad_link (pad, gst_element_get_pad (sink, "sink"));
	if (GST_PAD_LINK_FAILED(result))
	  g_print ("link-result: %d\n", result);
}

static gboolean
timeout_cb (gpointer user_data)
{
        PlayerContext *ctx = (PlayerContext *)user_data;
	GstFormat format = GST_FORMAT_TIME;
	gint64 position = 0;
	gint64 duration = 0;

	if (!gst_element_query_position (ctx->pipeline, &format, &position)) {
		return TRUE;
	}

	if (format != GST_FORMAT_TIME) {
		return TRUE;
	}

	if (!gst_element_query_duration (ctx->pipeline, &format, &duration)) {
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

gboolean event_probe (GstPad *pad, GstEvent *event, gpointer user_data)
{
        PlayerContext *ctx = (PlayerContext *)user_data;

        if (GST_EVENT_TYPE(event) == GST_EVENT_EOS) {
                g_print("\nEOS for player2\n");
                /* post a message to the bus, don't do scary stuff inside the
                 * streaming thread */
                gst_element_post_message (ctx->player2,
                        gst_message_new_element (GST_OBJECT (ctx->player2),
                                gst_structure_new ("fxplayer::eos", NULL)));
        }
        return TRUE;
}

static gboolean
fxtrigger_cb (gpointer user_data)
{
        PlayerContext *ctx = (PlayerContext *)user_data;
        GstElement* add = gst_bin_get_by_name (GST_BIN (ctx->pipeline), "mixer");
        
        g_print("\nStart player2\n");

        ctx->player2_peer = gst_element_get_request_pad (add,"sink%d");
        ctx->probe_id = gst_pad_add_event_probe (ctx->player2_peer, 
                         G_CALLBACK (event_probe), ctx);
	gst_pad_link(ctx->player2_pad, ctx->player2_peer);
        gst_element_set_locked_state (ctx->player2, FALSE);
        gst_element_set_state (ctx->player2, GST_STATE_PLAYING);
        gst_pad_set_blocked_async (ctx->player2_pad,
                                   FALSE,
                                   pad_blocked_cb,
                                   NULL);
        gst_object_unref (add);
        return FALSE;
        // right now it works only once
        return TRUE;
}

int
main (int argc, char** argv)
{
	struct sigaction intaction;
	GstElement* add;
	GstElement* filt;
	GstElement* sink;
	GstBus* bus;
	GstCaps* caps;

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

        /* background music loop */
	ctx.player1 = gst_bin_new ("background");
	{
        	GstElement* src;
        	GstElement* dec;
        	GstElement* conv;
	        GstPad* pad;
	        GstPad* target_pad;

        	src  = gst_element_factory_make ("filesrc",       "filesrc1");
        	dec  = gst_element_factory_make ("decodebin2",    "decodebin1");
        	conv = gst_element_factory_make ("audioconvert",  "audioconvert1");

        	g_object_set (src, "location", "game.ogg", NULL);

        	gst_bin_add_many (GST_BIN (ctx.player1), src, dec, conv, NULL);
        	gst_element_link (src , dec);
        	target_pad = gst_element_get_static_pad (conv, "src");
        	ctx.player1_pad = gst_ghost_pad_new ("src", target_pad);
        	gst_element_add_pad (ctx.player1, ctx.player1_pad);
        	gst_object_unref (target_pad);

        	g_signal_connect (dec, "new-decoded-pad",
        			  G_CALLBACK (new_decoded_pad), conv);
        }

        /* fx sound */
	ctx.player2 = gst_bin_new ("fx1");
	{
        	GstElement* src;
        	GstElement* dec;
        	GstElement* conv;
	        GstPad* pad;
	        GstPad* target_pad;

        	src  = gst_element_factory_make ("filesrc",       "filesrc2");
        	dec  = gst_element_factory_make ("decodebin2",    "decodebin2");
        	conv = gst_element_factory_make ("audioconvert",  "audioconvert2");

        	g_object_set (src, "location", "email.wav", NULL);

        	gst_bin_add_many (GST_BIN (ctx.player2), src, dec, conv, NULL);
        	gst_element_link (src , dec);
        	target_pad = gst_element_get_static_pad (conv, "src");
        	ctx.player2_pad = gst_ghost_pad_new ("src", target_pad);
        	gst_element_add_pad (ctx.player2, ctx.player2_pad);
        	gst_object_unref (target_pad);

        	g_signal_connect (dec, "new-decoded-pad",
        			  G_CALLBACK (new_decoded_pad), conv);
        }

        /* main pipeline */
	ctx.pipeline  = gst_pipeline_new ("pipeline");
	add  = gst_element_factory_make ("adder",         "mixer");
	filt = gst_element_factory_make ("capsfilter",    "format");
	sink = gst_element_factory_make ("autoaudiosink", "audiosink");

        caps = gst_caps_from_string( "audio/x-raw-int, "
                "rate = (int) [ 1, MAX ], "
                "channels = (int) 2, "
                "endianness = (int) BYTE_ORDER, "
                "width = (int) 16, "
                "depth = (int) 16, "
                "signed = (boolean) true");
	g_object_set (filt, "caps", caps, NULL);

	gst_bin_add_many (GST_BIN (ctx.pipeline), ctx.player1, ctx.player2, add, filt, sink, NULL);
	gst_element_link(ctx.player1, add);
	//gst_element_link(ctx.player2, add);
	gst_element_link_many (add, filt, sink, NULL);

        /* message bus handler */
	bus = gst_pipeline_get_bus (GST_PIPELINE (ctx.pipeline));
	gst_bus_add_watch (bus, bus_watch, &ctx);
	gst_object_unref (bus);


	intaction.sa_sigaction = siginthandler;
	intaction.sa_flags     = SA_SIGINFO; // | SA_RESETHAND;
	if (sigaction(SIGINT, &intaction, NULL) < 0) {
		g_warning ("Couldn't set up SIGINT handler");
	}

	GstStateChangeReturn result;
	result = gst_element_set_state (ctx.pipeline, GST_STATE_PAUSED);
	g_print ("state change to PAUSED => %d\n", result);

        /* show play position */
	g_timeout_add (50, timeout_cb, &ctx);

        /* periodically trigger fx */
	g_timeout_add (2000, fxtrigger_cb, &ctx);

	ctx.loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (ctx.loop);
	g_main_loop_unref (ctx.loop);

        /* shutdown */
        gst_element_set_locked_state (ctx.player2, FALSE);
	gst_element_set_state (ctx.pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (ctx.pipeline));

	g_print ("\n");

	return 0;
}


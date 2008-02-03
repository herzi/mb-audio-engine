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

int
main (int argc, char** argv)
{
	GstElement* bin;
	GstElement* src;
	GstElement* sink;

	gst_init (&argc, &argv);
	bin  = gst_pipeline_new ("pipeline0");
	src  = gst_element_factory_make ("filesrc",  "filesrc0");
	sink = gst_element_factory_make ("fakesink", "fakesink0");

	gst_bin_add_many (GST_BIN (bin), src, sink, NULL);
	gst_element_link_many (src, sink, NULL);

	gst_object_unref (GST_OBJECT (bin));
	bin = NULL;
	src = NULL;
	sink = NULL;

	return 0;
}


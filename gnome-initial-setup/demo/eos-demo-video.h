/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2017 Endless Mobile, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *   Sam Spilsbury <sam@endlessm.com>
 */

#ifndef _EOS_DEMO_VIDEO_H
#define _EOS_DEMO_VIDEO_H

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

G_BEGIN_DECLS

#define GIS_TYPE_DEMO_VIDEO               (gis_demo_video_get_type ())
#define GIS_DEMO_VIDEO(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_DEMO_VIDEO, GisDemoVideo))
#define GIS_DEMO_VIDEO_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_DEMO_VIDEO, GisDemoVideoClass))
#define GIS_IS_DEMO_VIDEO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_DEMO_VIDEO))
#define GIS_IS_DEMO_VIDEO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_DEMO_VIDEO))
#define GIS_DEMO_VIDEO_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_DEMO_VIDEO, GisDemoVideoClass))

typedef struct _GisDemoVideo        GisDemoVideo;
typedef struct _GisDemoVideoClass   GisDemoVideoClass;

struct _GisDemoVideo
{
  GtkApplication parent;
};

struct _GisDemoVideoClass
{
  GtkApplicationClass parent_class;
};

GType gis_demo_video_get_type (void);

GisDemoVideo * gis_demo_video_new (void);

G_END_DECLS

#endif

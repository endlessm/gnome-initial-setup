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

#include "config.h"

#include "eos-demo-video.h"

typedef struct _GisDemoVideoPrivate
{
  GtkWindow *main_window;
  GstElement *element;
  GstElement *player;
  GtkWidget *widget;
  GstBus *bus;
} GisDemoVideoPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisDemoVideo, gis_demo_video, GTK_TYPE_APPLICATION)

static gboolean
on_demo_video_event (GtkWidget *widget,
                     GdkEvent  *event,
                     gpointer  user_data)
{
  if (event->type == GDK_BUTTON_PRESS ||
      event->type == GDK_2BUTTON_PRESS ||
      event->type == GDK_3BUTTON_PRESS ||
      event->type == GDK_KEY_PRESS ||
      event->type == GDK_MOTION_NOTIFY)
    {
      gtk_widget_destroy (widget);
    }

  return FALSE;
}

static gboolean
on_demo_video_realize (GtkWidget *widget,
                       gpointer  user_data)
{
  GisDemoVideo *demo_video = GIS_DEMO_VIDEO (user_data);
  GisDemoVideoPrivate *priv = gis_demo_video_get_instance_private (demo_video);

  gst_element_set_state (priv->player, GST_STATE_PLAYING);

  return FALSE;
}

static gboolean
on_video_bus_event (GstBus     *bus,
                    GstMessage *msg,
                    gpointer   user_data)
{
  GisDemoVideo *demo_video = GIS_DEMO_VIDEO (user_data);
  GisDemoVideoPrivate *priv = gis_demo_video_get_instance_private (demo_video);

  switch (GST_MESSAGE_TYPE (msg))
    {
    case GST_MESSAGE_EOS:
      gst_element_set_state (priv->player, GST_STATE_PAUSED);
      gst_element_seek_simple (priv->player,
                               GST_FORMAT_TIME,
                               GST_SEEK_FLAG_FLUSH |
                               GST_SEEK_FLAG_ACCURATE,
                               0);
      gst_element_set_state (priv->player, GST_STATE_PLAYING);
    default:
      break;
    }

  return G_SOURCE_CONTINUE;
}

#define DEMO_VIDEO_FILE_URI "file://" DEMOVIDEODATADIR "/eos-demo-video.webm"

static void
create_demo_window (GisDemoVideo *demo_video)
{
  GisDemoVideoPrivate *priv = gis_demo_video_get_instance_private (demo_video);
  GtkApplication *application = GTK_APPLICATION (demo_video);

  priv->main_window = GTK_WINDOW (gtk_application_window_new (application));
  gtk_widget_set_events (GTK_WIDGET (priv->main_window),
                         GDK_POINTER_MOTION_MASK |
                         GDK_KEY_PRESS_MASK |
                         GDK_BUTTON_PRESS_MASK); 
  gtk_window_fullscreen (priv->main_window);

  gtk_application_inhibit (application,
                           priv->main_window,
                           GTK_APPLICATION_INHIBIT_LOGOUT |
                           GTK_APPLICATION_INHIBIT_SWITCH |
                           GTK_APPLICATION_INHIBIT_IDLE,
                           "Demonstration video");

  gtk_container_add (GTK_CONTAINER (priv->main_window), priv->widget);

  g_signal_connect_object (priv->main_window,
                           "event",
                           G_CALLBACK (on_demo_video_event),
                           demo_video,
                           G_CONNECT_AFTER);

  g_signal_connect_object (GTK_WIDGET (priv->main_window),
                           "realize",
                           G_CALLBACK (on_demo_video_realize),
                           demo_video,
                           G_CONNECT_AFTER);

  gtk_widget_show_all (GTK_WIDGET (priv->main_window));

  gst_bus_add_watch (priv->bus, on_video_bus_event, demo_video);
}

static void
gis_demo_video_activate (GApplication *application)
{
  GisDemoVideo *demo_video = GIS_DEMO_VIDEO (application);
  GisDemoVideoPrivate *priv = gis_demo_video_get_instance_private (demo_video);

  G_APPLICATION_CLASS (gis_demo_video_parent_class)->activate (application);

  if (priv->main_window)
    return;

  create_demo_window (demo_video);
}

static void
gis_demo_video_init (GisDemoVideo *demo_video)
{
  GisDemoVideoPrivate *priv = gis_demo_video_get_instance_private (demo_video);
  GstBus *bus = NULL;

  priv->element = gst_element_factory_make ("gtksink", NULL);
  g_object_get (&priv->element->object, "widget", &priv->widget, NULL);

  priv->player = gst_element_factory_make ("playbin", "GisDemoVideoPlayBin");
  priv->bus = gst_pipeline_get_bus (GST_PIPELINE (priv->player));

  g_object_set (priv->player, "video-sink", g_object_ref_sink (priv->element), NULL);
  g_object_set (priv->player, "uri", DEMO_VIDEO_FILE_URI, NULL);
}

static void
gis_demo_video_dispose (GObject *object)
{
  GisDemoVideo *demo_video = GIS_DEMO_VIDEO (object);
  GisDemoVideoPrivate *priv = gis_demo_video_get_instance_private (demo_video);

  if (priv->player)
    gst_element_set_state (priv->player, GST_STATE_NULL);

  if (priv->bus)
    {
      gst_bus_set_flushing (priv->bus, TRUE);
      gst_bus_remove_watch (priv->bus);
      gst_object_replace ((GstObject **) &priv->bus, NULL);
    }

  gst_object_replace ((GstObject **) &priv->element, NULL);
  g_clear_object (&priv->widget);
  gst_object_replace ((GstObject **) &priv->player, NULL);
}

static void
gis_demo_video_class_init (GisDemoVideoClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gis_demo_video_dispose;

  application_class->activate = gis_demo_video_activate;
}

GisDemoVideo *
gis_demo_video_new (void)
{
  return g_object_new (GIS_TYPE_DEMO_VIDEO,
                       "application-id", "com.endlessm.DemoMode.SplashVideo",
                       NULL);
}

int
main (int argc, char **argv)
{
  GisDemoVideo *demo_video_application;
  int status;

  gtk_init (&argc, &argv);
  gst_init (&argc, &argv);

  demo_video_application = gis_demo_video_new ();
  status = g_application_run (G_APPLICATION (demo_video_application), argc, argv);
  g_object_unref (demo_video_application);
 
  return status;
}

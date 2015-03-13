/*
 * Copyright (C) 2015 Endless Mobile
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "gis-music-player.h"

#include <gst/gst.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

struct _GisMusicPlayerPrivate
{
  GstElement *playbin;
  GstBus *bus;
  char *uri;

  gboolean playing;
  int64_t loop_point;

  /* fade */
  GstClockTime fade_start_time;
  int fade_timeout_id;
  double fade_start_volume;
};
typedef struct _GisMusicPlayerPrivate GisMusicPlayerPrivate;

enum {
  PROP_0,
  PROP_URI,
  PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE_WITH_PRIVATE (GisMusicPlayer, gis_music_player, G_TYPE_OBJECT);

static void
gis_music_player_get_property (GObject       *object,
                               guint         prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  GisMusicPlayer *player = GIS_MUSIC_PLAYER (object);
  GisMusicPlayerPrivate *priv = gis_music_player_get_instance_private (player);

  switch (prop_id)
    {
    case PROP_URI:
      g_value_set_string (value, priv->uri);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gis_music_player_set_property (GObject       *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GisMusicPlayer *player = GIS_MUSIC_PLAYER (object);
  GisMusicPlayerPrivate *priv = gis_music_player_get_instance_private (player);

  switch (prop_id)
    {
    case PROP_URI:
      priv->uri = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gis_music_player_dispose (GObject *object)
{
  GisMusicPlayer *player = GIS_MUSIC_PLAYER (object);
  GisMusicPlayerPrivate *priv = gis_music_player_get_instance_private (player);

  g_clear_object (&priv->bus);
  g_clear_object (&priv->playbin);

  if (priv->fade_timeout_id != 0)
    {
      g_source_remove (priv->fade_timeout_id);
      priv->fade_timeout_id = 0;
    }

  G_OBJECT_CLASS (gis_music_player_parent_class)->dispose (object);
}

static void
gis_music_player_finalize (GObject *object)
{
  GisMusicPlayer *player = GIS_MUSIC_PLAYER (object);
  GisMusicPlayerPrivate *priv = gis_music_player_get_instance_private (player);

  g_free (priv->uri);

  G_OBJECT_CLASS (gis_music_player_parent_class)->finalize (object);
}

static void
on_state_changed (GstBus     *bus,
                  GstMessage *message,
                  gpointer    user_data)
{
  GisMusicPlayer *player = user_data;
  GisMusicPlayerPrivate *priv = gis_music_player_get_instance_private (player);

  GstState old, new;
  gst_message_parse_state_changed (message, &old, &new, NULL);
  GstStateChange trans = GST_STATE_TRANSITION (old, new);

  if (priv->playing &&
      message->src == GST_OBJECT (priv->playbin) &&
      trans == GST_STATE_CHANGE_READY_TO_PAUSED)
    {
      gst_element_seek_simple (priv->playbin, GST_FORMAT_TIME,
                               (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SEGMENT),
                               0);
      gst_element_set_state (priv->playbin, GST_STATE_PLAYING);
    }
}

static void
process_tag (const GstTagList *tag_list,
             const char       *tag,
             gpointer          user_data)
{
  GisMusicPlayer *player = user_data;
  GisMusicPlayerPrivate *priv = gis_music_player_get_instance_private (player);

  /* loop points are non-standard, so they go in the extended
   * comments, which is gstreamer's "catch-all" */
  if (strcmp (tag, GST_TAG_EXTENDED_COMMENT) == 0)
    {
      int n = gst_tag_list_get_tag_size (tag_list, tag);
      while (n--)
        {
          char *str;
          int64_t loop_point;

          if (!gst_tag_list_get_string_index (tag_list, tag, n, &str))
            continue;
          if (sscanf (str, "LOOP_POINT=%" G_GINT64_FORMAT, &loop_point) != 0)
            priv->loop_point = loop_point;

          g_free (str);
        }
    }
}

static void
on_tag (GstBus     *bus,
        GstMessage *message,
        gpointer    user_data)
{
  GstTagList *tag_list;
  gst_message_parse_tag (message, &tag_list);
  gst_tag_list_foreach (tag_list, process_tag, user_data);
  gst_tag_list_unref (tag_list);
}

static void
on_segment_done (GstBus     *bus,
                 GstMessage *message,
                 gpointer    user_data)
{
  GisMusicPlayer *player = user_data;
  GisMusicPlayerPrivate *priv = gis_music_player_get_instance_private (player);

  if (priv->playing &&
      message->src == GST_OBJECT (priv->playbin))
    {
      gst_element_seek_simple (priv->playbin, GST_FORMAT_TIME,
                               GST_SEEK_FLAG_SEGMENT,
                               priv->loop_point);
    }
}

static void
gis_music_player_constructed (GObject *object)
{
  GisMusicPlayer *player = GIS_MUSIC_PLAYER (object);
  GisMusicPlayerPrivate *priv = gis_music_player_get_instance_private (player);

  priv->playbin = gst_element_factory_make ("playbin", "player");
  g_object_set (priv->playbin, "uri", priv->uri, NULL);

  priv->bus = gst_element_get_bus (priv->playbin);
  gst_bus_add_signal_watch (priv->bus);
  g_signal_connect (priv->bus, "message::state-changed",
                    G_CALLBACK (on_state_changed), player);
  g_signal_connect (priv->bus, "message::segment-done",
                    G_CALLBACK (on_segment_done), player);
  g_signal_connect (priv->bus, "message::tag",
                    G_CALLBACK (on_tag), player);

  G_OBJECT_CLASS (gis_music_player_parent_class)->constructed (object);
}

static void
gis_music_player_class_init (GisMusicPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gis_music_player_get_property;
  object_class->set_property = gis_music_player_set_property;
  object_class->constructed = gis_music_player_constructed;
  object_class->dispose = gis_music_player_dispose;
  object_class->finalize = gis_music_player_finalize;

  obj_props[PROP_URI] =
    g_param_spec_string ("uri", "", "",
                         "",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}

static void
gis_music_player_init (GisMusicPlayer *player)
{
}

GisMusicPlayer *
gis_music_player_new (const char *uri)
{
  return g_object_new (GIS_TYPE_MUSIC_PLAYER,
                       "uri", uri,
                       NULL);
}

void
gis_music_player_play (GisMusicPlayer *player)
{
  GisMusicPlayerPrivate *priv = gis_music_player_get_instance_private (player);
  priv->playing = TRUE;
  gst_element_set_state (priv->playbin, GST_STATE_PAUSED);
}

#define FADE_N_STEPS 60
#define FADE_TOTAL_TIME 1

static gboolean
fadeout (gpointer user_data)
{
  GisMusicPlayer *player = user_data;
  GisMusicPlayerPrivate *priv = gis_music_player_get_instance_private (player);
  GstClock *clock = GST_ELEMENT_CLOCK (priv->playbin);
  GstClockTime time = gst_clock_get_time (clock);
  GstClockTime dt = (time - priv->fade_start_time);
  double t = 1.0 - (dt / ((double) GST_SECOND * FADE_TOTAL_TIME));

  if (t > 0)
    {
      double volume = t * priv->fade_start_volume;
      g_object_set (priv->playbin, "volume", volume, NULL);
      return TRUE;
    }
  else
    {
      g_object_set (priv->playbin, "volume", 0.0, NULL);
      priv->fade_timeout_id = 0;
      gst_element_set_state (priv->playbin, GST_STATE_PAUSED);
      return FALSE;
    }
}

void
gis_music_player_rampout (GisMusicPlayer *player)
{
  GisMusicPlayerPrivate *priv = gis_music_player_get_instance_private (player);
  GstClock *clock = GST_ELEMENT_CLOCK (priv->playbin);

  if (priv->fade_timeout_id > 0)
    return;

  /* fade out at 60fps */
  priv->fade_start_time = gst_clock_get_time (clock);
  g_object_get (priv->playbin, "volume", &priv->fade_start_volume, NULL);
  priv->fade_timeout_id = g_timeout_add (FADE_TOTAL_TIME * 1000 / FADE_N_STEPS, fadeout, player);
}

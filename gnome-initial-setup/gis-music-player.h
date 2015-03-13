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

#ifndef GIS_MUSIC_PLAYER_H
#define GIS_MUSIC_PLAYER_H

#include <glib-object.h>

#define GIS_TYPE_MUSIC_PLAYER            (gis_music_player_get_type ())
#define GIS_MUSIC_PLAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_MUSIC_PLAYER, GisMusicPlayer))
#define GIS_MUSIC_PLAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_MUSIC_PLAYER, GisMusicPlayerClass))
#define GIS_IS_MUSIC_PLAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_MUSIC_PLAYER))
#define GIS_IS_MUSIC_PLAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_MUSIC_PLAYER))
#define GIS_MUSIC_PLAYER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_MUSIC_PLAYER, GisMusicPlayerClass))

typedef struct _GisMusicPlayer      GisMusicPlayer;
typedef struct _GisMusicPlayerClass GisMusicPlayerClass;

struct _GisMusicPlayer
{
  GObject parent;
};

struct _GisMusicPlayerClass
{
  GObjectClass parent_class;
};

GType gis_music_player_get_type (void) G_GNUC_CONST;

GisMusicPlayer * gis_music_player_new (const char *uri);
void gis_music_player_play (GisMusicPlayer *player);
void gis_music_player_rampout (GisMusicPlayer *player);

#endif /* GIS_MUSIC_PLAYER_H */

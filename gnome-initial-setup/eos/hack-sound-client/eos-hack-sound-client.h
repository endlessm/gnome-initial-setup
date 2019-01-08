/*
 * Copyright (C) 2019 Endless Mobile. Inc.
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
 *     Cesar Fabian Orccon Chipana <cfoch.fabian@gmail.com>
 */

#ifndef __EOS_HACK_SOUND_CLIENT__
#define __EOS_HACK_SOUND_CLIENT__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define EOS_TYPE_HACK_SOUND_CLIENT              (eos_hack_sound_client_get_type ())
G_DECLARE_FINAL_TYPE (EOSHackSoundClient, eos_hack_sound_client, EOS, HACK_SOUND_CLIENT, GObject)

EOSHackSoundClient *eos_hack_sound_client_new (GError ** error);
void eos_hack_sound_client_play (EOSHackSoundClient  *client,
                                 const gchar         *sound_event_id,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data);
gchar *eos_hack_sound_client_play_finish (EOSHackSoundClient *client,
                                          GAsyncResult       *result,
                                          GError            **error);
void eos_hack_sound_client_stop (EOSHackSoundClient *client,
                                 const gchar *uuid,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data);

gboolean eos_hack_sound_client_stop_finish (EOSHackSoundClient *client,
                                            GAsyncResult       *result,
                                            GError            **error);

G_END_DECLS

#endif /* __EOS_HACK_SOUND_CLIENT__ */

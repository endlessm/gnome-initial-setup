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

#include "eos-hack-sound-client.h"

static void eos_hack_sound_client_initable_iface_init (GInitableIface *iface);
static void eos_hack_sound_client_finalize            (GObject *object);
static gboolean eos_hack_sound_client_initable_init (GInitable    *initable,
                                                     GCancellable *cancellable,
                                                     GError      **error);
static void play_sound_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer     data);
static void stop_sound_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer     data);

struct _EOSHackSoundClient
{
  GObject parent_instance;
  GDBusProxy *proxy;
};

#define eos_hack_sound_client_parent_class parent_class

G_DEFINE_TYPE_WITH_CODE(EOSHackSoundClient,
                        eos_hack_sound_client,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                               eos_hack_sound_client_initable_iface_init);)

static void
eos_hack_sound_client_class_init (EOSHackSoundClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = eos_hack_sound_client_finalize;
}

static void
eos_hack_sound_client_init (EOSHackSoundClient *client)
{
  /* empty */
}

static void
eos_hack_sound_client_finalize (GObject *object)
{
  EOSHackSoundClient *client = EOS_HACK_SOUND_CLIENT (object);

  g_object_unref (client->proxy);
  G_OBJECT_CLASS (eos_hack_sound_client_parent_class)->finalize (object);
}

void
eos_hack_sound_client_stop (EOSHackSoundClient  *client,
                            const gchar         *uuid,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  GTask *task;
  g_return_if_fail (EOS_IS_HACK_SOUND_CLIENT (client));

  task = g_task_new (client, NULL, callback, user_data);
  g_dbus_proxy_call (client->proxy,
                     "StopSound", g_variant_new ("(s)", uuid),
                     G_DBUS_CALL_FLAGS_NONE, -1, NULL, stop_sound_cb, task);
}

static void
stop_sound_cb (GObject      *source_object,
               GAsyncResult *res,
               gpointer     data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (source_object);
  g_autoptr(GTask) task = G_TASK (data);
  GError *error = NULL;

  g_dbus_proxy_call_finish (proxy, res, &error);
  if (error)
    {
      g_task_return_error (task, error);
      return;
    }
  g_task_return_boolean (task, TRUE);
}

gboolean
eos_hack_sound_client_stop_finish (EOSHackSoundClient  *client,
                                   GAsyncResult        *result,
                                   GError             **error)
{
  g_return_val_if_fail (g_task_is_valid (result, client), NULL);

  return g_task_propagate_boolean (G_TASK (result), error);
}

void
eos_hack_sound_client_play (EOSHackSoundClient  *client,
                            const gchar         *sound_event_id,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  GTask *task;
  g_return_if_fail (EOS_IS_HACK_SOUND_CLIENT (client));

  task = g_task_new (client, NULL, callback, user_data);
  g_dbus_proxy_call (client->proxy,
                     "PlaySound", g_variant_new ("(s)", sound_event_id),
                     G_DBUS_CALL_FLAGS_NONE, -1, NULL, play_sound_cb, task);
}

gchar *
eos_hack_sound_client_play_finish (EOSHackSoundClient * client,
    GAsyncResult * result, GError ** error)
{
  g_return_val_if_fail (g_task_is_valid (result, client), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
play_sound_cb (GObject      *source_object,
               GAsyncResult *res,
               gpointer     data)
{
  GVariant *variant;
  GDBusProxy *proxy = G_DBUS_PROXY (source_object);
  g_autoptr(GTask) task = G_TASK (data);
  GError *error = NULL;

  variant = g_dbus_proxy_call_finish (proxy, res, &error);
  if (error)
    g_task_return_error (task, error);
  else
    {
      gchar *uuid;
      g_variant_get (variant, "(s)", &uuid);
      g_task_return_pointer (task, uuid, g_free);
    }
}

static void
eos_hack_sound_client_initable_iface_init (GInitableIface *iface)
{
  iface->init = eos_hack_sound_client_initable_init;
}

static gboolean
eos_hack_sound_client_initable_init (GInitable     *initable,
                                     GCancellable  *cancellable,
                                     GError       **error)
{
  EOSHackSoundClient *client;
  g_return_val_if_fail (EOS_IS_HACK_SOUND_CLIENT (initable), FALSE);

  client = EOS_HACK_SOUND_CLIENT (initable);

  if (client->proxy == NULL)
    client->proxy =
        g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                           G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                       NULL,
                                       "com.endlessm.HackSoundServer",
                                       "/com/endlessm/HackSoundServer",
                                       "com.endlessm.HackSoundServer",
                                       NULL, error);
  return client->proxy != NULL;
}

EOSHackSoundClient *
eos_hack_sound_client_new (GError **error)
{
  return EOS_HACK_SOUND_CLIENT (g_initable_new (EOS_TYPE_HACK_SOUND_CLIENT,
                                NULL, error, NULL));
}

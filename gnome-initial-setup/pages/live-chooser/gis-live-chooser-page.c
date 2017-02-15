/* gis-live-chooser-page.c
 *
 * Copyright (C) 2016 Endless Mobile, Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 */

#include "gis-live-chooser-page.h"
#include "live-chooser-resources.h"

#include <act/act-user-manager.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <polkit/polkit.h>

#include <locale.h>

#define LIVE_ACCOUNT_USERNAME "live"
#define LIVE_ACCOUNT_FULLNAME "Endless OS"

struct _GisLiveChooserPage
{
  GisPage           parent_instance;

  GCancellable     *cancellable;

  ActUserManager   *act_client;
};

G_DEFINE_TYPE (GisLiveChooserPage, gis_live_chooser_page, GIS_TYPE_PAGE)

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(self)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

static void
disable_chrome_auto_download (GisLiveChooserPage *self)
{
  GError *error = NULL;

  if (!gis_pkexec (DATADIR "/eos-google-chrome-helper/eos-google-chrome-system-helper.py",
                   NULL, &error))
    {
      g_warning ("Failed to disable Chrome auto-download: %s\n", error->message);
      g_clear_error (&error);
    }
}

static void
create_live_user (GisLiveChooserPage *self)
{
  ActUser *user;
  GError *error = NULL;
  const gchar *language;

  user = act_user_manager_create_user (self->act_client,
                                       LIVE_ACCOUNT_USERNAME,
                                       LIVE_ACCOUNT_FULLNAME,
                                       ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR,
                                       &error);
  if (error)
    {
      g_warning ("Failed to create live user: %s", error->message);
      g_clear_error (&error);
      return;
    }

  act_user_set_password_mode (user, ACT_USER_PASSWORD_MODE_NONE);
  act_user_set_automatic_login (user, FALSE);

  language = gis_driver_get_user_language (GIS_PAGE (self)->driver);

  if (language)
    act_user_set_language (user, language);

  if (!gis_pkexec (LIBEXECDIR "/eos-setup-live-user", LIVE_ACCOUNT_USERNAME,
                   &error))
    {
      g_warning ("Failed to setup live user: %s\n", error->message);
      g_clear_error (&error);
    }

  gis_driver_set_user_permissions (GIS_PAGE (self)->driver, user, NULL);

  g_object_unref (user);
}

static void
user_loaded (GObject    *object,
             GParamSpec *pspec,
             gpointer    user_data)
{
  gchar *language = user_data;

  act_user_set_language (ACT_USER (object), language);

  g_free (language);
}

static void
load_css_overrides (GisLiveChooserPage *self)
{
  GtkCssProvider *provider;
  GError *error;
  GFile *file;

  error = NULL;
  provider = gtk_css_provider_new ();
  file = g_file_new_for_uri ("resource:///org/gnome/initial-setup/live-chooser.css");
  gtk_css_provider_load_from_file (provider, file, &error);
  g_object_unref (file);

  if (error != NULL)
    {
      g_warning ("Unable to load CSS overrides for the live-chooser page: %s", error->message);
      g_clear_error (&error);
    }
  else
    {
      gtk_style_context_add_provider_for_screen (gtk_widget_get_screen (GTK_WIDGET (self)),
                                                 GTK_STYLE_PROVIDER (provider),
                                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  g_object_unref (provider);
}

static void
update_assistant (GisLiveChooserPage *self)
{
  GisAssistant *assistant = gis_driver_get_assistant (GIS_PAGE (self)->driver);

  gis_assistant_next_page (assistant);
}

static void
try_button_clicked (GisLiveChooserPage *self)
{
  create_live_user (self);
  disable_chrome_auto_download (self);
  update_assistant (self);
}

static void
reformatter_exited_cb (GPid     pid,
                       gint     status,
                       gpointer user_data)
{
  GisLiveChooserPage *self = user_data;
  GError *error = NULL;

  g_spawn_check_exit_status (status, &error);

  if (error)
    {
      GtkWidget *message_dialog;

      g_critical ("Error running the reformatter: %s", error->message);

      message_dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (user_data)),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               _("Error running the reformatter: %s"), error->message);

      gis_driver_show_window (GIS_PAGE (self)->driver);
      gtk_widget_show (message_dialog);

      g_clear_error (&error);
    }
  else
    {
      g_application_quit (G_APPLICATION (GIS_PAGE (self)->driver));
    }
}

static void
reformat_button_clicked (GisLiveChooserPage *self)
{
  GError *error = NULL;
  GPid pid;

  gchar* command[] = { "gnome-image-installer", NULL };

  g_spawn_async ("/usr/lib/eos-installer",
                 (gchar**) command,
                 NULL,
                 G_SPAWN_DEFAULT,
                 NULL,
                 NULL,
                 &pid,
                 &error);

  if (error)
    {
      g_critical ("Error running the reformatter: %s", error->message);
      g_clear_error (&error);
      return;
    }

  gis_driver_hide_window (GIS_PAGE (self)->driver);

  /*
   * Check for when the reformatter finishes running, and check
   * if it exited smoothly.
   */
  g_child_watch_add (pid, reformatter_exited_cb, self);
}

static void
gis_live_chooser_page_finalize (GObject *object)
{
  GisLiveChooserPage *self = (GisLiveChooserPage *)object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (gis_live_chooser_page_parent_class)->finalize (object);
}

static void
gis_live_chooser_page_constructed (GObject *object)
{
  GisLiveChooserPage *self = GIS_LIVE_CHOOSER_PAGE (object);

  G_OBJECT_CLASS (gis_live_chooser_page_parent_class)->constructed (object);

  gis_page_set_hide_forward_button (GIS_PAGE (self), TRUE);

  self->act_client = act_user_manager_get_default ();

  g_signal_connect_swapped (WID ("try_button"),
                            "clicked",
                            G_CALLBACK (try_button_clicked),
                            self);

  g_signal_connect_swapped (WID ("reformat_button"),
                            "clicked",
                            G_CALLBACK (reformat_button_clicked),
                            self);


  load_css_overrides (self);

  gtk_container_add (GTK_CONTAINER (self), WID ("main_box"));
  gtk_widget_show (GTK_WIDGET (self));
}

static void
gis_live_chooser_page_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
gis_live_chooser_page_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
gis_live_chooser_page_locale_changed (GisPage *page)
{
  GisLiveChooserPage *self = GIS_LIVE_CHOOSER_PAGE (page);

  gtk_label_set_label (OBJ (GtkLabel*, "try_label"), _("Try Endless OS by running it from the USB Stick."));
  gtk_label_set_label (OBJ (GtkLabel*, "reformat_label"), _("Reformat this computer with Endless OS."));
  gtk_button_set_label (OBJ (GtkButton*, "try_button"), _("Try It"));
  gtk_button_set_label (OBJ (GtkButton*, "reformat_button"), _("Reformat"));

  gis_page_set_title (page, _("Endless USB Stick"));
}

static void
gis_live_chooser_page_class_init (GisLiveChooserPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);

  page_class->page_id = "live-chooser";
  page_class->locale_changed = gis_live_chooser_page_locale_changed;

  object_class->finalize = gis_live_chooser_page_finalize;
  object_class->constructed = gis_live_chooser_page_constructed;
  object_class->get_property = gis_live_chooser_page_get_property;
  object_class->set_property = gis_live_chooser_page_set_property;
}

static void
gis_live_chooser_page_init (GisLiveChooserPage *self)
{
  g_resources_register (live_chooser_get_resource ());
}

void
gis_prepare_live_chooser_page (GisDriver *driver)
{
  /* Only show this page when running on a live boot session */
  if (!gis_driver_is_live_session (driver))
    return;

  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_LIVE_CHOOSER_PAGE,
                                     "driver", driver,
                                     NULL));
}

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

#define _GNU_SOURCE 1  /* for NL_LOCALE_NAME */

#include "gis-live-chooser-page.h"
#include "live-chooser-resources.h"

#include <act/act-user-manager.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <polkit/polkit.h>

#include <langinfo.h>
#include <locale.h>

#include "gis-page-util.h"
#include "pages/language/cc-language-chooser.h"

#define LIVE_ACCOUNT_AVATAR "/usr/share/pixmaps/faces/sunflower.jpg"
#define LIVE_ACCOUNT_USERNAME "live"
#define LIVE_ACCOUNT_FULLNAME "Endless OS"

struct _GisLiveChooserPagePrivate
{
  GtkWidget *try_label;
  GtkWidget *reformat_label;
  GtkWidget *try_button;
  GtkWidget *reformat_button;

  ActUserManager   *act_client;
};

typedef struct _GisLiveChooserPagePrivate GisLiveChooserPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisLiveChooserPage, gis_live_chooser_page, GIS_TYPE_PAGE);

static gboolean
gis_live_chooser_page_save_data (GisPage  *page,
                                 GError  **error)
{
  GisLiveChooserPage *self = GIS_LIVE_CHOOSER_PAGE (page);
  GisLiveChooserPagePrivate *priv = gis_live_chooser_page_get_instance_private (self);
  g_autoptr(ActUser) user = NULL;
  const gchar *language;

  if (gis_driver_has_live_persistence (page->driver))
    return TRUE;

  user = act_user_manager_create_user (priv->act_client,
                                       LIVE_ACCOUNT_USERNAME,
                                       LIVE_ACCOUNT_FULLNAME,
                                       ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR,
                                       error);
  if (user == NULL)
    {
      g_prefix_error (error, "Failed to create live user: ");
      return FALSE;
    }

  act_user_set_password_mode (user, ACT_USER_PASSWORD_MODE_NONE);
  act_user_set_automatic_login (user, FALSE);
  act_user_set_icon_file (user, LIVE_ACCOUNT_AVATAR);

  language = gis_driver_get_user_language (page->driver);

  if (language)
    act_user_set_language (user, language);

  gis_driver_set_user_permissions (page->driver, user, NULL);

  gis_update_login_keyring_password ("");

  return TRUE;
}

static void
load_css_overrides (GisLiveChooserPage *page)
{
  GtkCssProvider *provider;
  GError *error;

  error = NULL;
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/initial-setup/live-chooser.css");

  if (error != NULL)
    {
      g_warning ("Unable to load CSS overrides for the live-chooser page: %s", error->message);
      g_clear_error (&error);
    }
  else
    {
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  g_object_unref (provider);
}

static void
try_button_clicked (GisLiveChooserPage *page)
{
  GisAssistant *assistant = gis_driver_get_assistant (GIS_PAGE (page)->driver);

  gis_assistant_next_page (assistant);
}

static void
reformat_button_clicked (GisLiveChooserPage *page)
{
  gis_page_util_run_reformatter (GIS_PAGE (page), NULL, NULL);
}

static void
gis_live_chooser_page_constructed (GObject *object)
{
  GisLiveChooserPage *page = GIS_LIVE_CHOOSER_PAGE (object);
  GisLiveChooserPagePrivate *priv = gis_live_chooser_page_get_instance_private (page);

  G_OBJECT_CLASS (gis_live_chooser_page_parent_class)->constructed (object);

  gis_page_set_has_forward (GIS_PAGE (page), TRUE);

  priv->act_client = act_user_manager_get_default ();

  g_signal_connect_swapped (priv->try_button,
                            "clicked",
                            G_CALLBACK (try_button_clicked),
                            page);

  g_signal_connect_swapped (priv->reformat_button,
                            "clicked",
                            G_CALLBACK (reformat_button_clicked),
                            page);


  load_css_overrides (page);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_live_chooser_page_locale_changed (GisPage *page)
{
  GisLiveChooserPagePrivate *priv = gis_live_chooser_page_get_instance_private (GIS_LIVE_CHOOSER_PAGE (page));

  gtk_label_set_label (GTK_LABEL (priv->try_label), _("Try Endless OS by running it from the USB Stick."));
  gtk_label_set_label (GTK_LABEL (priv->reformat_label), _("Reformat this computer with Endless OS."));
  gtk_button_set_label (GTK_BUTTON (priv->try_button), _("Try It"));
  gtk_button_set_label (GTK_BUTTON (priv->reformat_button), _("Reformat"));

  gis_page_set_title (page, _("Try or Reformat"));
}

static void
gis_live_chooser_page_class_init (GisLiveChooserPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);

  page_class->page_id = "live-chooser";
  page_class->locale_changed = gis_live_chooser_page_locale_changed;
  page_class->save_data = gis_live_chooser_page_save_data;

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-live-chooser-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisLiveChooserPage, try_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisLiveChooserPage, reformat_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisLiveChooserPage, try_button);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisLiveChooserPage, reformat_button);

  object_class->constructed = gis_live_chooser_page_constructed;
}

static void
gis_live_chooser_page_init (GisLiveChooserPage *page)
{
  g_resources_register (live_chooser_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_live_chooser_page (GisDriver *driver)
{
  if (!gis_driver_is_live_session (driver))
    return NULL;

  return g_object_new (GIS_TYPE_LIVE_CHOOSER_PAGE,
                       "driver", driver,
                       NULL);
}

/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
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
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

/* Password page {{{1 */

#define PAGE_ID "password"

#include "config.h"
#include "password-resources.h"
#include "gis-password-page.h"

#include "gis-keyring.h"

#include "pw-utils.h"
#include "../account/um-utils.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "gis-page-header.h"

#define VALIDATION_TIMEOUT 600

struct _GisPasswordPagePrivate
{
  GtkWidget *password_entry;
  GtkWidget *confirm_entry;
  GtkWidget *password_strength;
  GtkWidget *password_explanation;
  GtkWidget *confirm_explanation;
  GtkWidget *confirm_checkmark;
  GtkWidget *reminder_entry;

  GDBusProxy *input_source_manager;

  gboolean valid_confirm;
  gboolean valid_password;
  guint timeout_id;
  const gchar *username;
};
typedef struct _GisPasswordPagePrivate GisPasswordPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisPasswordPage, gis_password_page, GIS_TYPE_PAGE);

static gboolean
page_validate (GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  gboolean has_reminder = (gtk_entry_get_text_length (GTK_ENTRY (priv->reminder_entry)) > 0);

  return priv->valid_confirm && has_reminder;
}

static void
update_page_validation (GisPasswordPage *page)
{
  gis_page_set_complete (GIS_PAGE (page), page_validate (page));
}

static void
gis_password_page_save_data (GisPage *gis_page)
{
  GisPasswordPage *page = GIS_PASSWORD_PAGE (gis_page);
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  ActUser *act_user;
  UmAccountMode account_mode;
  const gchar *password = NULL;
  gchar *sanitized_reminder;
  const gchar *reminder;

  if (gis_page->driver == NULL)
    return;

  account_mode = gis_driver_get_account_mode (gis_page->driver);

  gis_driver_get_user_permissions (gis_page->driver, &act_user, &password);

  if (account_mode == UM_ENTERPRISE) {
    if (password != NULL)
      gis_update_login_keyring_password (password);
    return;
  }

  password = gtk_entry_get_text (GTK_ENTRY (priv->password_entry));

  if (strlen (password) == 0) {
    act_user_set_password_mode (act_user, ACT_USER_PASSWORD_MODE_NONE);
  } else {
    reminder = gtk_entry_get_text (GTK_ENTRY (priv->reminder_entry));
    sanitized_reminder = g_strstrip (g_strdup (reminder));
    act_user_set_password (act_user, password, sanitized_reminder);
    g_free (sanitized_reminder);
  }

  gis_driver_set_user_permissions (gis_page->driver, act_user, password);

  gis_update_login_keyring_password (password);
}

static void
shell_enable_password_mode (GisPasswordPage *page, gboolean enable)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  GError *error = NULL;

  if (!priv->input_source_manager)
    return;

  g_dbus_proxy_call_sync (priv->input_source_manager,
                          "Set",
                          g_variant_new_parsed ("('org.gnome.Shell.InputSourceManager', 'PasswordModeEnabled', %v)",
                                                g_variant_new_boolean (enable)),
                          G_DBUS_CALL_FLAGS_NONE, -1,
                          NULL, &error);

  if (error != NULL) {
    g_critical ("Unable to set PasswordMode: %s", error->message);
    g_error_free (error);
  }
}

static void
gis_password_page_shown (GisPage *gis_page)
{
  GisPasswordPage *page = GIS_PASSWORD_PAGE (gis_page);
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

  shell_enable_password_mode (page, TRUE);

  gtk_widget_grab_focus (priv->password_entry);
}

static gboolean
validate (GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  const gchar *password;
  const gchar *verify;
  gint strength_level;
  const gchar *hint;

  if (priv->timeout_id != 0) {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }

  password = gtk_entry_get_text (GTK_ENTRY (priv->password_entry));
  verify = gtk_entry_get_text (GTK_ENTRY (priv->confirm_entry));

  pw_strength (password, NULL, priv->username, &hint, &strength_level);
  gtk_level_bar_set_value (GTK_LEVEL_BAR (priv->password_strength), strength_level);
  gtk_label_set_label (GTK_LABEL (priv->password_explanation), hint);

  gtk_label_set_label (GTK_LABEL (priv->confirm_explanation), "");
  priv->valid_confirm = FALSE;

  priv->valid_password = (strlen (password) && strength_level > 1);

  if (strlen (password) > 0 && strlen (verify) > 0) {
    priv->valid_confirm = password && *password != '\0' && strcmp (password, verify) == 0;
    if (!priv->valid_confirm) {
      gtk_label_set_label (GTK_LABEL (priv->confirm_explanation), _("The passwords do not match."));
      gtk_image_set_from_icon_name (GTK_IMAGE (priv->confirm_checkmark), NULL,
                                    GTK_ICON_SIZE_BUTTON);
    }
    else {
      gtk_image_set_from_icon_name (GTK_IMAGE (priv->confirm_checkmark),
                                    "object-select-symbolic",
                                    GTK_ICON_SIZE_BUTTON);
    }
  }

  update_page_validation (page);

  return FALSE;
}

static void
page_visibility_changed (GtkWidget  *w,
                         GParamSpec *pspec,
                         gpointer    unused)
{
  /* Make sure to reset the password page in this case, to prevent situations
   * where a previously introduced password, that is no longer required, would
   * end up being used anyway when creating the user's account.
   */
  if (!gtk_widget_get_visible (w)) {
    GisPasswordPage *page = GIS_PASSWORD_PAGE (w);
    GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

    gtk_entry_set_text (GTK_ENTRY (priv->password_entry), "");
    gtk_entry_set_text (GTK_ENTRY (priv->confirm_entry), "");
    gtk_entry_set_text (GTK_ENTRY (priv->reminder_entry), "");
    gtk_entry_set_visibility (GTK_ENTRY (priv->password_entry), FALSE);
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->password_entry),
                                       GTK_ENTRY_ICON_PRIMARY,
                                       "eye-open-negative-filled-symbolic");
    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (priv->password_entry),
                                     GTK_ENTRY_ICON_PRIMARY,
                                     _("Show password"));
  }
}

static gboolean
on_focusout (GisPasswordPage *page)
{
  validate (page);

  return FALSE;
}

static void
on_entry_icon_press (GtkEntry            *entry,
                     GtkEntryIconPosition icon_pos,
                     GdkEvent            *event G_GNUC_UNUSED,
                     gpointer             data G_GNUC_UNUSED)
{
  if (icon_pos != GTK_ENTRY_ICON_SECONDARY)
    return;

  if (gtk_entry_get_visibility (entry))
    {
      gtk_entry_set_visibility (entry, FALSE);
      gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY,
                                         "eye-open-negative-filled-symbolic");
      gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY,
                                       _("Show password"));
    }
  else
    {
      gtk_entry_set_visibility (entry, TRUE);
      gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY,
                                         "eye-not-looking-symbolic");
      gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY,
                                       _("Hide password"));
    }
}

static void
reminder_changed (GtkWidget       *w,
                  GParamSpec      *pspec,
                  GisPasswordPage *page)
{
  gis_page_set_complete (GIS_PAGE (page), page_validate (page));
}

static void
password_changed (GtkWidget      *w,
                  GParamSpec     *pspec,
                  GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

  priv->valid_password = FALSE;
  update_page_validation (page);

  if (priv->timeout_id != 0)
    g_source_remove (priv->timeout_id);
  priv->timeout_id = g_timeout_add (VALIDATION_TIMEOUT, (GSourceFunc)validate, page);
}

static void
confirm_changed (GtkWidget      *w,
                 GParamSpec     *pspec,
                 GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

  priv->valid_confirm = FALSE;
  update_page_validation (page);

  if (priv->timeout_id != 0)
    g_source_remove (priv->timeout_id);
  priv->timeout_id = g_timeout_add (VALIDATION_TIMEOUT, (GSourceFunc)validate, page);
}

static void
username_or_passwordless_changed (GObject         *obj,
                                  GParamSpec      *pspec,
                                  GisPasswordPage *page)
{
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  priv->username = gis_driver_get_username (GIS_DRIVER (obj));
  gboolean passwordless = gis_driver_get_passwordless (GIS_DRIVER (obj));

  if (priv->username && !passwordless)
    gtk_widget_show (GTK_WIDGET (page));
  else
    gtk_widget_hide (GTK_WIDGET (page));

  validate (page);
}

static void
confirm (GisPasswordPage *page)
{
  if (page_validate (page))
    gis_assistant_next_page (gis_driver_get_assistant (GIS_PAGE (page)->driver));
}

static void
gis_password_page_constructed (GObject *object)
{
  GisPasswordPage *page = GIS_PASSWORD_PAGE (object);
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);
  GtkSettings *settings;

  G_OBJECT_CLASS (gis_password_page_parent_class)->constructed (object);

  g_signal_connect (page, "notify::visible",
                    G_CALLBACK (page_visibility_changed), NULL);

  g_signal_connect (priv->password_entry, "notify::text",
                    G_CALLBACK (password_changed), page);
  g_signal_connect_swapped (priv->password_entry, "focus-out-event",
                            G_CALLBACK (on_focusout), page);
  g_signal_connect_swapped (priv->password_entry, "activate",
                            G_CALLBACK (confirm), page);
  g_signal_connect (priv->password_entry, "icon-press",
                    G_CALLBACK (on_entry_icon_press), NULL);

  g_signal_connect (priv->reminder_entry, "notify::text",
                      G_CALLBACK (reminder_changed), page);

  g_signal_connect (priv->confirm_entry, "notify::text",
                    G_CALLBACK (confirm_changed), page);
  g_signal_connect_swapped (priv->confirm_entry, "focus-out-event",
                            G_CALLBACK (on_focusout), page);
  g_signal_connect_swapped (priv->confirm_entry, "activate",
                            G_CALLBACK (confirm), page);
  g_signal_connect (priv->confirm_entry, "icon-press",
                    G_CALLBACK (on_entry_icon_press), NULL);

  g_signal_connect (GIS_PAGE (page)->driver, "notify::username",
                    G_CALLBACK (username_or_passwordless_changed), page);
  g_signal_connect (GIS_PAGE (page)->driver, "notify::passwordless",
                    G_CALLBACK (username_or_passwordless_changed), page);

  /* show the last character from the password; 600 is the recommended value
   * in the gtk-entry-password-hint-timeout documentation
   */
  settings = gtk_settings_get_default ();
  g_object_set (G_OBJECT (settings), "gtk-entry-password-hint-timeout", 600, NULL);

  validate (page);

  priv->input_source_manager = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                              G_DBUS_PROXY_FLAGS_NONE,
                                                              NULL,
                                                              "org.gnome.Shell.InputSourceManager",
                                                              "/org/gnome/Shell/InputSourceManager",
                                                              "org.freedesktop.DBus.Properties",
                                                              NULL, NULL);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_password_page_dispose (GObject *object)
{
  GisPasswordPage *page = GIS_PASSWORD_PAGE (object);
  GisPasswordPagePrivate *priv = gis_password_page_get_instance_private (page);

  GtkSettings *settings = gtk_settings_get_default ();
  g_object_set (G_OBJECT (settings), "gtk-entry-password-hint-timeout", 0, NULL);

  shell_enable_password_mode (page, FALSE);

  g_clear_object (&priv->input_source_manager);

  if (GIS_PAGE (object)->driver)
    g_signal_handlers_disconnect_by_func (GIS_PAGE (object)->driver,
                                          username_or_passwordless_changed, object);

  G_OBJECT_CLASS (gis_password_page_parent_class)->dispose (object);
}

static void
gis_password_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Password"));
}

static void
gis_password_page_class_init (GisPasswordPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-password-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, password_entry);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, confirm_entry);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, password_strength);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, password_explanation);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, confirm_explanation);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, confirm_checkmark);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPasswordPage, reminder_entry);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_password_page_locale_changed;
  page_class->save_data = gis_password_page_save_data;
  page_class->shown = gis_password_page_shown;

  object_class->constructed = gis_password_page_constructed;
  object_class->dispose = gis_password_page_dispose;
}

static void
gis_password_page_init (GisPasswordPage *page)
{
  GtkCssProvider *provider;

  g_resources_register (password_get_resource ());
  g_type_ensure (GIS_TYPE_PAGE_HEADER);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/initial-setup/gis-password-page.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_password_page (GisDriver *driver)
{
  if (gis_driver_is_live_session (driver) ||
  	  gis_driver_is_in_demo_mode (driver))
    return NULL;

  return g_object_new (GIS_TYPE_PASSWORD_PAGE,
                       "driver", driver,
                       NULL);
}


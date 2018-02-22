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
 *     Michael Wood <michael.g.wood@intel.com>
 *
 * Based on gnome-control-center cc-region-panel.c
 */

/* Language page {{{1 */

#define PAGE_ID "language"

#define GNOME_SYSTEM_LOCALE_DIR "org.gnome.system.locale"
#define REGION_KEY "region"

#include "config.h"
#include "language-resources.h"
#include "gis-welcome-widget.h"
#include "cc-language-chooser.h"
#include "gis-factory-dialog.h"
#include "gis-language-page.h"
#include "gis-page-util.h"

#include <errno.h>
#include <langinfo.h>
#include <locale.h>

#include <act/act-user-manager.h>
#include <gdesktop-enums.h>
#include <polkit/polkit.h>
#include <gtk/gtk.h>

#define CLOCK_SCHEMA "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"
#define DEFAULT_CLOCK_FORMAT G_DESKTOP_CLOCK_FORMAT_24H

struct _GisLanguagePagePrivate
{
  GtkWidget *logo;
  GtkWidget *welcome_widget;
  GtkWidget *language_chooser;

  GDBusProxy *localed;
  GPermission *permission;
  const gchar *new_locale_id;

  GCancellable *cancellable;

  gboolean checked_unattended_config;
};
typedef struct _GisLanguagePagePrivate GisLanguagePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisLanguagePage, gis_language_page, GIS_TYPE_PAGE);

G_DEFINE_AUTO_CLEANUP_FREE_FUNC(locale_t, freelocale, NULL)

static void
set_localed_locale (GisLanguagePage *self)
{
  GisLanguagePagePrivate *priv = gis_language_page_get_instance_private (self);
  GVariantBuilder *b;
  gchar *s;

  b = g_variant_builder_new (G_VARIANT_TYPE ("as"));
  s = g_strconcat ("LANG=", priv->new_locale_id, NULL);
  g_variant_builder_add (b, "s", s);
  g_free (s);

  g_dbus_proxy_call (priv->localed,
                     "SetLocale",
                     g_variant_new ("(asb)", b, TRUE),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, NULL, NULL);
  g_variant_builder_unref (b);
}

static void
change_locale_permission_acquired (GObject      *source,
                                   GAsyncResult *res,
                                   gpointer      data)
{
  GisLanguagePage *page = GIS_LANGUAGE_PAGE (data);
  GisLanguagePagePrivate *priv = gis_language_page_get_instance_private (page);
  GError *error = NULL;
  gboolean allowed;

  allowed = g_permission_acquire_finish (priv->permission, res, &error);
  if (error) {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to acquire permission: %s", error->message);
      g_error_free (error);
      return;
  }

  if (allowed)
    set_localed_locale (page);
}

static void
user_loaded (GObject    *object,
             GParamSpec *pspec,
             gpointer    user_data)
{
  gchar *new_locale_id = user_data;

  act_user_set_language (ACT_USER (object), new_locale_id);

  g_free (new_locale_id);
}

static GDesktopClockFormat
get_default_time_format (const gchar *lang_id)
{
  const char *ampm, *nl_fmt;
  locale_t undef_locale;

  if (lang_id == NULL)
    {
      undef_locale = uselocale ((locale_t) 0);
      if (undef_locale == (locale_t) 0)
        {
          g_warning ("Failed to detect current locale: %s", g_strerror (errno));
          return DEFAULT_CLOCK_FORMAT;
        }
    }
  else
    {
      undef_locale = newlocale (LC_MESSAGES_MASK | LC_TIME_MASK, lang_id, (locale_t) 0);
      if (undef_locale == (locale_t) 0)
        {
          g_warning ("Failed to create locale %s: %s", lang_id, g_strerror (errno));
          return DEFAULT_CLOCK_FORMAT;
        }
    }

  /* It's necessary to duplicate the locale because undef_locale might be
   * LC_GLOBAL_LOCALE, and duplocale() will make a concrete locale. Passing
   * LC_GLOBAL_LOCALE to nl_langinfo_l() is undefined behaviour. */
  g_auto(locale_t) locale = duplocale (undef_locale);
  if (locale == (locale_t) 0)
    {
      g_warning ("Failed to copy current locale: %s", g_strerror (errno));
      return DEFAULT_CLOCK_FORMAT;
    }

  /* Default if we can't get the format from the locale */
  nl_fmt = nl_langinfo_l (T_FMT, locale);
  if (nl_fmt == NULL || *nl_fmt == '\0')
    return DEFAULT_CLOCK_FORMAT;

  /* Fall back to 24 hour specifically if AM/PM is not available in the locale */
  ampm = nl_langinfo_l (AM_STR, locale);
  if (ampm == NULL || ampm[0] == '\0')
    return G_DESKTOP_CLOCK_FORMAT_24H;

  /* Parse out any formats that use 12h format. See stftime(3). */
  if (g_str_has_prefix (nl_fmt, "%I") ||
      g_str_has_prefix (nl_fmt, "%l") ||
      g_str_has_prefix (nl_fmt, "%r"))
    return G_DESKTOP_CLOCK_FORMAT_12H;
  else
    return G_DESKTOP_CLOCK_FORMAT_24H;
}

static void
set_time_format (const char *lang_id)
{
  g_autoptr(GSettings) settings = g_settings_new (CLOCK_SCHEMA);
  GDesktopClockFormat clock_format = get_default_time_format (lang_id);
  g_settings_set_enum (settings, CLOCK_FORMAT_KEY, clock_format);
}

static void
language_changed (CcLanguageChooser  *chooser,
                  GParamSpec         *pspec,
                  GisLanguagePage    *page)
{
  GisLanguagePagePrivate *priv = gis_language_page_get_instance_private (page);
  GisDriver *driver;
  GSettings *region_settings;
  ActUser *user;

  priv->new_locale_id = cc_language_chooser_get_language (chooser);
  driver = GIS_PAGE (page)->driver;

  gis_driver_set_user_language (driver, priv->new_locale_id, TRUE);
  gtk_widget_set_default_direction (gtk_get_locale_direction ());
  set_time_format (priv->new_locale_id);

  if (gis_driver_get_mode (driver) == GIS_DRIVER_MODE_NEW_USER) {
      if (g_permission_get_allowed (priv->permission)) {
          set_localed_locale (page);
      }
      else if (g_permission_get_can_acquire (priv->permission)) {
          g_permission_acquire_async (priv->permission,
                                      NULL,
                                      change_locale_permission_acquired,
                                      page);
      }
  }

  /* Ensure we won't override the selected language for format strings */
  region_settings = g_settings_new (GNOME_SYSTEM_LOCALE_DIR);
  g_settings_reset (region_settings, REGION_KEY);
  g_object_unref (region_settings);

  user = act_user_manager_get_user (act_user_manager_get_default (),
                                    g_get_user_name ());
  if (act_user_is_loaded (user))
    act_user_set_language (user, priv->new_locale_id);
  else
    g_signal_connect (user,
                      "notify::is-loaded",
                      G_CALLBACK (user_loaded),
                      g_strdup (priv->new_locale_id));

  gis_welcome_widget_show_locale (GIS_WELCOME_WIDGET (priv->welcome_widget),
                                  priv->new_locale_id);
}

static void
localed_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
  GisLanguagePage *self = data;
  GisLanguagePagePrivate *priv = gis_language_page_get_instance_private (self);
  GDBusProxy *proxy;
  GError *error = NULL;

  proxy = g_dbus_proxy_new_finish (res, &error);

  if (!proxy) {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to contact localed: %s", error->message);
      g_error_free (error);
      return;
  }

  priv->localed = proxy;
}

static void
update_distro_logo (GisLanguagePage *page)
{
  GisLanguagePagePrivate *priv = gis_language_page_get_instance_private (page);
  g_autofree char *id = g_get_os_info (G_OS_INFO_KEY_ID);
  gsize i;

  static const struct {
    const char *id;
    const char *logo;
  } id_to_logo[] = {
    { "debian",                         "emblem-debian" },
    { "endless",                        "endlessos" },
    { "fedora",                         "fedora-logo-icon" },
    { "ubuntu",                         "ubuntu-logo-icon" },
    { "openSUSE Tumbleweed",            "opensuse-logo-icon" },
    { "openSUSE Leap",                  "opensuse-logo-icon" },
    { "SLED",                           "suse-logo-icon" },
    { "SLES",                           "suse-logo-icon" },
  };

  for (i = 0; i < G_N_ELEMENTS (id_to_logo); i++)
    {
      if (g_strcmp0 (id, id_to_logo[i].id) == 0)
        {
          g_object_set (priv->logo, "icon-name", id_to_logo[i].logo, NULL);
          break;
        }
    }
}

static void
language_confirmed (CcLanguageChooser *chooser,
                    GisLanguagePage   *page)
{
  gis_assistant_next_page (gis_driver_get_assistant (GIS_PAGE (page)->driver));
}

static gboolean
gis_language_page_show_factory_dialog (GtkWidget *widget,
                                       GVariant  *args,
                                       gpointer   user_data)
{
  gis_factory_dialog_show_for_widget (widget);

  return TRUE;
}

static void
gis_language_page_constructed (GObject *object)
{
  GisLanguagePage *page = GIS_LANGUAGE_PAGE (object);
  GisLanguagePagePrivate *priv = gis_language_page_get_instance_private (page);
  GDBusConnection *bus;

  g_type_ensure (CC_TYPE_LANGUAGE_CHOOSER);

  G_OBJECT_CLASS (gis_language_page_parent_class)->constructed (object);

  update_distro_logo (page);
  set_time_format (NULL);

  g_signal_connect (priv->language_chooser, "notify::language",
                    G_CALLBACK (language_changed), page);
  g_signal_connect (priv->language_chooser, "confirm",
                    G_CALLBACK (language_confirmed), page);

  /* If we're in new user mode then we're manipulating system settings */
  if (gis_driver_get_mode (GIS_PAGE (page)->driver) == GIS_DRIVER_MODE_NEW_USER)
    {
      priv->permission = polkit_permission_new_sync ("org.freedesktop.locale1.set-locale", NULL, NULL, NULL);

      bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
      g_dbus_proxy_new (bus,
                        G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                        NULL,
                        "org.freedesktop.locale1",
                        "/org/freedesktop/locale1",
                        "org.freedesktop.locale1",
                        priv->cancellable,
                        (GAsyncReadyCallback) localed_proxy_ready,
                        object);
      g_object_unref (bus);
    }

  gis_page_set_complete (GIS_PAGE (page), TRUE);
  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_language_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Welcome"));
}

static void
reformatter_exited_cb (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GisPage *page = GIS_PAGE (source);
  /* We claim that the apply operation was unsuccessful so that the assistant
   * doesn't advance to the next page.
   */
  gboolean valid = FALSE;

  gis_page_apply_complete (page, valid);
}

static gboolean
gis_language_page_apply (GisPage      *page,
                         GCancellable *cancellable)
{
  if (!gis_driver_is_reformatter (page->driver))
    return FALSE;

  /* We abuse the "apply" hook to not actually advance to the next page when
   * launching the reformatter.
   */
  gis_page_util_run_reformatter (page, reformatter_exited_cb, NULL);
  return TRUE;
}

/* See https://github.com/endlessm/eos-installer/blob/master/UNATTENDED.md for
 * full documentation on the reformatter's unattended mode.
 */

/* On eosinstaller images, a run-mount-eosimages.mount unit is shipped which
 * arranges for this path to be mounted.
 */
#define EOSIMAGES_MOUNT_POINT "/run/mount/eosimages/"
/* Created by hand, or by hitting Ctrl+U on the last page of the reformatter.
 * Its mere presence triggers an unattended installation; it may also specify a
 * locale.
 */
#define UNATTENDED_INI_PATH EOSIMAGES_MOUNT_POINT "unattended.ini"
/* Created by the installer for Windows when it creates a reformatter USB.
 * Doesn't trigger unattended mode, just pre-selects a UI language.
 */
#define INSTALL_INI_PATH EOSIMAGES_MOUNT_POINT "install.ini"

#define LOCALE_GROUP "EndlessOS"
#define LOCALE_KEY "locale"

/*
 * check_reformatter_key_file:
 * @locale: (out): locale specified within @path, or %NULL if none was
 *  specified.
 *
 * Checks whether @path is a valid keyfile, and whether it specifies a locale.
 *
 * Returns: %TRUE if @path could be read.
 */
static gboolean
check_reformatter_key_file (const gchar *path,
                            gchar      **locale)
{
  g_autoptr(GKeyFile) key_file = g_key_file_new ();
  g_autoptr(GError) error = NULL;

  g_return_val_if_fail (locale != NULL, FALSE);
  g_return_val_if_fail (*locale == NULL, FALSE);

  if (!g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, &error))
    {
      if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        g_warning ("Failed to read %s: %s", path, error->message);

      return FALSE;
    }

  *locale = g_key_file_get_string (key_file, LOCALE_GROUP, LOCALE_KEY, &error);
  if (error != NULL &&
      !g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND) &&
      !g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND))
    {
      g_warning ("Failed to read locale from %s: %s", path, error->message);
    }

  return TRUE;
}

static void
gis_language_page_shown (GisPage *page)
{
  GisLanguagePage *self = GIS_LANGUAGE_PAGE (page);
  GisLanguagePagePrivate *priv = gis_language_page_get_instance_private (self);
  gboolean launch_immediately = FALSE;
  g_autofree gchar *locale = NULL;

  /* For now, only support unattended mode on eosinstaller images. */
  if (!gis_driver_is_reformatter (page->driver))
    return;

  /* Only take action once. This prevents an infinite loop if eos-installer
   * crashes on launch.
   */
  if (priv->checked_unattended_config)
    return;

  priv->checked_unattended_config = TRUE;

  if (check_reformatter_key_file (UNATTENDED_INI_PATH, &locale))
    launch_immediately = TRUE;
  else
    check_reformatter_key_file (INSTALL_INI_PATH, &locale);

  if (locale != NULL)
    cc_language_chooser_set_language (CC_LANGUAGE_CHOOSER (priv->language_chooser),
                                      locale);

  if (launch_immediately)
    gis_page_util_run_reformatter (page, NULL, NULL);
}

static void
gis_language_page_dispose (GObject *object)
{
  GisLanguagePage *page = GIS_LANGUAGE_PAGE (object);
  GisLanguagePagePrivate *priv = gis_language_page_get_instance_private (page);

  g_clear_object (&priv->permission);
  g_clear_object (&priv->localed);
  g_clear_object (&priv->cancellable);

  G_OBJECT_CLASS (gis_language_page_parent_class)->dispose (object);
}

static void
gis_language_page_class_init (GisLanguagePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-language-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisLanguagePage, welcome_widget);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisLanguagePage, language_chooser);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisLanguagePage, logo);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_language_page_locale_changed;
  page_class->apply = gis_language_page_apply;
  page_class->shown = gis_language_page_shown;
  object_class->constructed = gis_language_page_constructed;
  object_class->dispose = gis_language_page_dispose;
}

static void
gis_language_page_init (GisLanguagePage *page)
{
  g_resources_register (language_get_resource ());
  g_type_ensure (GIS_TYPE_WELCOME_WIDGET);
  g_type_ensure (CC_TYPE_LANGUAGE_CHOOSER);

  gtk_widget_init_template (GTK_WIDGET (page));

  GtkEventController *controller = gtk_shortcut_controller_new ();
  gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
  gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (controller), GTK_SHORTCUT_SCOPE_MANAGED);

  GtkShortcutTrigger *trigger = gtk_shortcut_trigger_parse_string ("<Ctrl>f");
  GtkShortcutAction *action = gtk_callback_action_new (gis_language_page_show_factory_dialog, NULL, NULL);
  GtkShortcut *shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);
  gtk_widget_add_controller (GTK_WIDGET (page), controller);
}

GisPage *
gis_prepare_language_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_LANGUAGE_PAGE,
                       "driver", driver,
                       NULL);
}

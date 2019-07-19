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

#include "config.h"

#include "gnome-initial-setup.h"

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <webkit2/webkit2.h>
#include <gio/gunixmounts.h>

#include "cc-common-language.h"
#include "gis-assistant.h"
#include "gis-page-util.h"

#define GIS_TYPE_DRIVER_MODE (gis_driver_mode_get_type ())

/* Statically include this for now. Maybe later
 * we'll generate this from glib-mkenums. */
GType
gis_driver_mode_get_type (void) {
  static GType enum_type_id = 0;
  if (G_UNLIKELY (!enum_type_id))
    {
      static const GEnumValue values[] = {
        { GIS_DRIVER_MODE_NEW_USER, "GIS_DRIVER_MODE_NEW_USER", "new_user" },
        { GIS_DRIVER_MODE_EXISTING_USER, "GIS_DRIVER_MODE_EXISTING_USER", "existing_user" },
        { 0, NULL, NULL }
      };
      enum_type_id = g_enum_register_static("GisDriverMode", values);
    }
  return enum_type_id;
}

enum {
  REBUILD_PAGES,
  LOCALE_CHANGED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL];

typedef enum {
  PROP_MODE = 1,
  PROP_LIVE_SESSION,
  PROP_LIVE_PERSISTENCE,
  PROP_USERNAME,
  PROP_PASSWORDLESS,
  PROP_SMALL_SCREEN,
  PROP_PARENTAL_CONTROLS_ENABLED,
  PROP_FULL_NAME,
  PROP_AVATAR,
} GisDriverProperty;

static GParamSpec *obj_props[PROP_AVATAR + 1];

struct _GisDriver {
  GtkApplication  parent_instance;

  GtkWindow *main_window;
  GisAssistant *assistant;

  GdmClient *client;
  GdmGreeter *greeter;
  GdmUserVerifier *user_verifier;

  ActUser *user_account;
  gchar *user_password;

  ActUser *parent_account;  /* (owned) (nullable) */
  gchar *parent_password;  /* (owned) (nullable) */

  gboolean parental_controls_enabled;

  gchar *lang_id;
  gchar *username;
  gchar *full_name;  /* (owned) (nullable) */
  gboolean passwordless;

  GdkPixbuf *avatar;  /* (owned) (nullable) */

  gboolean is_live_session;
  gboolean is_reformatter;
  gboolean has_live_persistence;

  GisDriverMode mode;
  UmAccountMode account_mode;
  gboolean small_screen;

  locale_t locale;

  const gchar *vendor_conf_file_path;
  GKeyFile *vendor_conf_file;

  gchar *product_name;
};

G_DEFINE_TYPE (GisDriver, gis_driver, GTK_TYPE_APPLICATION)

/* Should be kept in sync with eos-installer */
static gboolean
check_for_live_boot (gchar **uuid)
{
  const gchar *force = NULL;
  GError *error = NULL;
  g_autofree gchar *cmdline = NULL;
  gboolean live_boot = FALSE;
  g_autoptr(GRegex) reg = NULL;
  g_autoptr(GMatchInfo) info = NULL;

  g_return_val_if_fail (uuid != NULL, FALSE);

  force = g_getenv ("EI_FORCE_LIVE_BOOT_UUID");
  if (force != NULL && *force != '\0')
    {
      *uuid = g_strdup (force);
      return TRUE;
    }

  if (!g_file_get_contents ("/proc/cmdline", &cmdline, NULL, &error))
    {
      g_error_free (error);
      return FALSE;
    }

  live_boot = g_regex_match_simple ("\\bendless\\.live_boot\\b", cmdline, 0, 0);

  reg = g_regex_new ("\\bendless\\.image\\.device=UUID=([^\\s]*)", 0, 0, NULL);
  g_regex_match (reg, cmdline, 0, &info);
  if (g_match_info_matches (info))
    *uuid = g_match_info_fetch (info, 1);

  return live_boot;
}

static gboolean
running_live_session (void)
{
  g_autofree gchar *uuid = NULL;

  if (!check_for_live_boot (&uuid))
    return FALSE;

  return TRUE;
}

static void
check_live_persistence (GisDriver *driver)
{
  g_autoptr(GUnixMountEntry) entry = NULL;

  if (driver->is_live_session)
    {
      entry = g_unix_mount_at ("/run/eos-live", NULL);
      driver->has_live_persistence = entry != NULL;
    }
  else
    {
      driver->has_live_persistence = FALSE;
    }
}

#define EOS_IMAGE_VERSION_PATH "/sysroot"
#define EOS_IMAGE_VERSION_ALT_PATH "/"

static char *
get_image_version (void)
{
  g_autoptr(GError) error_sysroot = NULL;
  g_autoptr(GError) error_root = NULL;
  char *image_version =
    gis_page_util_get_image_version (EOS_IMAGE_VERSION_PATH, &error_sysroot);

  if (image_version == NULL)
    image_version =
      gis_page_util_get_image_version (EOS_IMAGE_VERSION_ALT_PATH, &error_root);

  if (image_version == NULL)
    {
      g_warning ("%s", error_sysroot->message);
      g_warning ("%s", error_root->message);
    }

  return image_version;
}

static gboolean
image_is_reformatter (const gchar *image_version)
{
  return image_version != NULL &&
    g_str_has_prefix (image_version, "eosinstaller-");
}

static gchar *
get_product_from_image_version (const gchar *image_version)
{
  gchar *hyphen_index = NULL;

  if (image_version == NULL)
    return NULL;

  hyphen_index = index (image_version, '-');
  if (hyphen_index == NULL)
    return NULL;

  return g_strndup (image_version, hyphen_index - image_version);
}

const gchar *
gis_driver_get_product_name (GisDriver *driver)
{
  return driver->product_name;
}

static void
gis_driver_dispose (GObject *object)
{
  GisDriver *driver = GIS_DRIVER (object);

  g_clear_object (&driver->user_verifier);
  g_clear_object (&driver->greeter);
  g_clear_object (&driver->client);

  G_OBJECT_CLASS (gis_driver_parent_class)->dispose (object);
}

static void
gis_driver_finalize (GObject *object)
{
  GisDriver *driver = GIS_DRIVER (object);

  g_free (driver->product_name);
  g_free (driver->lang_id);
  g_free (driver->username);
  g_free (driver->full_name);
  g_free (driver->user_password);

  g_clear_object (&driver->avatar);

  g_clear_object (&driver->user_account);
  g_clear_pointer (&driver->vendor_conf_file, g_key_file_free);

  g_clear_object (&driver->parent_account);
  g_free (driver->parent_password);

  if (driver->locale != (locale_t) 0)
    {
      uselocale (LC_GLOBAL_LOCALE);
      freelocale (driver->locale);
    }

  G_OBJECT_CLASS (gis_driver_parent_class)->finalize (object);
}

static void
assistant_page_changed (GtkScrolledWindow *sw)
{
  gtk_adjustment_set_value (gtk_scrolled_window_get_vadjustment (sw), 0);
}

static void
prepare_main_window (GisDriver *driver)
{
  GtkWidget *child, *sw;

  child = g_object_ref (gtk_bin_get_child (GTK_BIN (driver->main_window)));
  gtk_container_remove (GTK_CONTAINER (driver->main_window), child);
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);
  gtk_container_add (GTK_CONTAINER (driver->main_window), sw);
  gtk_container_add (GTK_CONTAINER (sw), child);
  g_object_unref (child);

  g_signal_connect_swapped (driver->assistant,
                            "page-changed",
                            G_CALLBACK (assistant_page_changed),
                            sw);

  gtk_window_set_titlebar (driver->main_window,
                           gis_assistant_get_titlebar (driver->assistant));
}

static void
rebuild_pages (GisDriver *driver)
{
  g_signal_emit (G_OBJECT (driver), signals[REBUILD_PAGES], 0);
}

GisAssistant *
gis_driver_get_assistant (GisDriver *driver)
{
  return driver->assistant;
}

static void
gis_driver_locale_changed (GisDriver *driver)
{
  GtkTextDirection direction;

  direction = gtk_get_locale_direction ();
  gtk_widget_set_default_direction (direction);

  rebuild_pages (driver);
  gis_assistant_locale_changed (driver->assistant);

  g_signal_emit (G_OBJECT (driver), signals[LOCALE_CHANGED], 0);
}

void
gis_driver_set_user_language (GisDriver *driver, const gchar *lang_id, gboolean update_locale)
{
  g_free (driver->lang_id);
  driver->lang_id = g_strdup (lang_id);

  cc_common_language_set_current_language (lang_id);

  if (update_locale)
    {
      locale_t locale = newlocale (LC_MESSAGES_MASK, lang_id, (locale_t) 0);
      if (locale == (locale_t) 0)
        {
          g_warning ("Failed to create locale %s: %s", lang_id, g_strerror (errno));
          return;
        }

      uselocale (locale);

      if (driver->locale != (locale_t) 0 && driver->locale != LC_GLOBAL_LOCALE)
        freelocale (driver->locale);
      driver->locale = locale;

      gis_driver_locale_changed (driver);
    }
}

const gchar *
gis_driver_get_user_language (GisDriver *driver)
{
  return driver->lang_id;
}

void
gis_driver_set_username (GisDriver *driver, const gchar *username)
{
  g_free (driver->username);
  driver->username = g_strdup (username);

  g_object_notify (G_OBJECT (driver), "username");
}

const gchar *
gis_driver_get_username (GisDriver *driver)
{
  return driver->username;
}

/**
 * gis_driver_set_full_name:
 * @driver: a #GisDriver
 * @full_name: (nullable): full name of the main user, or %NULL if not known
 *
 * Set the #GisDriver:full-name property.
 *
 * Since: 3.36
 */
void
gis_driver_set_full_name (GisDriver   *driver,
                          const gchar *full_name)
{
  g_return_if_fail (GIS_IS_DRIVER (driver));
  g_return_if_fail (full_name == NULL ||
                    g_utf8_validate (full_name, -1, NULL));

  if (g_strcmp0 (driver->full_name, full_name) == 0)
    return;

  g_free (driver->full_name);
  driver->full_name = g_strdup (full_name);

  g_object_notify_by_pspec (G_OBJECT (driver), obj_props[PROP_FULL_NAME]);
}

/**
 * gis_driver_get_full_name:
 * @driver: a #GisDriver
 *
 * Get the #GisDriver:full-name property.
 *
 * Returns: (nullable): full name of the main user, or %NULL if not known
 * Since: 3.36
 */
const gchar *
gis_driver_get_full_name (GisDriver *driver)
{
  g_return_val_if_fail (GIS_IS_DRIVER (driver), NULL);

  return driver->full_name;
}

/**
 * gis_driver_set_avatar:
 * @driver: a #GisDriver
 * @avatar: (nullable) (transfer none): avatar of the main user, or %NULL if not known
 *
 * Set the #GisDriver:avatar property.
 *
 * Since: 3.36
 */
void
gis_driver_set_avatar (GisDriver *driver,
                       GdkPixbuf *avatar)
{
  g_return_if_fail (GIS_IS_DRIVER (driver));
  g_return_if_fail (avatar == NULL || GDK_IS_PIXBUF (avatar));

  if (g_set_object (&driver->avatar, avatar))
    g_object_notify_by_pspec (G_OBJECT (driver), obj_props[PROP_AVATAR]);
}

/**
 * gis_driver_get_avatar:
 * @driver: a #GisDriver
 *
 * Get the #GisDriver:avatar property.
 *
 * Returns: (nullable) (transfer none): avatar of the main user, or %NULL if not known
 * Since: 3.36
 */
GdkPixbuf *
gis_driver_get_avatar (GisDriver *driver)
{
  g_return_val_if_fail (GIS_IS_DRIVER (driver), NULL);

  return driver->avatar;
}

void
gis_driver_set_passwordless (GisDriver *self,
                             gboolean   passwordless)
{
  gboolean should_notify = (self->passwordless != passwordless);
  self->passwordless = passwordless;
  if (should_notify)
    g_object_notify (G_OBJECT (self), "passwordless");
}

gboolean
gis_driver_get_passwordless (GisDriver *self)
{
  return self->passwordless;
}

void
gis_driver_set_user_permissions (GisDriver   *driver,
                                 ActUser     *user,
                                 const gchar *password)
{
  g_set_object (&driver->user_account, user);
  g_free (driver->user_password);
  driver->user_password = g_strdup (password);
}

void
gis_driver_get_user_permissions (GisDriver    *driver,
                                 ActUser     **user,
                                 const gchar **password)
{
  if (user != NULL)
    *user = driver->user_account;

  if (password != NULL)
    *password = driver->user_password;
}

/**
 * gis_driver_set_parent_permissions:
 * @driver: a #GisDriver
 * @parent: (transfer none): user account for the parent
 * @password: password for the parent
 *
 * Stores the parent account details for later use when saving the initial setup
 * data.
 *
 * Since: 3.36
 */
void
gis_driver_set_parent_permissions (GisDriver   *driver,
                                   ActUser     *parent,
                                   const gchar *password)
{
  g_set_object (&driver->parent_account, parent);
  g_free (driver->parent_password);
  driver->parent_password = g_strdup (password);
}

/**
 * gis_driver_get_parent_permissions:
 * @driver: a #GisDriver
 * @parent: (out) (transfer none) (optional) (nullable): return location for the
 *    user account for the parent, which may be %NULL
 * @password: (out) (transfer none) (optional) (nullable): return location for
 *    the password for the parent
 *
 * Gets the parent account details saved from an earlier step in the initial
 * setup process. They may be %NULL if not set yet.
 *
 * Since: 3.36
 */
void
gis_driver_get_parent_permissions (GisDriver    *driver,
                                   ActUser     **parent,
                                   const gchar **password)
{
  if (parent != NULL)
    *parent = driver->parent_account;
  if (password != NULL)
    *password = driver->parent_password;
}

void
gis_driver_set_account_mode (GisDriver     *driver,
                             UmAccountMode  mode)
{
  driver->account_mode = mode;
}

UmAccountMode
gis_driver_get_account_mode (GisDriver *driver)
{
  return driver->account_mode;
}

/**
 * gis_driver_set_parental_controls_enabled:
 * @driver: a #GisDriver
 * @parental_controls_enabled: whether parental controls are enabled for the main user
 *
 * Set the #GisDriver:parental-controls-enabled property.
 *
 * Since: 3.36
 */
void
gis_driver_set_parental_controls_enabled (GisDriver *driver,
                                          gboolean   parental_controls_enabled)
{
  if (driver->parental_controls_enabled == parental_controls_enabled)
    return;

  driver->parental_controls_enabled = parental_controls_enabled;
  rebuild_pages (driver);

  g_object_notify_by_pspec (G_OBJECT (driver), obj_props[PROP_PARENTAL_CONTROLS_ENABLED]);
}

/**
 * gis_driver_get_parental_controls_enabled:
 * @driver: a #GisDriver
 *
 * Get the #GisDriver:parental-controls-enabled property.
 *
 * Returns: whether parental controls are enabled for the main user
 * Since: 3.36
 */
gboolean
gis_driver_get_parental_controls_enabled (GisDriver *driver)
{
  return driver->parental_controls_enabled;
}

gboolean
gis_driver_get_gdm_objects (GisDriver        *driver,
                            GdmGreeter      **greeter,
                            GdmUserVerifier **user_verifier)
{
  if (driver->greeter == NULL || driver->user_verifier == NULL)
    return FALSE;

  *greeter = driver->greeter;
  *user_verifier = driver->user_verifier;

  return TRUE;
}

void
gis_driver_add_page (GisDriver *driver,
                     GisPage   *page)
{
  gis_assistant_add_page (driver->assistant, page);
}

void
gis_driver_show_window (GisDriver *driver)
{
  gtk_window_present (driver->main_window);
}

void
gis_driver_hide_window (GisDriver *driver)
{
  gtk_widget_hide (GTK_WIDGET (driver->main_window));
}

static gboolean
load_vendor_conf_file_at_path (GisDriver *driver,
                               const char *path)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GKeyFile) vendor_conf_file = g_key_file_new ();

  if (!g_key_file_load_from_file (vendor_conf_file, path, G_KEY_FILE_NONE, &error))
    {
      if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        g_warning ("Could not read file %s: %s:", path, error->message);
      return FALSE;
    }

  driver->vendor_conf_file_path = path;
  driver->vendor_conf_file = g_steal_pointer (&vendor_conf_file);
  return TRUE;
}

static void
load_vendor_conf_file (GisDriver *driver)
{
#ifdef VENDOR_CONF_FILE
  load_vendor_conf_file_at_path (driver, VENDOR_CONF_FILE);
#else
  /* If no path was passed at build time, then we have search path:
   *
   *  - First check $(sysconfdir)/gnome-initial-setup/vendor.conf
   *  - Then check $(datadir)/gnome-initial-setup/vendor.conf
   *
   * This allows distributions to provide a default packaged config in a
   * location that might be managed by ostree, and allows OEMs to
   * override using an unmanaged location.
   */
  if (!load_vendor_conf_file_at_path (driver, PKGSYSCONFDIR "/vendor.conf"))
    load_vendor_conf_file_at_path (driver, PKGDATADIR "/vendor.conf");
#endif
}

static void
report_conf_error_if_needed (GisDriver *driver,
                             const gchar *group,
                             const gchar *key,
                             const GError *error)
{
  if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND) &&
      !g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND))
    g_warning ("Error getting the value for key '%s' of group [%s] in %s: %s",
               group, key, driver->vendor_conf_file_path, error->message);
}

gboolean
gis_driver_conf_get_boolean (GisDriver *driver,
                             const gchar *group,
                             const gchar *key,
                             gboolean default_value)
{
  if (driver->vendor_conf_file) {
    g_autoptr(GError) error = NULL;
    gboolean new_value = g_key_file_get_boolean (driver->vendor_conf_file, group,
                                                 key, &error);
    if (error == NULL)
      return new_value;

    report_conf_error_if_needed (driver, group, key, error);
  }

  return default_value;
}

GStrv
gis_driver_conf_get_string_list (GisDriver *driver,
                                 const gchar *group,
                                 const gchar *key,
                                 gsize *out_length)
{
  if (driver->vendor_conf_file) {
    g_autoptr(GError) error = NULL;
    GStrv new_value = g_key_file_get_string_list (driver->vendor_conf_file, group,
                                                  key, out_length, &error);
    if (error == NULL)
      return new_value;

    report_conf_error_if_needed (driver, group, key, error);
  }

  return NULL;
}

gchar *
gis_driver_conf_get_string (GisDriver *driver,
                            const gchar *group,
                            const gchar *key)
{
  if (driver->vendor_conf_file) {
    g_autoptr(GError) error = NULL;
    gchar *new_value = g_key_file_get_string (driver->vendor_conf_file, group,
                                              key, &error);
    if (error == NULL)
      return new_value;

    report_conf_error_if_needed (driver, group, key, error);
  }

  return NULL;
}

GisDriverMode
gis_driver_get_mode (GisDriver *driver)
{
  return driver->mode;
}

gboolean
gis_driver_is_live_session (GisDriver *driver)
{
    return driver->is_live_session;
}

gboolean
gis_driver_has_live_persistence (GisDriver *driver)
{
    return driver->has_live_persistence;
}

gboolean
gis_driver_is_reformatter (GisDriver *driver)
{
  return driver->is_reformatter;
}

gboolean
gis_driver_is_small_screen (GisDriver *driver)
{
  return driver->small_screen;
}

static gboolean
monitor_is_small (GdkMonitor *monitor)
{
  GdkRectangle geom;

  if (g_getenv ("GIS_SMALL_SCREEN"))
    return TRUE;

  gdk_monitor_get_geometry (monitor, &geom);
  return geom.height < 800;
}

static void
gis_driver_get_property (GObject      *object,
                         guint         prop_id,
                         GValue       *value,
                         GParamSpec   *pspec)
{
  GisDriver *driver = GIS_DRIVER (object);

  switch ((GisDriverProperty) prop_id)
    {
    case PROP_LIVE_SESSION:
      g_value_set_boolean (value, driver->is_live_session);
      break;
    case PROP_LIVE_PERSISTENCE:
      g_value_set_boolean (value, driver->has_live_persistence);
      break;
    case PROP_MODE:
      g_value_set_enum (value, driver->mode);
      break;
    case PROP_USERNAME:
      g_value_set_string (value, driver->username);
      break;
    case PROP_PASSWORDLESS:
      g_value_set_boolean (value, driver->passwordless);
      break;
    case PROP_SMALL_SCREEN:
      g_value_set_boolean (value, driver->small_screen);
      break;
    case PROP_PARENTAL_CONTROLS_ENABLED:
      g_value_set_boolean (value, driver->parental_controls_enabled);
      break;
    case PROP_FULL_NAME:
      g_value_set_string (value, driver->full_name);
      break;
    case PROP_AVATAR:
      g_value_set_object (value, driver->avatar);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gis_driver_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GisDriver *driver = GIS_DRIVER (object);

  switch ((GisDriverProperty) prop_id)
    {
    case PROP_MODE:
      driver->mode = g_value_get_enum (value);
      break;
    case PROP_USERNAME:
      g_free (driver->username);
      driver->username = g_value_dup_string (value);
      break;
    case PROP_PASSWORDLESS:
      driver->passwordless = g_value_get_boolean (value);
      break;
    case PROP_PARENTAL_CONTROLS_ENABLED:
      gis_driver_set_parental_controls_enabled (driver, g_value_get_boolean (value));
      break;
    case PROP_FULL_NAME:
      gis_driver_set_full_name (driver, g_value_get_string (value));
      break;
    case PROP_AVATAR:
      gis_driver_set_avatar (driver, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gis_driver_activate (GApplication *app)
{
  GisDriver *driver = GIS_DRIVER (app);

  G_APPLICATION_CLASS (gis_driver_parent_class)->activate (app);

  gtk_window_present (GTK_WINDOW (driver->main_window));
}

static void
set_small_screen_based_on_primary_monitor (GisDriver *driver)
{
  GdkDisplay *default_display;
  GdkMonitor *primary_monitor;

  default_display = gdk_display_get_default ();
  if (default_display == NULL)
    return;

  primary_monitor = gdk_display_get_primary_monitor (default_display);
  if (primary_monitor == NULL)
    return;

  driver->small_screen = monitor_is_small (primary_monitor);
}

/* Recompute driver->small_screen based on the monitor where the window is
 * located, if the window is actually realized. If not, recompute it based on
 * the primary monitor of the default display. */
static void
recompute_small_screen (GisDriver *driver) {
  GdkWindow *window;
  GdkDisplay *default_display = gdk_display_get_default ();
  GdkMonitor *active_monitor;
  gboolean old_value = driver->small_screen;

  if (!gtk_widget_get_realized (GTK_WIDGET (driver->main_window)))
    {
      set_small_screen_based_on_primary_monitor (driver);
    }
  else
    {
      window = gtk_widget_get_window (GTK_WIDGET (driver->main_window));
      active_monitor = gdk_display_get_monitor_at_window (default_display, window);
      driver->small_screen = monitor_is_small (active_monitor);
    }

  if (driver->small_screen != old_value)
    g_object_notify (G_OBJECT (driver), "small-screen");
}

static void
update_screen_size (GisDriver *driver)
{
  GdkWindow *window;
  GdkGeometry size_hints;
  GtkWidget *sw;

  recompute_small_screen (driver);

  if (!gtk_widget_get_realized (GTK_WIDGET (driver->main_window)))
    return;

  sw = gtk_bin_get_child (GTK_BIN (driver->main_window));
  window = gtk_widget_get_window (GTK_WIDGET (driver->main_window));

  if (driver->small_screen)
    {
      if (window)
        gdk_window_set_functions (window,
                                  GDK_FUNC_ALL | GDK_FUNC_MINIMIZE | GDK_FUNC_CLOSE);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);

      gtk_window_set_geometry_hints (driver->main_window, NULL, NULL, 0);
      gtk_window_set_resizable (driver->main_window, TRUE);
      gtk_window_set_position (driver->main_window, GTK_WIN_POS_NONE);

      gtk_window_maximize (driver->main_window);
      gtk_window_present (driver->main_window);
    }
  else
    {
      if (window)
        gdk_window_set_functions (window,
                                  GDK_FUNC_ALL | GDK_FUNC_MINIMIZE | GDK_FUNC_CLOSE |
                                  GDK_FUNC_RESIZE | GDK_FUNC_MOVE | GDK_FUNC_MAXIMIZE);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_NEVER);

      size_hints.min_width = size_hints.max_width = 1024;
      size_hints.min_height = size_hints.max_height = 768;
      size_hints.win_gravity = GDK_GRAVITY_CENTER;

      gtk_window_set_geometry_hints (driver->main_window,
                                     NULL,
                                     &size_hints,
                                     GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE | GDK_HINT_WIN_GRAVITY);
      gtk_window_set_resizable (driver->main_window, FALSE);
      gtk_window_set_position (driver->main_window, GTK_WIN_POS_CENTER_ALWAYS);

      gtk_window_unmaximize (driver->main_window);
      gtk_window_present (driver->main_window);
    }
}

static void
screen_size_changed (GdkScreen *screen, GisDriver *driver)
{
  update_screen_size (driver);
}

static void
window_realize_cb (GtkWidget *widget, gpointer user_data)
{
  update_screen_size (GIS_DRIVER (user_data));
}

static void
connect_to_gdm (GisDriver *driver)
{
  g_autoptr(GError) error = NULL;

  driver->client = gdm_client_new ();

  driver->greeter = gdm_client_get_greeter_sync (driver->client, NULL, &error);
  if (error == NULL)
    driver->user_verifier = gdm_client_get_user_verifier_sync (driver->client, NULL, &error);

  if (error != NULL) {
    g_warning ("Failed to open connection to GDM: %s", error->message);
    g_clear_object (&driver->user_verifier);
    g_clear_object (&driver->greeter);
    g_clear_object (&driver->client);
  }
}

static void
gis_driver_startup (GApplication *app)
{
  GisDriver *driver = GIS_DRIVER (app);
  WebKitWebContext *context = webkit_web_context_get_default ();
  g_autofree char *image_version = NULL;

  G_APPLICATION_CLASS (gis_driver_parent_class)->startup (app);

  webkit_web_context_set_sandbox_enabled (context, TRUE);

  if (driver->mode == GIS_DRIVER_MODE_NEW_USER)
    connect_to_gdm (driver);

  driver->main_window = g_object_new (GTK_TYPE_APPLICATION_WINDOW,
                                    "application", app,
                                    "type", GTK_WINDOW_TOPLEVEL,
                                    "icon-name", "preferences-system",
                                    "deletable", FALSE,
                                    NULL);

  g_signal_connect (driver->main_window,
                    "realize",
                    G_CALLBACK (window_realize_cb),
                    (gpointer)app);

  driver->assistant = g_object_new (GIS_TYPE_ASSISTANT, NULL);
  gtk_container_add (GTK_CONTAINER (driver->main_window), GTK_WIDGET (driver->assistant));

  gtk_widget_show (GTK_WIDGET (driver->assistant));

  image_version = get_image_version ();
  driver->product_name = get_product_from_image_version (image_version);

  driver->is_live_session = running_live_session ();
  g_object_notify_by_pspec (G_OBJECT (driver), obj_props[PROP_LIVE_SESSION]);

  check_live_persistence (driver);
  g_object_notify_by_pspec (G_OBJECT (driver), obj_props[PROP_LIVE_PERSISTENCE]);

  driver->is_reformatter = image_is_reformatter (image_version);

  gis_driver_set_user_language (driver, setlocale (LC_MESSAGES, NULL), FALSE);

  prepare_main_window (driver);
  rebuild_pages (driver);
}

static void
gis_driver_init (GisDriver *driver)
{
  GdkScreen *screen;

  screen = gdk_screen_get_default ();

  set_small_screen_based_on_primary_monitor (driver);

  load_vendor_conf_file (driver);

  if (screen != NULL)
    g_signal_connect (screen, "size-changed",
                      G_CALLBACK (screen_size_changed), driver);
}

static void
gis_driver_class_init (GisDriverClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = gis_driver_get_property;
  gobject_class->set_property = gis_driver_set_property;
  gobject_class->dispose = gis_driver_dispose;
  gobject_class->finalize = gis_driver_finalize;
  application_class->startup = gis_driver_startup;
  application_class->activate = gis_driver_activate;

  signals[REBUILD_PAGES] =
    g_signal_new ("rebuild-pages",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[LOCALE_CHANGED] =
    g_signal_new ("locale-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  obj_props[PROP_LIVE_SESSION] =
    g_param_spec_boolean ("live-session", "", "",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  obj_props[PROP_LIVE_PERSISTENCE] =
    g_param_spec_boolean ("live-persistence", "", "",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  obj_props[PROP_MODE] =
    g_param_spec_enum ("mode", "", "",
                       GIS_TYPE_DRIVER_MODE,
                       GIS_DRIVER_MODE_EXISTING_USER,
                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_props[PROP_USERNAME] =
    g_param_spec_string ("username", "", "",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_props[PROP_PASSWORDLESS] =
    g_param_spec_boolean ("passwordless", "", "Main user account is passwordless",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_props[PROP_SMALL_SCREEN] =
    g_param_spec_boolean ("small-screen", "", "",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GisDriver:parental-controls-enabled:
   *
   * Whether parental controls are enabled for the main user. If this is %TRUE,
   * two user accounts will be created when this page is saved: one for the main
   * user (a child) which will be a standard account; and one for the parent
   * which will be an administrative account.
   *
   * Since: 3.36
   */
  obj_props[PROP_PARENTAL_CONTROLS_ENABLED] =
    g_param_spec_boolean ("parental-controls-enabled",
                          "Parental Controls Enabled",
                          "Whether parental controls are enabled for the main user.",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GisDriver:full-name: (nullable)
   *
   * Full name of the main user. May be %NULL if unknown or not set yet.
   *
   * Since: 3.36
   */
  obj_props[PROP_FULL_NAME] =
    g_param_spec_string ("full-name",
                         "Full Name",
                         "Full name of the main user.",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GisDriver:avatar: (nullable)
   *
   * Avatar of the main user. May be %NULL if unknown or not set yet.
   *
   * Since: 3.36
   */
  obj_props[PROP_AVATAR] =
    g_param_spec_object ("avatar",
                         "Avatar",
                         "Avatar of the main user.",
                         GDK_TYPE_PIXBUF,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, G_N_ELEMENTS (obj_props), obj_props);
}

gboolean
gis_driver_save_data (GisDriver  *driver,
                      GError    **error)
{
  if (gis_get_mock_mode ())
    {
      g_message ("%s: Skipping saving data due to being in mock mode", G_STRFUNC);
      return TRUE;
    }

  return gis_assistant_save_data (driver->assistant, error);
}

GisDriver *
gis_driver_new (GisDriverMode mode)
{
  return g_object_new (GIS_TYPE_DRIVER,
                       "application-id", "org.gnome.InitialSetup",
                       "mode", mode,
                       NULL);
}

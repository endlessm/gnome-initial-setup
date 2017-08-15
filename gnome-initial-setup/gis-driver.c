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

#include <stdlib.h>
#include <locale.h>

#include "gis-assistant.h"

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

enum {
  PROP_0,
  PROP_LIVE_SESSION,
  PROP_MODE,
  PROP_USERNAME,
  PROP_SMALL_SCREEN,
  PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

struct _GisDriverPrivate {
  GtkWindow *main_window;
  GisAssistant *assistant;

  ActUser *user_account;
  gchar *user_password;

  gchar *lang_id;
  gchar *username;

  gboolean is_live_session;
  gboolean is_in_demo_mode;

  GisDriverMode mode;
  UmAccountMode account_mode;
  gboolean small_screen;
};
typedef struct _GisDriverPrivate GisDriverPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GisDriver, gis_driver, GTK_TYPE_APPLICATION)

static gboolean
running_live_session (void)
{
  /* This is the same variable used by eos-installer. We do not care about
   * the UUID of the image partition here, just whether the variable is set.
   */
  const gchar *force = g_getenv ("EI_FORCE_LIVE_BOOT_UUID");
  gboolean has_live_boot_param;
  GError *error = NULL;
  g_autofree gchar *contents = NULL;

  if (force != NULL && *force != '\0')
    {
      return TRUE;
    }


  if (!g_file_get_contents ("/proc/cmdline", &contents, NULL, &error))
    {
      g_warning ("Couldn't check kernel parameters: %s", error->message);
      g_clear_error (&error);
      return FALSE;
    }

  has_live_boot_param = g_strrstr (contents, "endless.live_boot") != NULL;

  return has_live_boot_param;
}

static void
gis_driver_finalize (GObject *object)
{
  GisDriver *driver = GIS_DRIVER (object);
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  g_free (priv->lang_id);
  g_free (priv->username);
  g_free (priv->user_password);

  g_clear_object (&priv->user_account);

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
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  GtkWidget *child, *sw;

  child = g_object_ref (gtk_bin_get_child (GTK_BIN (priv->main_window)));
  gtk_container_remove (GTK_CONTAINER (priv->main_window), child);
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);
  gtk_container_add (GTK_CONTAINER (priv->main_window), sw);
  gtk_container_add (GTK_CONTAINER (sw), child);
  g_object_unref (child);

  g_signal_connect_swapped (priv->assistant,
                            "page-changed",
                            G_CALLBACK (assistant_page_changed),
                            sw);

  gtk_window_set_titlebar (priv->main_window,
                           gis_assistant_get_titlebar (priv->assistant));
}

static void
rebuild_pages (GisDriver *driver)
{
  g_signal_emit (G_OBJECT (driver), signals[REBUILD_PAGES], 0);
}

GisAssistant *
gis_driver_get_assistant (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  return priv->assistant;
}

void
gis_driver_set_user_language (GisDriver *driver, const gchar *lang_id)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  g_free (priv->lang_id);
  priv->lang_id = g_strdup (lang_id);

  /* Now, if we already have a user configured, make sure to
   * propogate that change to the user */
  if (priv->user_account)
    act_user_set_language (priv->user_account, lang_id);
}

const gchar *
gis_driver_get_user_language (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  return priv->lang_id;
}

void
gis_driver_set_username (GisDriver *driver, const gchar *username)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  g_free (priv->username);
  priv->username = g_strdup (username);
  g_object_notify (G_OBJECT (driver), "username");
}

const gchar *
gis_driver_get_username (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  return priv->username;
}
void
gis_driver_set_user_permissions (GisDriver   *driver,
                                 ActUser     *user,
                                 const gchar *password)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  g_set_object (&priv->user_account, user);
  priv->user_password = g_strdup (password);
}

void
gis_driver_get_user_permissions (GisDriver    *driver,
                                 ActUser     **user,
                                 const gchar **password)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  *user = priv->user_account;
  *password = priv->user_password;
}

void
gis_driver_set_account_mode (GisDriver     *driver,
                             UmAccountMode  mode)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  priv->account_mode = mode;
}

UmAccountMode
gis_driver_get_account_mode (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  return priv->account_mode;
}

void
gis_driver_add_page (GisDriver *driver,
                     GisPage   *page)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  gis_assistant_add_page (priv->assistant, page);
}

void
gis_driver_show_window (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  gtk_window_present (priv->main_window);
}

void
gis_driver_hide_window (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  gtk_widget_hide (GTK_WIDGET (priv->main_window));
}

static void
gis_driver_real_locale_changed (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  GtkTextDirection direction;

  direction = gtk_get_locale_direction ();
  gtk_widget_set_default_direction (direction);

  rebuild_pages (driver);
  gis_assistant_locale_changed (priv->assistant);
}

void
gis_driver_locale_changed (GisDriver *driver)
{
  g_signal_emit (G_OBJECT (driver), signals[LOCALE_CHANGED], 0);
}

GisDriverMode
gis_driver_get_mode (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  return priv->mode;
}

gboolean
gis_driver_is_in_demo_mode (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  return priv->is_in_demo_mode;
}

#define DEMO_ACCOUNT_USERNAME "demo-guest"
#define DEMO_ACCOUNT_FULLNAME "Demo"

static void
handle_demo_mode_error (GError *error)
{
  GtkWidget *dialog;

  g_warning ("Failed to enter demo mode: %s", error->message);
  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   _("Failed to enter demo mode: %s"),
                                   error->message);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  g_error_free (error);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ActUser, g_object_unref)

static gboolean
create_demo_user (GisDriver *driver, GError **error)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  g_autoptr(ActUser) user;
  const gchar *language;

  user = act_user_manager_create_user (act_user_manager_get_default (),
                                       DEMO_ACCOUNT_USERNAME,
                                       DEMO_ACCOUNT_FULLNAME,
                                       ACT_USER_ACCOUNT_TYPE_STANDARD,
                                       error);
  if (!user)
    return FALSE;

  act_user_set_password_mode (user, ACT_USER_PASSWORD_MODE_NONE);
  act_user_set_automatic_login (user, TRUE);

  language = gis_driver_get_user_language (driver);

  if (language)
    act_user_set_language (user, language);

  gis_driver_set_user_permissions (driver, user, NULL);
  gis_update_login_keyring_password ("");

  return TRUE;
}

#define GSD_POWER_SCHEMA "org.gnome.settings-daemon.plugins.power"
#define GSD_SLEEP_INACTIVE_AC_TYPE "sleep-inactive-ac-type"
#define GSD_SLEEP_INACTIVE_AC_TIMEOUT "sleep-inactive-ac-timeout"
#define GSD_SLEEP_INACTIVE_BATTERY_TYPE "sleep-inactive-battery-type"
#define GSD_SLEEP_INACTIVE_BATTERY_TIMEOUT "sleep-inactive-battery-timeout"

#define ONE_MINUTE 60

static void
configure_demo_mode_power_settings (void)
{
  GSettings *power_settings = g_settings_new (GSD_POWER_SCHEMA);

  g_settings_set_string (power_settings, GSD_SLEEP_INACTIVE_AC_TYPE, "logout");
  g_settings_set_int (power_settings, GSD_SLEEP_INACTIVE_AC_TIMEOUT, ONE_MINUTE);
  g_settings_set_string (power_settings, GSD_SLEEP_INACTIVE_BATTERY_TYPE, "logout");
  g_settings_set_int (power_settings, GSD_SLEEP_INACTIVE_BATTERY_TIMEOUT, ONE_MINUTE);

  g_object_unref (power_settings);
}

#define GS_SCHEMA "org.gnome.software"
#define GS_ALLOW_UPDATES "allow-updates"

static void
configure_demo_mode_disallow_app_center_updates (void)
{
  GSettings *gnome_software_settings = g_settings_new (GS_SCHEMA);

  g_settings_set_boolean (gnome_software_settings, GS_ALLOW_UPDATES, FALSE);

  g_object_unref (gnome_software_settings);
}

#define GNOME_SHELL_SCHEMA "org.gnome.shell"
#define GNOME_SHELL_FAVORITE_APPS "favorite-apps"

#define GOOGLE_CHROME_DESKTOP_ID "google-chrome.desktop"
#define CHROMIUM_BROWSER_DESKTOP_ID "chromium-browser.desktop"

static void
replace_string_in_strv (GStrv       strv,
                        const gchar *needle,
                        const gchar *replacement)
{
  guint i = 0;

  while (strv[i] != NULL)
    {
      if (g_strcmp0 (strv[i], needle) == 0)
        {
          g_free (strv[i]);
          strv[i] = g_strdup (replacement);
        }

      ++i;
    }
}

static void
configure_demo_mode_replace_chrome_with_chromium (void)
{
  GSettings *gnome_shell_settings = g_settings_new (GNOME_SHELL_SCHEMA);
  GStrv favorite_apps = g_settings_get_strv (gnome_shell_settings,
                                             GNOME_SHELL_FAVORITE_APPS);
  replace_string_in_strv (favorite_apps,
                          GOOGLE_CHROME_DESKTOP_ID,
                          CHROMIUM_BROWSER_DESKTOP_ID);
  g_settings_set_strv (gnome_shell_settings,
                       GNOME_SHELL_FAVORITE_APPS,
                       (const gchar * const *) favorite_apps);

  g_strfreev (favorite_apps);
  g_object_unref (gnome_shell_settings);
}

gboolean
setup_demo_config (GisDriver *driver, GError **error)
{
  gchar *stamp_file = g_build_filename (g_get_user_config_dir (),
                                        "eos-demo-mode",
                                        NULL);

  if (!g_file_set_contents (stamp_file, "1", sizeof(char), error))
    {
      g_free (stamp_file);
      return FALSE;
    }

  configure_demo_mode_power_settings ();
  configure_demo_mode_disallow_app_center_updates ();
  configure_demo_mode_replace_chrome_with_chromium ();

  g_free (stamp_file);
  return TRUE;
}

void
gis_driver_enter_demo_mode (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  if (priv->is_in_demo_mode)
    return;

  GError *error = NULL;
  if (!gis_pkexec (LIBEXECDIR "/eos-demo-mode", DEMO_ACCOUNT_USERNAME, NULL, &error))
    {
      handle_demo_mode_error (error);
      return;
    }

  if (!create_demo_user (driver, &error))
    {
      handle_demo_mode_error (error);
      return;
    }

  if (!setup_demo_config (driver, &error))
    {
      handle_demo_mode_error (error);
      return;
    }

  priv->is_in_demo_mode = TRUE;

  /* Set up the demo user account, destroying it if necessary */
  gis_driver_set_username (driver, DEMO_ACCOUNT_USERNAME);

  rebuild_pages (driver);
}

gboolean
gis_driver_is_live_session (GisDriver *driver)
{
    GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
    return priv->is_live_session;
}

gboolean
gis_driver_is_small_screen (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  return priv->small_screen;
}

static gboolean
screen_is_small (GdkScreen *screen)
{
  if (g_getenv ("GIS_SMALL_SCREEN"))
    return TRUE;

  return gdk_screen_get_height (screen) < 600 ||
         gdk_screen_get_width (screen) < 800;
}

static void
gis_driver_get_property (GObject      *object,
                         guint         prop_id,
                         GValue       *value,
                         GParamSpec   *pspec)
{
  GisDriver *driver = GIS_DRIVER (object);
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  switch (prop_id)
    {
    case PROP_LIVE_SESSION:
      g_value_set_boolean (value, priv->is_live_session);
      break;
    case PROP_MODE:
      g_value_set_enum (value, priv->mode);
      break;
    case PROP_USERNAME:
      g_value_set_string (value, priv->username);
      break;
    case PROP_SMALL_SCREEN:
      g_value_set_boolean (value, priv->small_screen);
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
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  switch (prop_id)
    {
    case PROP_MODE:
      priv->mode = g_value_get_enum (value);
      break;
    case PROP_USERNAME:
      g_free (priv->username);
      priv->username = g_value_dup_string (value);
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
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  G_APPLICATION_CLASS (gis_driver_parent_class)->activate (app);

  gtk_window_present (GTK_WINDOW (priv->main_window));
}

static gboolean
maximize (gpointer data)
{
  GtkWindow *window = data;

  gtk_window_maximize (window);
  gtk_window_present (window);

  return G_SOURCE_REMOVE;
}

static gboolean
unmaximize (gpointer data)
{
  GtkWindow *window = data;

  gtk_window_unmaximize (window);
  gtk_window_present (window);

  return G_SOURCE_REMOVE;
}

static void
update_screen_size (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  GdkWindow *window;
  GdkGeometry size_hints;
  GtkWidget *sw;

  if (!gtk_widget_get_realized (GTK_WIDGET (priv->main_window)))
    return;

  sw = gtk_bin_get_child (GTK_BIN (priv->main_window));
  window = gtk_widget_get_window (GTK_WIDGET (priv->main_window));

  if (priv->small_screen)
    {
      if (window)
        gdk_window_set_functions (window,
                                  GDK_FUNC_ALL | GDK_FUNC_MINIMIZE | GDK_FUNC_CLOSE);

      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);

      gtk_window_set_geometry_hints (priv->main_window, NULL, NULL, 0);
      gtk_window_set_resizable (priv->main_window, TRUE);
      gtk_window_set_position (priv->main_window, GTK_WIN_POS_NONE);

      g_idle_add (maximize, priv->main_window);
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

      size_hints.min_width = size_hints.max_width = 747;
      size_hints.min_height = size_hints.max_height = 539;
      size_hints.win_gravity = GDK_GRAVITY_CENTER;

      gtk_window_set_geometry_hints (priv->main_window,
                                     NULL,
                                     &size_hints,
                                     GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE | GDK_HINT_WIN_GRAVITY);
      gtk_window_set_resizable (priv->main_window, FALSE);
      gtk_window_set_position (priv->main_window, GTK_WIN_POS_CENTER_ALWAYS);

      g_idle_add (unmaximize, priv->main_window);
    }
}

static void
screen_size_changed (GdkScreen *screen, GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  gboolean small_screen;

  small_screen = screen_is_small (screen);

  if (priv->small_screen != small_screen)
    {
      priv->small_screen = small_screen;
      update_screen_size (driver);
      g_object_notify (G_OBJECT (driver), "small-screen");
    }
}

static void
window_realize_cb (GtkWidget *widget, gpointer user_data)
{
  update_screen_size (GIS_DRIVER (user_data));
}

static void
gis_driver_startup (GApplication *app)
{
  GisDriver *driver = GIS_DRIVER (app);
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);

  G_APPLICATION_CLASS (gis_driver_parent_class)->startup (app);

  priv->main_window = g_object_new (GTK_TYPE_APPLICATION_WINDOW,
                                    "application", app,
                                    "type", GTK_WINDOW_TOPLEVEL,
                                    "icon-name", "preferences-system",
                                    "deletable", FALSE,
                                    NULL);

  gtk_application_inhibit (GTK_APPLICATION (app), priv->main_window,
                           GTK_APPLICATION_INHIBIT_IDLE,
                           "Should not be idle on first boot.");

  g_signal_connect (priv->main_window,
                    "realize",
                    G_CALLBACK (window_realize_cb),
                    (gpointer)app);

  priv->assistant = g_object_new (GIS_TYPE_ASSISTANT, NULL);
  gtk_container_add (GTK_CONTAINER (priv->main_window), GTK_WIDGET (priv->assistant));

  gtk_widget_show (GTK_WIDGET (priv->assistant));

  priv->is_live_session = running_live_session ();
  g_object_notify_by_pspec (G_OBJECT (driver), obj_props[PROP_LIVE_SESSION]);

  gis_driver_set_user_language (driver, setlocale (LC_MESSAGES, NULL));

  prepare_main_window (driver);
  rebuild_pages (driver);
}

static void
gis_driver_init (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  GdkScreen *screen;

  screen = gdk_screen_get_default ();

  priv->small_screen = screen_is_small (screen);

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
  gobject_class->finalize = gis_driver_finalize;
  application_class->startup = gis_driver_startup;
  application_class->activate = gis_driver_activate;
  klass->locale_changed = gis_driver_real_locale_changed;

  signals[REBUILD_PAGES] =
    g_signal_new ("rebuild-pages",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GisDriverClass, rebuild_pages),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[LOCALE_CHANGED] =
    g_signal_new ("locale-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GisDriverClass, locale_changed),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  obj_props[PROP_LIVE_SESSION] =
    g_param_spec_boolean ("live-session", "", "",
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

  obj_props[PROP_SMALL_SCREEN] =
    g_param_spec_boolean ("small-screen", "", "",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

void
gis_driver_save_data (GisDriver *driver)
{
  GisDriverPrivate *priv = gis_driver_get_instance_private (driver);
  gis_assistant_save_data (priv->assistant);
}

GisDriver *
gis_driver_new (GisDriverMode mode)
{
  return g_object_new (GIS_TYPE_DRIVER,
                       "application-id", "org.gnome.InitialSetup",
                       "mode", mode,
                       NULL);
}

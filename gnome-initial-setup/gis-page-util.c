/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright © 2017–2018 Endless Mobile, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Mario Sanchez Prada <mario@endlessm.com>
 *     Will Thompson <wjt@endlessm.com>
 */

#define _GNU_SOURCE 1  /* for NL_LOCALE_NAME */

#include "config.h"

#include "gis-page-util.h"

#include <errno.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <langinfo.h>
#include <locale.h>
#include <polkit/polkit.h>
#include <sys/types.h>
#include <sys/xattr.h>

#define EOS_IMAGE_VERSION_XATTR "user.eos-image-version"
#define SERIAL_VERSION_FILE "/sys/devices/virtual/dmi/id/product_uuid"
#define DT_COMPATIBLE_FILE  "/proc/device-tree/compatible"
#define SYSROOT_MOUNT       "/sysroot"

static GtkBuilder *
get_modals_builder (void)
{
  GtkBuilder *builder = NULL;
  const gchar *resource_path = "/org/gnome/initial-setup/gis-page-util.ui";
  g_autoptr(GError) error = NULL;

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, resource_path, &error);

  if (error != NULL) {
    g_warning ("Error while loading %s: %s", resource_path, error->message);
    g_object_unref (builder);
    return NULL;
  }

  return builder;
}

static gboolean
get_serial_version (gchar **display_serial)
{
  GError *error = NULL;
  gchar *serial = NULL;
  gchar **split;
  GString *display;
  gchar *display_str;
  gint idx;

  g_file_get_contents (SERIAL_VERSION_FILE, &serial, NULL, &error);

  if (error) {
    g_warning ("Error when reading %s: %s", SERIAL_VERSION_FILE, error->message);
    g_error_free (error);
    return FALSE;
  }

  /* Drop hyphens */
  split = g_strsplit (serial, "-", -1);
  g_free (serial);
  serial = g_strstrip (g_strjoinv (NULL, split));
  g_strfreev (split);

  display = g_string_new (NULL);

  /* Each byte is encoded here as two characters; valid bytes are
   * followed by markers (same length), and we need to get the first 6
   * valid bytes only.
   * So, 6 * 4 = 24 below...
   */
  for (idx = 0; idx < MIN (strlen (serial), 24); idx++) {
    /* Discard markers */
    if (idx % 4 > 1)
      continue;

    g_string_append_c (display, serial[idx]);

    /* Space out valid bytes in the display version of the string */
    if (idx % 4 == 1)
      g_string_append_c (display, ' ');
  }

  g_free (serial);

  display_str = g_strstrip (g_string_free (display, FALSE));

  if (display_serial)
    *display_serial = display_str;
  else
    g_free (display_str);

  return TRUE;
}

gchar *
gis_page_util_get_image_version (const gchar *path,
                                 GError     **error)
{
  ssize_t attrsize;
  g_autofree gchar *value = NULL;

  g_return_val_if_fail (path != NULL, NULL);

  attrsize = getxattr (path, EOS_IMAGE_VERSION_XATTR, NULL, 0);
  if (attrsize >= 0)
    {
      value = g_malloc (attrsize + 1);
      value[attrsize] = 0;

      attrsize = getxattr (path, EOS_IMAGE_VERSION_XATTR, value,
                           attrsize);
    }

  if (attrsize >= 0)
    {
      return g_steal_pointer (&value);
    }
  else
    {
      int errsv = errno;
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                   "Error examining " EOS_IMAGE_VERSION_XATTR " on %s: %s",
                   path, g_strerror (errsv));
      return NULL;
    }
}

static gchar *
get_software_version (void)
{
  g_autoptr(GString) software_version = NULL;
  g_autofree gchar *name = NULL;
  g_autofree gchar *version = NULL;

  software_version = g_string_new ("");

  name = g_get_os_info (G_OS_INFO_KEY_NAME);
  version = g_get_os_info (G_OS_INFO_KEY_VERSION);

  if (name)
    g_string_append (software_version, name);
  if (name && version)
    g_string_append_c (software_version, ' ');
  if (version)
    g_string_append (software_version, version);

  return g_string_free (g_steal_pointer (&software_version), FALSE);
}

static gchar *
get_product_id (void)
{
  GError *error = NULL;
  gchar *compatible = NULL;

  g_file_get_contents (DT_COMPATIBLE_FILE, &compatible, NULL, &error);

  if (error) {
    g_error_free (error);
    return NULL;
  }

  return compatible;
}

static void
system_poweroff (gpointer data)
{
  GDBusConnection *bus;
  GError *error = NULL;
  GPermission *permission;

  permission = polkit_permission_new_sync ("org.freedesktop.login1.power-off", NULL, NULL, &error);
  if (error) {
    g_warning ("Failed getting permission to power off: %s", error->message);
    g_error_free (error);
    return;
  }

  if (!g_permission_get_allowed (permission)) {
    g_warning ("Not allowed to power off");
    g_object_unref (permission);
    return;
  }

  g_object_unref (permission);

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (error) {
    g_warning ("Failed to get system bus: %s", error->message);
    g_error_free (error);
    return;
  }

  g_dbus_connection_call (bus,
                          "org.freedesktop.login1",
                          "/org/freedesktop/login1",
                          "org.freedesktop.login1.Manager",
                          "PowerOff",
                          g_variant_new ("(b)", FALSE),
                          NULL, 0, G_MAXINT, NULL, NULL, NULL);

  g_object_unref (bus);
}

static void
system_testmode (GtkButton *button, gpointer data)
{
  GtkWindow *factory_dialog = GTK_WINDOW (data);
  GError *error = NULL;

  /* Test mode can only be initialized once */
  gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);

  if (!gis_pkexec ("eos-test-mode", NULL, NULL, &error)) {
    GtkWidget *dialog;

    g_warning ("%s", error->message);
    dialog = gtk_message_dialog_new (factory_dialog,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     "%s",
                                     error->message);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    g_error_free (error);
  }

  gtk_window_close (factory_dialog);
}

void
gis_page_util_show_factory_dialog (GisPage *page)
{
  g_autoptr(GtkBuilder) builder = NULL;
  GtkButton *poweroff_button;
  GtkButton *testmode_button;
  GtkDialog *factory_dialog;
  GtkLabel *image_version_label;
  GtkLabel *serial_label;
  GtkLabel *product_id_label;
  GtkLabel *version_label;
  gboolean have_serial;
  g_autofree gchar *display_serial = NULL;
  g_autofree gchar *version = NULL;
  g_autofree gchar *image_version = NULL;
  g_autofree gchar *image_version_text = NULL;
  g_autofree gchar *product_id_text = NULL;

  builder = get_modals_builder ();
  if (builder == NULL) {
    g_warning ("Can't get private builder object for factory mode!");
    return;
  }

  factory_dialog = (GtkDialog *)gtk_builder_get_object (builder, "factory-dialog");
  version_label = (GtkLabel *)gtk_builder_get_object (builder, "software-version");
  product_id_label = (GtkLabel *)gtk_builder_get_object (builder, "product-id");
  image_version_label = (GtkLabel *)gtk_builder_get_object (builder, "image-version");
  serial_label = (GtkLabel *)gtk_builder_get_object (builder, "serial-text");
  poweroff_button = (GtkButton *)gtk_builder_get_object (builder, "poweroff-button");
  testmode_button = (GtkButton *)gtk_builder_get_object (builder, "testmode-button");

  version = get_software_version ();
  gtk_label_set_text (version_label, version);

  product_id_text = get_product_id ();
  if (product_id_text) {
    gtk_label_set_text (product_id_label, product_id_text);
    gtk_widget_set_visible (GTK_WIDGET (product_id_label), TRUE);
  }

  have_serial = get_serial_version (&display_serial);

  if (have_serial) {
    gtk_label_set_text (serial_label, display_serial);
  } else {
    gtk_widget_set_visible (GTK_WIDGET (serial_label), FALSE);
  }

  image_version = gis_page_util_get_image_version (SYSROOT_MOUNT, NULL);
  if (!image_version)
    image_version = g_strdup (_("Unknown"));

  image_version_text = g_strdup_printf (_("Image: %s"), image_version);
  gtk_label_set_text (image_version_label, image_version_text);

  g_signal_connect_swapped (poweroff_button, "clicked",
                            G_CALLBACK (system_poweroff), NULL);
  g_signal_connect (testmode_button, "clicked",
                    G_CALLBACK (system_testmode), factory_dialog);

  gtk_window_set_transient_for (GTK_WINDOW (factory_dialog),
                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page))));
  gtk_window_set_modal (GTK_WINDOW (factory_dialog), TRUE);
  gtk_window_present (GTK_WINDOW (factory_dialog));
  g_signal_connect (factory_dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);
}

static gboolean
run_dialog_cb (gpointer user_data)
{
  GtkDialog *dialog = GTK_DIALOG (user_data);
  gtk_dialog_run (dialog);
  return G_SOURCE_REMOVE;
}

static void
on_reformatter_exited (GTask  *task,
                       GError *error)
{
  GisPage *page = GIS_PAGE (g_task_get_source_object (task));

  gis_driver_show_window (page->driver);

  if (error == NULL)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  GtkWindow *toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (page)));
  GtkWidget *message_dialog;

  g_critical ("Error running the reformatter: %s", error->message);

  message_dialog = gtk_message_dialog_new (toplevel,
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           /* Translators: this is shown when launching the
                                            * reformatter (an external program) fails. The
                                            * placeholder is an error message describing what went
                                            * wrong.
                                            */
                                           _("Error running the reformatter: %s"), error->message);
  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, run_dialog_cb,
                   message_dialog, (GDestroyNotify) gtk_widget_destroy);

  g_task_return_error (task, g_steal_pointer (&error));
}

static void
reformatter_exited_cb (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr(GTask) task = G_TASK (user_data);
  g_autoptr(GError) error = NULL;

  g_subprocess_wait_check_finish (G_SUBPROCESS (source), result, &error);
  on_reformatter_exited (task, g_steal_pointer (&error));
}

#define EOS_INSTALLER_DESKTOP_ID "com.endlessm.Installer.desktop"

/**
 * gis_page_util_run_reformatter:
 *
 * Launches the reformatter, and arranges for the assistant to be hidden for
 * the duration of its runtime. @callback will be called when the reformatter
 * exits.
 *
 * There is no corresponding _finish() function because (at present) neither
 * caller actually cares about the result.
 */
void
gis_page_util_run_reformatter (GisPage            *page,
                               GAsyncReadyCallback callback,
                               gpointer            user_data)
{
  g_autoptr(GTask) task = g_task_new (page, NULL, callback, user_data);
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  g_autoptr(GSubprocess) subprocess = NULL;
  const gchar *locale = nl_langinfo (NL_LOCALE_NAME (LC_MESSAGES));
  g_autoptr(GDesktopAppInfo) installer = NULL;
  const gchar *executable = NULL;
  g_autoptr(GError) error = NULL;

  installer = g_desktop_app_info_new (EOS_INSTALLER_DESKTOP_ID);
  if (installer == NULL)
    {
      g_set_error (&error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   EOS_INSTALLER_DESKTOP_ID " not found");
      on_reformatter_exited (task, g_steal_pointer (&error));
    }

  /* We can't just activate the GAppInfo because that API gives no way to
   * track success or failure of the launched application, and it has proven
   * helpful in practice to be able to show an error message on failure.
   */
  executable = g_app_info_get_executable (G_APP_INFO (installer));
  if (executable == NULL)
    {
      g_set_error (&error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "No executable path for " EOS_INSTALLER_DESKTOP_ID);
      on_reformatter_exited (task, g_steal_pointer (&error));
    }

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_NONE);
  g_subprocess_launcher_setenv (launcher, "LANG", locale, TRUE);
  subprocess = g_subprocess_launcher_spawn (launcher, &error, executable, NULL);

  if (error)
    {
      on_reformatter_exited (task, g_steal_pointer (&error));
      return;
    }

  gis_driver_hide_window (page->driver);
  g_subprocess_wait_check_async (subprocess, NULL, reformatter_exited_cb,
                                 g_steal_pointer (&task));
}

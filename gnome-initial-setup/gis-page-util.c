/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright © 2017–2024 Endless Mobile, Inc.
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

#include "gis-page.h"
#include "gis-page-util.h"

#include <gio/gdesktopappinfo.h>
#include <langinfo.h>
#include <locale.h>

#define EOS_IMAGE_VERSION_XATTR "xattr::eos-image-version"

static char *
get_image_version (const char *path,
                   GError **error)
{
  g_autoptr(GFile) file = g_file_new_for_path (path);
  g_autoptr(GFileInfo) info = NULL;

  info = g_file_query_info (file,
                            EOS_IMAGE_VERSION_XATTR,
                            G_FILE_QUERY_INFO_NONE,
                            NULL /* cancellable */,
                            error);

  if (info == NULL)
    return NULL;

  return g_strdup (g_file_info_get_attribute_string (info, EOS_IMAGE_VERSION_XATTR));
}

gchar *
gis_page_util_get_image_version (void)
{
  g_autoptr(GError) error_sysroot = NULL;
  g_autoptr(GError) error_root = NULL;
  char *image_version = get_image_version ("/sysroot", &error_sysroot);

  if (image_version == NULL)
    image_version = get_image_version ("/", &error_root);

  if (image_version == NULL)
    {
      if (error_sysroot != NULL)
        g_warning ("%s", error_sysroot->message);
      else if (error_root != NULL)
        g_warning ("%s", error_root->message);
      else
        g_debug ("Neither /sysroot nor / had " EOS_IMAGE_VERSION_XATTR);
    }

  return image_version;
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

  GtkWidget *message_dialog;

  g_critical ("Error running the reformatter: %s", error->message);

  message_dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (page))),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           /* Translators: this is shown when launching the
                                            * reformatter (an external program) fails. The
                                            * placeholder is an error message describing what went
                                            * wrong.
                                            */
                                           _("Error running the reformatter: %s"), error->message);

  g_signal_connect (message_dialog, "response",
                    G_CALLBACK (gtk_window_destroy),
                    NULL);
  gtk_window_present (GTK_WINDOW (message_dialog));

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
      return;
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
      return;
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

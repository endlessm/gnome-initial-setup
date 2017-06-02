/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Endless Mobile, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 * Written by:
 *     Mario Sanchez Prada <mario@endlessm.com>
 */

#include "config.h"

#include "gis-page-util.h"

#include <attr/xattr.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <polkit/polkit.h>
#include <sys/types.h>
#include <zint.h>

#define EOS_IMAGE_VERSION_XATTR "user.eos-image-version"
#define OSRELEASE_FILE      "/etc/os-release"
#define SERIAL_VERSION_FILE "/sys/devices/virtual/dmi/id/product_uuid"
#define DT_COMPATIBLE_FILE  "/proc/device-tree/compatible"
#define SD_CARD_MOUNT       LOCALSTATEDIR "/endless-extra"

static GtkBuilder *
get_factory_mode_builder (void)
{
  static GtkBuilder *builder = NULL;
  const gchar *resource_path = "/org/gnome/initial-setup/gis-page-util.ui";
  g_autoptr(GError) error = NULL;

  if (builder != NULL)
    return builder;

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, resource_path, &error);

  if (error != NULL) {
    g_warning ("Error while loading %s: %s", resource_path, error->message);
    g_object_unref (builder);
    return NULL;
  }

  return builder;
}

static gchar *
create_serial_barcode (const gchar *serial)
{
  gchar *savefile;
  const gchar *cache_dir;
  struct zint_symbol *barcode;

  cache_dir = g_get_user_cache_dir ();

  /* Create the directory if it's missing */
  g_mkdir_with_parents (cache_dir, 0755);

  savefile = g_build_filename (cache_dir, "product_serial.png", NULL);

  barcode = ZBarcode_Create();
  strncpy ((char *) barcode->outfile, savefile, 4096);
  if (ZBarcode_Encode_and_Print (barcode, (guchar *) serial, 0, 0)) {
    g_warning ("Error while generating barcode: %s", barcode->errtxt);
  }
  ZBarcode_Delete (barcode);

  return savefile;
}

static gboolean
get_serial_version (gchar **display_serial,
                    gchar **barcode_serial)
{
  GError *error = NULL;
  gchar *serial = NULL;
  gchar **split;
  GString *display, *barcode;
  gchar *display_str, *barcode_str;
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
  barcode = g_string_new (NULL);

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
    g_string_append_c (barcode, serial[idx]);

    /* Space out valid bytes in the display version of the string */
    if (idx % 4 == 1)
      g_string_append_c (display, ' ');
  }

  g_free (serial);

  display_str = g_strstrip (g_string_free (display, FALSE));
  barcode_str = g_strstrip (g_string_free (barcode, FALSE));

  /* ZBarcode_Encode_and_Print() needs UTF-8 */
  if (!g_utf8_validate (barcode_str, -1, NULL)) {
    g_warning ("Error when reading %s: not a valid UTF-8 string", SERIAL_VERSION_FILE);
    g_free (barcode_str);
    g_free (display_str);
    return FALSE;
  }

  if (barcode_serial)
    *barcode_serial = barcode_str;
  else
    g_free (barcode_str);

  if (display_serial)
    *display_serial = display_str;
  else
    g_free (display_str);

  return TRUE;
}

static gboolean
get_have_sdcard (void)
{
  GDir *dir;
  gboolean has_bundles;

  dir = g_dir_open (SD_CARD_MOUNT, 0, NULL);
  if (!dir)
    return FALSE;

  has_bundles = (g_dir_read_name (dir) != NULL);
  g_dir_close (dir);

  return has_bundles;
}

static gchar *
get_sdcard_version (void)
{
  ssize_t attrsize;
  gchar *value;

  attrsize = getxattr (SD_CARD_MOUNT, EOS_IMAGE_VERSION_XATTR, NULL, 0);
  if (attrsize < 0) {
    g_warning ("Error examining SD card xattr: %s", g_strerror (errno));
    return NULL;
  }

  value = g_malloc (attrsize + 1);
  value[attrsize] = 0;

  attrsize = getxattr (SD_CARD_MOUNT, EOS_IMAGE_VERSION_XATTR, value,
                       attrsize);
  if (attrsize < 0) {
    g_warning ("Error reading SD card xattr: %s", g_strerror (errno));
    g_free (value);
    return NULL;
  }

  return value;
}

static gchar *
get_software_version (void)
{
  GDataInputStream *datastream;
  GError *error = NULL;
  GFile *osrelease_file = NULL;
  GFileInputStream *filestream;
  GString *software_version;
  gchar *line;
  gchar *name = NULL;
  gchar *version = NULL;
  gchar *version_string = NULL;

  osrelease_file = g_file_new_for_path (OSRELEASE_FILE);
  filestream = g_file_read (osrelease_file, NULL, &error);
  if (error) {
    goto bailout;
  }

  datastream = g_data_input_stream_new (G_INPUT_STREAM (filestream));

  while ((!name || !version) &&
         (line = g_data_input_stream_read_line (datastream, NULL, NULL, &error))) {
    if (g_str_has_prefix (line, "NAME=")) {
      name = line;
    } else if (g_str_has_prefix (line, "VERSION=")) {
      version = line;
    } else {
      g_free (line);
    }
  }

  if (error) {
    goto bailout;
  }

  software_version = g_string_new ("");

  if (name) {
    g_string_append (software_version, name + strlen ("NAME=\""));
    g_string_erase (software_version, software_version->len - 1, 1);
  }

  if (version) {
    if (name) {
      g_string_append_c (software_version, ' ');
    }
    g_string_append (software_version, version + strlen ("VERSION=\""));
    g_string_erase (software_version, software_version->len - 1, 1);
  }

  version_string = g_string_free (software_version, FALSE);

 bailout:
  g_free (name);
  g_free (version);

  if (error) {
    g_warning ("Error reading " OSRELEASE_FILE ": %s", error->message);
    g_error_free (error);
  }

  g_clear_object (&datastream);
  g_clear_object (&filestream);
  g_clear_object (&osrelease_file);

  if (version_string) {
    return version_string;
  } else {
    return g_strdup ("");
  }
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

  if (!gis_pkexec (LIBEXECDIR "/eos-test-mode", NULL, NULL, &error)) {
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
  GisDriver *driver = GIS_PAGE (page)->driver;
  GtkBuilder *builder = NULL;
  GtkButton *poweroff_button;
  GtkButton *testmode_button;
  GtkDialog *factory_dialog;
  GtkImage *serial_image;
  GtkLabel *sdcard_label;
  GtkLabel *serial_label;
  GtkLabel *product_id_label;
  GtkLabel *version_label;
  gboolean have_serial;
  gchar *barcode;
  gchar *barcode_serial, *display_serial;
  gchar *version;
  gchar *sd_version = NULL;
  gchar *sd_text;
  gchar *product_id_text;

  builder = get_factory_mode_builder ();
  if (builder == NULL) {
    g_warning ("Can't get private builder object for factory mode!");
    return;
  }

  factory_dialog = (GtkDialog *)gtk_builder_get_object (builder, "factory-dialog");
  version_label = (GtkLabel *)gtk_builder_get_object (builder, "software-version");
  product_id_label = (GtkLabel *)gtk_builder_get_object (builder, "product-id");
  sdcard_label = (GtkLabel *)gtk_builder_get_object (builder, "sd-card");
  serial_label = (GtkLabel *)gtk_builder_get_object (builder, "serial-text");
  serial_image = (GtkImage *)gtk_builder_get_object (builder, "serial-barcode");
  poweroff_button = (GtkButton *)gtk_builder_get_object (builder, "poweroff-button");
  testmode_button = (GtkButton *)gtk_builder_get_object (builder, "testmode-button");

  version = get_software_version ();
  gtk_label_set_text (version_label, version);

  product_id_text = get_product_id ();
  if (product_id_text) {
    gtk_label_set_text (product_id_label, product_id_text);
    gtk_widget_set_visible (GTK_WIDGET (product_id_label), TRUE);
    g_free (product_id_text);
  }

  have_serial = get_serial_version (&display_serial, &barcode_serial);

  if (have_serial) {
    gtk_label_set_text (serial_label, display_serial);

    barcode = create_serial_barcode (barcode_serial);
    gtk_image_set_from_file (serial_image, barcode);
  } else {
    gtk_widget_set_visible (GTK_WIDGET (serial_label), FALSE);
    gtk_widget_set_visible (GTK_WIDGET (serial_image), FALSE);
  }

  if (get_have_sdcard ())
    sd_version = get_sdcard_version ();

  if (!sd_version)
    sd_version = g_strdup (_("Disabled"));

  sd_text = g_strdup_printf (_("SD Card: %s"), sd_version);
  gtk_label_set_text (sdcard_label, sd_text);
  g_free (sd_version);
  g_free (sd_text);

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

  if (have_serial) {
    g_remove (barcode);
    g_free (barcode);
    g_free (barcode_serial);
    g_free (display_serial);
  }

  g_free (version);
}

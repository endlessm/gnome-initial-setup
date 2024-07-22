/*
 * gis-factory-dialog.c
 *
 * Copyright 2017–2024 Endless OS Foundation LLC
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
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "gis-factory-dialog.h"

#include <polkit/polkit.h>

#include "gis-pkexec.h"
#include "gis-page-util.h"

struct _GisFactoryDialog
{
  GtkWindow parent_instance;

  GtkLabel *software_version_label;
  GtkLabel *image_version_label;
};

G_DEFINE_FINAL_TYPE (GisFactoryDialog, gis_factory_dialog, GTK_TYPE_WINDOW)

enum {
  PROP_SOFTWARE_VERSION = 1,
  PROP_IMAGE_VERSION,
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = { NULL, };

static gboolean
system_poweroff (GError **error)
{
  g_autoptr(GPermission) permission = NULL;
  g_autoptr(GDBusConnection) bus = NULL;

  permission = polkit_permission_new_sync ("org.freedesktop.login1.power-off", NULL, NULL, error);
  if (permission == NULL)
    {
      g_prefix_error (error, "Failed getting permission to power off: ");
      return FALSE;
    }

  if (!g_permission_get_allowed (permission)) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED, "Not allowed to power off");
    return FALSE;
  }

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, error);
  if (bus == NULL) {
    g_prefix_error (error, "Failed to get system bus: ");
    return FALSE;
  }

  g_dbus_connection_call (bus,
                          "org.freedesktop.login1",
                          "/org/freedesktop/login1",
                          "org.freedesktop.login1.Manager",
                          "PowerOff",
                          g_variant_new ("(b)", FALSE),
                          NULL, 0, G_MAXINT, NULL, NULL, NULL);

  return TRUE;
}

static void
show_error (GisFactoryDialog *self,
            const GError     *error)
{
  g_autoptr(GtkAlertDialog) dialog = NULL;

  dialog = gtk_alert_dialog_new (_("Failed to Enter Test Mode"));
  gtk_alert_dialog_set_detail (dialog, error->message);
  gtk_alert_dialog_show (dialog, GTK_WINDOW (self));
}

static void
poweroff_clicked_cb (GisFactoryDialog *self,
                     GtkButton        *button)
{
  g_autoptr(GError) error = NULL;
  if (!system_poweroff (&error))
    g_warning ("Failed to power off: %s", error->message);
}

static void
testmode_clicked_cb (GisFactoryDialog *self,
                     GtkButton        *button)
{
  g_autoptr(GError) error = NULL;

  /* Test mode can only be initialized once */
  gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);

  if (gis_pkexec ("eos-test-mode", NULL, NULL, &error))
    gtk_window_close (GTK_WINDOW (self));
  else
    show_error (self, error);
}

GisFactoryDialog *
gis_factory_dialog_new (const char *software_version,
                        const char *image_version)

{
  return g_object_new (GIS_TYPE_FACTORY_DIALOG,
                       "software-version", software_version,
                       "image-version", image_version,
                       NULL);
}

static void
gis_factory_dialog_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GisFactoryDialog *self = GIS_FACTORY_DIALOG (object);

  switch (prop_id)
    {
    case PROP_SOFTWARE_VERSION:
      gtk_label_set_text (self->software_version_label, g_value_get_string (value));
      break;

    case PROP_IMAGE_VERSION:
      gtk_label_set_text (self->image_version_label, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gis_factory_dialog_class_init (GisFactoryDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/initial-setup/gis-factory-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, GisFactoryDialog, software_version_label);
  gtk_widget_class_bind_template_child (widget_class, GisFactoryDialog, image_version_label);

  gtk_widget_class_bind_template_callback (widget_class, poweroff_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, testmode_clicked_cb);

  object_class->set_property = gis_factory_dialog_set_property;

  properties [PROP_SOFTWARE_VERSION] =
    g_param_spec_string ("software-version", NULL,
                         "The OS version, i.e. PRETTY_NAME from os-release",
                         "",
                         (G_PARAM_WRITABLE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_IMAGE_VERSION] =
    g_param_spec_string ("image-version", NULL,
                         "eos-image-version xattr from /sysroot",
                         "",
                         (G_PARAM_WRITABLE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
}

static void
gis_factory_dialog_init (GisFactoryDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}


void
gis_factory_dialog_show_for_widget (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

  GisFactoryDialog *factory_dialog = NULL;
  g_autofree gchar *software_version = NULL;
  g_autofree gchar *image_version = NULL;

  software_version = g_get_os_info (G_OS_INFO_KEY_PRETTY_NAME);

  image_version = gis_page_util_get_image_version ();
  if (!image_version)
    image_version = g_strdup (_("Unknown"));

  factory_dialog = gis_factory_dialog_new (software_version, image_version);

  gtk_window_set_transient_for (GTK_WINDOW (factory_dialog),
                                GTK_WINDOW (gtk_widget_get_root (widget)));
  gtk_window_set_modal (GTK_WINDOW (factory_dialog), TRUE);
  gtk_window_present (GTK_WINDOW (factory_dialog));
}

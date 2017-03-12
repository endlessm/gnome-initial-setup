/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2017 Endless Mobile, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Mario Sanchez Prada <mario@endlessm.com>
 */

/* Branding Welcome page {{{1 */

#define PAGE_ID "branding-welcome"

#include "config.h"

#include "gis-branding-welcome-page.h"

#include "branding-welcome-resources.h"
#include "gis-page-util.h"

struct _GisBrandingWelcomePagePrivate {
  gchar *title;
  gchar *description;
  gchar *logo_path;

  GtkAccelGroup *accel_group;
};
typedef struct _GisBrandingWelcomePagePrivate GisBrandingWelcomePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisBrandingWelcomePage, gis_branding_welcome_page, GIS_TYPE_PAGE);

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

#define VENDOR_BRANDING_WELCOME_GROUP "Welcome"
#define VENDOR_BRANDING_WELCOME_TITLE_KEY "title"
#define VENDOR_BRANDING_WELCOME_DESC_KEY "description"
#define VENDOR_BRANDING_WELCOME_LOGO_KEY "logo"

static void
load_css_overrides (GisBrandingWelcomePage *page)
{
  g_autoptr(GtkCssProvider) provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/initial-setup/branding-welcome-page.css");
  gtk_style_context_add_provider_for_screen (gtk_widget_get_screen (GTK_WIDGET (page)),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
read_config_file (GisBrandingWelcomePage *page)
{
  GisBrandingWelcomePagePrivate *priv = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;

  /* VENDOR_CONF_FILE points to a keyfile containing vendor customization
   * options. This code will look for options under the "Welcome" group, and
   * supports the following keys:
   *   - title (required): short string to show as title.
   *   - description (optional): string containing long text, likely to be wrapped.
   *   - logo (optional): absolute path to the file with a logo for the brand.
   *
   * For example, this is how this file would look on a vendor image defining a title,
   * a description an a logo:
   *
   *   [Welcome]
   *   title=A title to be shown at the top
   *   description=A long description that will be shown at the bottom of this
   *     page, right below the branded logo (if any), explaining what the
   *     branded edition is about.
   *   logo=/path/to/the/image/with/the/logo.png
   */
  keyfile = g_key_file_new ();
  if (!g_key_file_load_from_file (keyfile, VENDOR_CONF_FILE, G_KEY_FILE_NONE, &error)) {
    if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
      g_warning ("Could not read file %s: %s", VENDOR_CONF_FILE, error->message);

    return;
  }

  priv = gis_branding_welcome_page_get_instance_private (page);

  priv->title = g_key_file_get_string (keyfile, VENDOR_BRANDING_WELCOME_GROUP,
                                       VENDOR_BRANDING_WELCOME_TITLE_KEY, &error);
  if (priv->title == NULL) {
    g_warning ("Could not read title for 'Welcome' branding page from %s: %s",
               VENDOR_CONF_FILE, error->message);
    return;
  }

  priv->description = g_key_file_get_string (keyfile, VENDOR_BRANDING_WELCOME_GROUP,
                                             VENDOR_BRANDING_WELCOME_DESC_KEY, &error);
  if (priv->description == NULL) {
    g_debug ("Could not read description for 'Welcome' branding page from %s: %s",
             VENDOR_CONF_FILE, error->message);
    g_clear_error (&error);
  }

  priv->logo_path = g_key_file_get_string (keyfile, VENDOR_BRANDING_WELCOME_GROUP,
                                           VENDOR_BRANDING_WELCOME_LOGO_KEY, &error);
  if (priv->logo_path == NULL) {
    g_debug ("Could not read logo path for 'Welcome' branding page from %s: %s",
             VENDOR_CONF_FILE, error->message);
    g_clear_error (&error);
  }
}

static void
update_branding_specific_info (GisBrandingWelcomePage *page)
{
  GisBrandingWelcomePagePrivate *priv = gis_branding_welcome_page_get_instance_private (page);
  GtkWidget *opt_widget = NULL;

  read_config_file (page);

  if (priv->title == NULL) {
    g_debug ("No branding configuration found");
    return;
  }

  gtk_label_set_label (GTK_LABEL (WID("branding-title")), priv->title);

  if (priv->description != NULL) {
    opt_widget = WID("branding-text");
    gtk_label_set_label (GTK_LABEL (opt_widget), priv->description);
    gtk_widget_show (opt_widget);
  }

  if (priv->logo_path != NULL) {
    opt_widget = WID("branding-logo");
    gtk_image_set_from_file (GTK_IMAGE (opt_widget), priv->logo_path);
    gtk_widget_show (opt_widget);
  }
}

static GtkAccelGroup *
gis_branding_welcome_page_get_accel_group (GisPage *page)
{
  GisBrandingWelcomePage *self = GIS_BRANDING_WELCOME_PAGE (page);
  GisBrandingWelcomePagePrivate *priv = gis_branding_welcome_page_get_instance_private (self);

  return priv->accel_group;
}

static void
gis_branding_welcome_page_constructed (GObject *object)
{
  GisBrandingWelcomePage *page = GIS_BRANDING_WELCOME_PAGE (object);
  GisBrandingWelcomePagePrivate *priv = gis_branding_welcome_page_get_instance_private (page);
  GClosure *closure;

  G_OBJECT_CLASS (gis_branding_welcome_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("branding-welcome-page"));

  update_branding_specific_info (page);
  load_css_overrides (page);

  /* Use ctrl+f to show factory dialog */
  priv->accel_group = gtk_accel_group_new ();
  closure = g_cclosure_new_swap (G_CALLBACK (gis_page_util_show_factory_dialog), page, NULL);
  gtk_accel_group_connect (priv->accel_group, GDK_KEY_f, GDK_CONTROL_MASK, 0, closure);
  g_closure_unref (closure);

  gis_page_set_complete (GIS_PAGE (page), TRUE);
  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_branding_welcome_page_finalize (GObject *object)
{
  GisBrandingWelcomePage *page = GIS_BRANDING_WELCOME_PAGE (object);
  GisBrandingWelcomePagePrivate *priv = gis_branding_welcome_page_get_instance_private (page);

  g_free (priv->title);
  g_free (priv->description);
  g_free (priv->logo_path);
  g_clear_object (&priv->accel_group);

  G_OBJECT_CLASS (gis_branding_welcome_page_parent_class)->finalize (object);
}

static void
gis_branding_welcome_page_class_init (GisBrandingWelcomePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->get_accel_group = gis_branding_welcome_page_get_accel_group;
  object_class->constructed = gis_branding_welcome_page_constructed;
  object_class->finalize = gis_branding_welcome_page_finalize;
}

static void
gis_branding_welcome_page_init (GisBrandingWelcomePage *page)
{
  g_resources_register (branding_welcome_get_resource ());
}

void
gis_prepare_branding_welcome_page (GisDriver *driver)
{
  GisPage *page = NULL;
  GisBrandingWelcomePagePrivate *priv = NULL;

  page = GIS_PAGE (g_object_new (GIS_TYPE_BRANDING_WELCOME_PAGE, "driver", driver, NULL));
  priv = gis_branding_welcome_page_get_instance_private (GIS_BRANDING_WELCOME_PAGE (page));

  /* We don't actually add the page unless we have the only required field */
  if (priv->title == NULL) {
    g_object_ref_sink (page);
    g_object_unref (page);
    return;
  }

  gis_driver_add_page (driver, page);
}

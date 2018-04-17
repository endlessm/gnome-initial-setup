/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (C) 2018 Endless Mobile, Inc.
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
 *     Travis Reitter <travis.reitter@endlessm.com>
 */

/* Site page {{{1 */

#define PAGE_ID GIS_SITE_PAGE_ID

#include "config.h"
#include "gis-site.h"
#include "gis-site-page.h"
#include "gis-site-search-entry.h"
#include "site-resources.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <json-glib/json-glib.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define LOCATION_FILENAME_TEMPL "gnome-initial-setup-location-XXXXXX.conf"

struct _GisSitePagePrivate
{
  GtkWidget *search_entry;
  GtkWidget *manual_check;
  GtkWidget *id_entry;
  GtkWidget *facility_entry;
  GtkWidget *street_entry;
  GtkWidget *locality_entry;
  GtkWidget *region_entry;
  GtkWidget *country_entry;
};
typedef struct _GisSitePagePrivate GisSitePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisSitePage, gis_site_page, GIS_TYPE_PAGE)

static void
set_site (GisSitePage  *page,
          GisSite      *site)
{
  GisSitePagePrivate *priv = gis_site_page_get_instance_private (page);
  const gchar *id = NULL;
  const gchar *facility = NULL;
  const gchar *street = NULL;
  const gchar *locality = NULL;
  const gchar *region = NULL;
  const gchar *country = NULL;

  if (site)
    {
      id = gis_site_get_id (site);
      facility = gis_site_get_facility (site);
      street = gis_site_get_street (site);
      locality = gis_site_get_locality (site);
      region = gis_site_get_region (site);
      country = gis_site_get_country (site);
    }

  gtk_entry_set_text (GTK_ENTRY (priv->id_entry), id ? id : "");
  gtk_entry_set_text (GTK_ENTRY (priv->facility_entry),
                      facility ? facility : "");
  gtk_entry_set_text (GTK_ENTRY (priv->street_entry), street ? street : "");
  gtk_entry_set_text (GTK_ENTRY (priv->locality_entry),
                      locality ? locality : "");
  gtk_entry_set_text (GTK_ENTRY (priv->region_entry), region ? region : "");
  gtk_entry_set_text (GTK_ENTRY (priv->country_entry), country ? country : "");
}

static gboolean
page_validate (GisSitePage *page)
{
  GisSitePagePrivate *priv = gis_site_page_get_instance_private (page);

  if (gtk_entry_get_text_length (GTK_ENTRY (priv->id_entry)) ||
      gtk_entry_get_text_length (GTK_ENTRY (priv->facility_entry)) ||
      gtk_entry_get_text_length (GTK_ENTRY (priv->street_entry)) ||
      gtk_entry_get_text_length (GTK_ENTRY (priv->locality_entry)) ||
      gtk_entry_get_text_length (GTK_ENTRY (priv->region_entry)) ||
      gtk_entry_get_text_length (GTK_ENTRY (priv->country_entry)))
    {
      return TRUE;
    }

  return FALSE;
}

static void
update_page_validation (GObject *object, GParamSpec *param, GisSitePage *page)
{
  gis_page_set_complete (GIS_PAGE (page), page_validate (page));
}

static GisSite*
get_site_from_widgets (GisSitePage *page)
{
  GisSitePagePrivate *priv = gis_site_page_get_instance_private (page);
  const gchar *id = gtk_entry_get_text (GTK_ENTRY (priv->id_entry));
  const gchar *facility = gtk_entry_get_text (GTK_ENTRY (priv->facility_entry));
  const gchar *street = gtk_entry_get_text (GTK_ENTRY (priv->street_entry));
  const gchar *locality = gtk_entry_get_text (GTK_ENTRY (priv->locality_entry));
  const gchar *region = gtk_entry_get_text (GTK_ENTRY (priv->region_entry));
  const gchar *country = gtk_entry_get_text (GTK_ENTRY (priv->country_entry));

  return gis_site_new (id, facility, street, locality, region, country);
}

static void
ini_string_append_safe (GString     *string,
                        const gchar *property,
                        const gchar *value)
{
  g_string_append_printf (string, "%s = %s\n", property, value ? value : "");
}

static gchar*
ini_file_content_from_site (GisSite *site)
{
  GString *builder = g_string_new ("[Label]\n");

  /* Note the field names aren't 1:1 between GisSite and the file format */
  ini_string_append_safe (builder, "id", gis_site_get_id (site));
  ini_string_append_safe (builder, "facility", gis_site_get_facility (site));
  ini_string_append_safe (builder, "street", gis_site_get_street (site));
  ini_string_append_safe (builder, "city", gis_site_get_locality (site));
  ini_string_append_safe (builder, "state", gis_site_get_region (site));
  ini_string_append_safe (builder, "country", gis_site_get_country (site));

  return g_string_free (builder, FALSE);
}

static gboolean
gis_site_page_save_data (GisPage  *gis_page,
                         GError  **error)
{
  GisSitePage *page = GIS_SITE_PAGE (gis_page);
  g_autofree gchar *location_file_content = NULL;
  g_autofree gchar *filename_created = NULL;
  g_autoptr(GisSite) site = NULL;
  int fd;

  if (gis_page->driver == NULL)
    return TRUE;

  site = get_site_from_widgets (page);
  location_file_content = ini_file_content_from_site (site);

  fd = g_file_open_tmp (LOCATION_FILENAME_TEMPL, &filename_created, error);
  if (fd < 0)
    return FALSE;

  /* immediately close the new file so we don't have multiple FDs open for
   * the file at the same time
   */
  if (!g_close (fd, error))
    return FALSE;

  if (!g_file_set_contents (filename_created, location_file_content, -1,
                            error))
    return FALSE;

  if (!gis_pkexec (LIBEXECDIR "/eos-write-location", filename_created, NULL,
                   error))
    return FALSE;

  return TRUE;
}

static void
entry_site_changed (GObject *object, GParamSpec *param, GisSitePage *page)
{
  GisSiteSearchEntry *entry = GIS_SITE_SEARCH_ENTRY (object);
  GisSite *site;

  site = gis_site_search_entry_get_site (entry);
  set_site (page, site);
}

static void
manual_check_toggled (GtkToggleButton *manual_check, GisSitePage *page)
{
  GisSitePagePrivate *priv = gis_site_page_get_instance_private (page);
  gboolean active = gtk_toggle_button_get_active (manual_check);

  /* clear the search entry and field GtkEntrys */
  gtk_entry_set_text (GTK_ENTRY (priv->search_entry), "");

  /* make the search active and all the "field" GtkEntrys inactive or vice versa
   */
  gtk_widget_set_can_focus (priv->search_entry, !active);
  gtk_widget_set_can_focus (priv->id_entry, active);
  gtk_widget_set_can_focus (priv->facility_entry, active);
  gtk_widget_set_can_focus (priv->street_entry, active);
  gtk_widget_set_can_focus (priv->locality_entry, active);
  gtk_widget_set_can_focus (priv->region_entry, active);
  gtk_widget_set_can_focus (priv->country_entry, active);

  gtk_widget_set_sensitive (priv->search_entry, !active);
  gtk_widget_set_sensitive (priv->id_entry, active);
  gtk_widget_set_sensitive (priv->facility_entry, active);
  gtk_widget_set_sensitive (priv->street_entry, active);
  gtk_widget_set_sensitive (priv->locality_entry, active);
  gtk_widget_set_sensitive (priv->region_entry, active);
  gtk_widget_set_sensitive (priv->country_entry, active);

  /* focus the first sensible widget for entry */
  if (active)
    gtk_widget_grab_focus (priv->id_entry);
  else
    gtk_widget_grab_focus (priv->search_entry);
}

static void
gis_site_page_shown (GisPage *gis_page)
{
  GisSitePage *page = GIS_SITE_PAGE (gis_page);
  GisSitePagePrivate *priv = gis_site_page_get_instance_private (page);

  gtk_widget_grab_focus (priv->search_entry);
}

/**
 * Parses the sites file which is a JSON file in the format:
 *
 * [SITE_1, SITE_2, SITE_3, ...]
 *
 * where each SITE is a JSON object containing one or more of the following
 * members:
 * * id: (string)       - a pre-existing ID (if any) the site admins use for it
 * * facility: (string) - a name for the site/building
 * * street: (string)   - the street address for the site (eg, "123 Main St.")
 * * locality: (string) - city or equivalent
 * * region: (string)   - state (US), department (Colombia), or equivalent
 * * country: (string)  - country
 */
static GPtrArray*
parse_sites_file (const gchar *filename)
{
  gboolean sites_usable = TRUE;
  GPtrArray *sites = NULL;
  JsonParser *parser;
  JsonNode *root;
  JsonReader *reader = NULL;
  GError *error;

  parser = json_parser_new ();

  error = NULL;
  json_parser_load_from_file (parser, filename, &error);
  if (error)
    {
      if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        g_warning ("Unable to parse ‘%s’: %s", filename, error->message);
      g_error_free (error);
      goto out;
    }

  root = json_parser_get_root (parser);
  if (!root)
    goto out;

  reader = json_reader_new (root);
  if (!json_reader_is_array (reader))
    goto out;

  sites = g_ptr_array_new_full (0, g_object_unref);
  int count = json_reader_count_elements (reader);
  for (int i = 0; count > 0 && i < count; i++)
    {
      json_reader_read_element (reader, i);
      if (json_reader_is_object (reader))
        {
          GisSite *site;
          const gchar *id = NULL;
          const gchar *facility = NULL;
          const gchar *street = NULL;
          const gchar *locality = NULL;
          const gchar *region = NULL;
          const gchar *country = NULL;

          json_reader_read_member (reader, "id");
          id = json_reader_get_string_value (reader);
          json_reader_end_member (reader);

          json_reader_read_member (reader, "facility");
          facility = json_reader_get_string_value (reader);
          json_reader_end_member (reader);

          json_reader_read_member (reader, "street");
          street = json_reader_get_string_value (reader);
          json_reader_end_member (reader);

          json_reader_read_member (reader, "locality");
          locality = json_reader_get_string_value (reader);
          json_reader_end_member (reader);

          json_reader_read_member (reader, "region");
          region = json_reader_get_string_value (reader);
          json_reader_end_member (reader);

          json_reader_read_member (reader, "country");
          country = json_reader_get_string_value (reader);
          json_reader_end_member (reader);

          site = gis_site_new (id, facility, street, locality, region, country);
          g_ptr_array_add (sites, site);
        }
      else
        {
          sites_usable = FALSE;
          goto out;
        }
      json_reader_end_element (reader);
    }

out:
  g_clear_object (&reader);
  g_clear_object (&parser);
  if (!sites_usable)
    g_clear_pointer (&sites, g_ptr_array_unref);

  return sites;
}

static void
gis_site_page_constructed (GObject *object)
{
  GisSitePage *page = GIS_SITE_PAGE (object);
  GisSitePagePrivate *priv = gis_site_page_get_instance_private (page);
  gboolean visible = TRUE;

  G_OBJECT_CLASS (gis_site_page_parent_class)->constructed (object);

  if (!g_file_test (GIS_SITE_PAGE_SITES_FILE, G_FILE_TEST_EXISTS))
    {
      visible = FALSE;
      goto out;
    }

  /* make all the fields insensitive to start */
  manual_check_toggled (GTK_TOGGLE_BUTTON (priv->manual_check), page);

  /* let the ID field show its current value (if populated based on a matched
   * pre-defined site) but don't let the user set it when editing manually
   */
  gtk_widget_set_can_focus (priv->id_entry, FALSE);
  gtk_widget_set_sensitive (priv->id_entry, FALSE);

  g_signal_connect (priv->search_entry,
                    "notify::" GIS_SITE_SEARCH_ENTRY_PROP_SITE,
                    G_CALLBACK (entry_site_changed), page);

  g_signal_connect (priv->manual_check, "toggled",
                    G_CALLBACK (manual_check_toggled), page);

  g_signal_connect (priv->id_entry,
                    "notify::text-length",
                    G_CALLBACK (update_page_validation), page);

  g_signal_connect (priv->facility_entry,
                    "notify::text-length",
                    G_CALLBACK (update_page_validation), page);

  g_signal_connect (priv->street_entry,
                    "notify::text-length",
                    G_CALLBACK (update_page_validation), page);

  g_signal_connect (priv->locality_entry,
                    "notify::text-length",
                    G_CALLBACK (update_page_validation), page);

  g_signal_connect (priv->region_entry,
                    "notify::text-length",
                    G_CALLBACK (update_page_validation), page);

  g_signal_connect (priv->country_entry,
                    "notify::text-length",
                    G_CALLBACK (update_page_validation), page);

out:
  gtk_widget_set_visible (GTK_WIDGET (page), visible);
}

static void
gis_site_page_class_init (GisSitePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/initial-setup/gis-site-page.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GisSitePage,
                                                search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GisSitePage,
                                                manual_check);
  gtk_widget_class_bind_template_child_private (widget_class, GisSitePage,
                                                id_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GisSitePage,
                                                facility_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GisSitePage,
                                                street_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GisSitePage,
                                                locality_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GisSitePage,
                                                region_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GisSitePage,
                                                country_entry);

  page_class->page_id = PAGE_ID;
  page_class->save_data = gis_site_page_save_data;
  page_class->shown = gis_site_page_shown;
  object_class->constructed = gis_site_page_constructed;
}

static void
gis_site_page_init (GisSitePage *page)
{
  GisSitePagePrivate *priv = gis_site_page_get_instance_private (page);
  GPtrArray *sites_array;

  g_resources_register (site_get_resource ());

  g_type_ensure (GIS_TYPE_SITE_SEARCH_ENTRY);

  gtk_widget_init_template (GTK_WIDGET (page));

  sites_array = parse_sites_file (GIS_SITE_PAGE_SITES_FILE);

  g_object_set (priv->search_entry, GIS_SITE_SEARCH_ENTRY_PROP_SITES_AVAILABLE,
                sites_array, NULL);

  g_clear_pointer (&sites_array, g_ptr_array_unref);
}

GisPage *
gis_prepare_site_page (GisDriver *driver)
{
  if (gis_driver_is_live_session (driver))
    return NULL;

  return g_object_new (GIS_TYPE_SITE_PAGE,
                       "driver", driver,
                       NULL);
}

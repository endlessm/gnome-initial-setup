/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/* location-entry.h - Location-selecting text entry
 *
 * Copyright 2008, Red Hat, Inc.
 * Copyright (C) 2018 Endless Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gis-site.h"

#define GIS_SITE_SEARCH_ENTRY_PROP_SITES_AVAILABLE "sites-available"
#define GIS_SITE_SEARCH_ENTRY_PROP_SITE "site"

#define GIS_TYPE_SITE_SEARCH_ENTRY (gis_site_search_entry_get_type())
G_DECLARE_DERIVABLE_TYPE (GisSiteSearchEntry, gis_site_search_entry, GIS, SITE_SEARCH_ENTRY, GtkSearchEntry)

struct _GisSiteSearchEntryClass {
    GtkSearchEntryClass parent_class;
};

GtkWidget*        gis_site_search_entry_new          (GPtrArray *sites);

void              gis_site_search_entry_set_site (GisSiteSearchEntry *entry,
                                                  GisSite            *site);

GisSite*          gis_site_search_entry_get_site (GisSiteSearchEntry *entry);

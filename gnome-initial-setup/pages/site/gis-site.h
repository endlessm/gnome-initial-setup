/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
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
 *     Travis Reitter <travis.reitter@endlessm.com>
 */

#pragma once

#include <glib.h>
#include <glib-object.h>

#define GIS_TYPE_SITE (gis_site_get_type())
G_DECLARE_FINAL_TYPE (GisSite, gis_site, GIS, SITE, GObject)

struct _GisSiteClass
{
  GObjectClass parent_class;
};

GisSite *     gis_site_new              (const gchar *id,
                                         const gchar *facility,
                                         const gchar *street,
                                         const gchar *locality,
                                         const gchar *region,
                                         const gchar *country);
const gchar * gis_site_get_id           (GisSite *site);
const gchar * gis_site_get_facility     (GisSite *site);
const gchar * gis_site_get_street       (GisSite *site);
const gchar * gis_site_get_locality     (GisSite *site);
const gchar * gis_site_get_region       (GisSite *site);
const gchar * gis_site_get_country      (GisSite *site);
gchar *       gis_site_get_display_name (GisSite *site);

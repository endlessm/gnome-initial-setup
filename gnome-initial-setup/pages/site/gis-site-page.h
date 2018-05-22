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

#ifndef __GIS_SITE_PAGE_H__
#define __GIS_SITE_PAGE_H__

#include <glib-object.h>

#include "gnome-initial-setup.h"

#define GIS_SITE_PAGE_ID "site"
#define GIS_SITE_PAGE_SITES_FILE "/var/lib/eos-image-defaults/deployment-sites.json"

G_BEGIN_DECLS

#define GIS_TYPE_SITE_PAGE               (gis_site_page_get_type ())
#define GIS_SITE_PAGE(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_SITE_PAGE, GisSitePage))
#define GIS_SITE_PAGE_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_SITE_PAGE, GisSitePageClass))
#define GIS_IS_SITE_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_SITE_PAGE))
#define GIS_IS_SITE_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_SITE_PAGE))
#define GIS_SITE_PAGE_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_SITE_PAGE, GisSitePageClass))

typedef struct _GisSitePage        GisSitePage;
typedef struct _GisSitePageClass   GisSitePageClass;

struct _GisSitePage
{
  GisPage parent;
};

struct _GisSitePageClass
{
  GisPageClass parent_class;
};

GType gis_site_page_get_type (void);

void gis_prepare_site_page (GisDriver *driver);

G_END_DECLS

#endif /* __GIS_SITE_PAGE_H__ */

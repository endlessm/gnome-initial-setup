/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2014–2024 Endless OS Foundation LLC
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
 */

/* Endless EULA page {{{1 */

#define PAGE_ID "endless-eula"

#include "config.h"
#include "gis-endless-eula-page.h"
#include "gis-endless-eula-viewer.h"

#include "endless-eula-resources.h"

#include <gio/gio.h>


struct _GisEndlessEulaPage {
  GisPage parent;
};

G_DEFINE_TYPE (GisEndlessEulaPage, gis_endless_eula_page, GIS_TYPE_PAGE)

static void
gis_endless_eula_page_class_init (GisEndlessEulaPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);

  g_type_ensure (GIS_TYPE_ENDLESS_EULA_VIEWER);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-endless-eula-page.ui");

  page_class->page_id = PAGE_ID;
}

static void
gis_endless_eula_page_init (GisEndlessEulaPage *page)
{
  g_resources_register (endless_eula_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_endless_eula_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_ENDLESS_EULA_PAGE,
                       "driver", driver,
                       NULL);
}

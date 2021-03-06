/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2014 Endless Mobile, Inc
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
 *     Cosimo Cecchi <cosimo@endlessm.com>
 */

#ifndef __GIS_ENDLESS_EULA_PAGE_H__
#define __GIS_ENDLESS_EULA_PAGE_H__

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

#define GIS_TYPE_ENDLESS_EULA_PAGE               (gis_endless_eula_page_get_type ())
#define GIS_ENDLESS_EULA_PAGE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_ENDLESS_EULA_PAGE, GisEndlessEulaPage))
#define GIS_ENDLESS_EULA_PAGE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_ENDLESS_EULA_PAGE, GisEndlessEulaPageClass))
#define GIS_IS_ENDLESS_EULA_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_ENDLESS_EULA_PAGE))
#define GIS_IS_ENDLESS_EULA_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_ENDLESS_EULA_PAGE))
#define GIS_ENDLESS_EULA_PAGE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_ENDLESS_EULA_PAGE, GisEndlessEulaPageClass))

typedef struct _GisEndlessEulaPage        GisEndlessEulaPage;
typedef struct _GisEndlessEulaPageClass   GisEndlessEulaPageClass;

struct _GisEndlessEulaPage
{
  GisPage parent;
};

struct _GisEndlessEulaPageClass
{
  GisPageClass parent_class;
};

GType gis_endless_eula_page_get_type (void);

GisPage *gis_prepare_endless_eula_page (GisDriver *driver);

G_END_DECLS

#endif /* __GIS_ENDLESS_EULA_PAGE_H__ */

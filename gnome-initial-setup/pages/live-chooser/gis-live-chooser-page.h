/* gis-live-chooser-page.h
 *
 * Copyright (C) 2016 Endless Mobile, Inc
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
 * Written by:
 *     Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 */

#ifndef GIS_LIVE_CHOOSER_PAGE_H
#define GIS_LIVE_CHOOSER_PAGE_H

#include "gnome-initial-setup.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define GIS_TYPE_LIVE_CHOOSER_PAGE (gis_live_chooser_page_get_type())
#define GIS_LIVE_CHOOSER_PAGE(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIS_TYPE_LIVE_CHOOSER_PAGE, GisLiveChooserPage))
#define GIS_LIVE_CHOOSER_PAGE_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass),  GIS_TYPE_LIVE_CHOOSER_PAGE, GisLiveChooserPageClass))
#define GIS_IS_LIVE_CHOOSER_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIS_TYPE_LIVE_CHOOSER_PAGE))
#define GIS_IS_LIVE_CHOOSER_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GIS_TYPE_LIVE_CHOOSER_PAGE))
#define GIS_LIVE_CHOOSER_PAGE_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),  GIS_TYPE_LIVE_CHOOSER_PAGE, GisLiveChooserPageClass))

typedef struct _GisLiveChooserPage        GisLiveChooserPage;
typedef struct _GisLiveChooserPageClass   GisLiveChooserPageClass;

struct _GisLiveChooserPage
{
    GisPage parent;
};

struct _GisLiveChooserPageClass
{
    GisPageClass parent_class;
};

GType gis_live_chooser_page_get_type (void);

GisPage *gis_prepare_live_chooser_page (GisDriver *driver);

G_END_DECLS

#endif /* GIS_LIVE_CHOOSER_PAGE_H */


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright © 2017–2018 Endless Mobile, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Mario Sanchez Prada <mario@endlessm.com>
 *     Will Thompson <wjt@endlessm.com>
 */

#ifndef __GIS_PAGE_UTIL_H__
#define __GIS_PAGE_UTIL_H__

#include <glib.h>

#include "gis-page.h"

G_BEGIN_DECLS

void gis_page_util_show_factory_dialog (GisPage *page);

void gis_page_util_run_reformatter (GisPage            *page,
                                    GAsyncReadyCallback callback,
                                    gpointer            user_data);

gchar *gis_page_util_get_image_version (const gchar *path,
                                        GError     **error);

G_END_DECLS

#endif

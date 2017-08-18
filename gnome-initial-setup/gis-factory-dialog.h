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
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GIS_TYPE_FACTORY_DIALOG (gis_factory_dialog_get_type())

G_DECLARE_FINAL_TYPE (GisFactoryDialog, gis_factory_dialog, GIS, FACTORY_DIALOG, GtkWindow)

GisFactoryDialog *gis_factory_dialog_new (const char *software_version,
                                          const char *image_version);

void              gis_factory_dialog_show_for_widget (GtkWidget *widget);

G_END_DECLS

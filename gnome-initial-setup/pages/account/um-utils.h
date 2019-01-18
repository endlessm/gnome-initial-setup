/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __UM_UTILS_H__
#define __UM_UTILS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Endless-specific: username and full name for the built-in shared account, an
 * unprivileged, passwordless user created after the administrator account on
 * most images.
 */
#define SHARED_ACCOUNT_USERNAME "shared"
#define SHARED_ACCOUNT_FULLNAME "Shared Account"

void     set_entry_validation_error       (GtkEntry    *entry,
                                           const gchar *text);
void     set_entry_validation_checkmark   (GtkEntry    *entry);
void     clear_entry_validation_error     (GtkEntry    *entry);

void     popup_menu_below_button          (GtkMenu     *menu,
                                           gint        *x,
                                           gint        *y,
                                           gboolean    *push_in,
                                           GtkWidget   *button);

void     down_arrow                       (GtkStyleContext *context,
                                           cairo_t         *cr,
                                           gint             x,
                                           gint             y,
                                           gint             width,
                                           gint             height);

gboolean is_valid_name                    (const gchar     *name);
gboolean is_valid_username                (const gchar     *name,
                                           gchar          **tip);

void     generate_username_choices        (const gchar     *name,
                                           GtkListStore    *store);

G_END_DECLS

#endif

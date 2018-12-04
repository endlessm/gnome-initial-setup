/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2018 Endless Mobile, Inc
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
 *     Damián Nohales <damian@endlessm.com>
 */

#ifndef __GIS_ACCOUNT_PAGE_EULA_DIALOG_H__
#define __GIS_ACCOUNT_PAGE_EULA_DIALOG_H__

#include "gnome-initial-setup.h"

G_BEGIN_DECLS

#define GIS_TYPE_ACCOUNT_PAGE_EULA_DIALOG (gis_account_page_eula_dialog_get_type ())
G_DECLARE_FINAL_TYPE (GisAccountPageEulaDialog, gis_account_page_eula_dialog, GIS, ACCOUNT_PAGE_EULA_DIALOG, GtkDialog)

G_END_DECLS

#endif /* __GIS_ACCOUNT_PAGE_EULA_DIALOG_H__ */

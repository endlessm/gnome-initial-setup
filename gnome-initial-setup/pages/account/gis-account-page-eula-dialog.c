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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Damián Nohales <damian@endlessm.com>
 */

#include "config.h"
#include "gis-account-page-eula-dialog.h"
#include "../endless-eula/gis-endless-eula-viewer.h"

struct _GisAccountPageEulaDialog {
  GtkDialog parent;

  GtkWidget *eula_viewer_container;
};

G_DEFINE_TYPE (GisAccountPageEulaDialog, gis_account_page_eula_dialog, GTK_TYPE_DIALOG)

static void
gis_account_page_eula_dialog_constructed (GObject *object)
{
  GisAccountPageEulaDialog *dialog = GIS_ACCOUNT_PAGE_EULA_DIALOG (object);
  GtkWidget *eula_viewer;

  G_OBJECT_CLASS (gis_account_page_eula_dialog_parent_class)->constructed (object);

  eula_viewer = g_object_new (GIS_TYPE_ENDLESS_EULA_VIEWER, NULL);
  gtk_container_add (GTK_CONTAINER (dialog->eula_viewer_container), eula_viewer);

  gtk_widget_show_all (GTK_WIDGET (dialog));
}

static void
gis_account_page_eula_dialog_class_init (GisAccountPageEulaDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-account-page-eula-dialog.ui");
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GisAccountPageEulaDialog, eula_viewer_container);

  object_class->constructed = gis_account_page_eula_dialog_constructed;
}

static void
gis_account_page_eula_dialog_init (GisAccountPageEulaDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
}

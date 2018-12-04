/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2014 Endless Mobile, Inc.
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

/* Endless EULA page {{{1 */

#define PAGE_ID "endless-eula"

#include "config.h"
#include "gis-page-util.h"
#include "gis-endless-eula-page.h"
#include "gis-endless-eula-viewer.h"

#include "endless-eula-resources.h"

#include <eosmetrics/eosmetrics.h>
#include <gio/gio.h>

typedef struct {
  GtkWidget *eula_viewer_container;
  GtkWidget *metrics_separator;
  GtkWidget *metrics_label;
  GtkWidget *metrics_checkbutton;
  GtkWidget *metrics_privacy_label;

  GisEndlessEulaViewer *eula_viewer;
} GisEndlessEulaPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisEndlessEulaPage, gis_endless_eula_page, GIS_TYPE_PAGE);

static void
gis_endless_eula_page_constructed (GObject *object)
{
  GisEndlessEulaPage *page = GIS_ENDLESS_EULA_PAGE (object);
  GisEndlessEulaPagePrivate *priv = gis_endless_eula_page_get_instance_private (page);
  gboolean demo_mode;

  G_OBJECT_CLASS (gis_endless_eula_page_parent_class)->constructed (object);

  gtk_widget_show (GTK_WIDGET (page));

  gis_page_set_forward_text (GIS_PAGE (page), _("_Accept and Continue"));
  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_container_add (GTK_CONTAINER (priv->eula_viewer_container), GTK_WIDGET (priv->eula_viewer));

  /* Hide the page in demo mode; we still want to create it, since we
   * want the save_data implementation to run at the end of the FBE.
   *
   * We'll want metrics to remain on, except that the tracking-id will
   * be reset.
   */
  demo_mode = gis_driver_is_in_demo_mode (GIS_PAGE (page)->driver);
  gtk_widget_set_visible (GTK_WIDGET (page), !demo_mode);
}

static void
gis_endless_eula_page_locale_changed (GisPage *page)
{
  GisEndlessEulaPage *self = GIS_ENDLESS_EULA_PAGE (page);
  GisEndlessEulaPagePrivate *priv = gis_endless_eula_page_get_instance_private (self);

  gis_page_set_title (page, _("Terms of Use"));
  gis_endless_eula_viewer_reload (priv->eula_viewer);
}

#define DEMO_MODE_ENTERED_METRIC "c75af67f-cf2f-433d-a060-a670087d93a1"

void
report_demo_mode_metric (void)
{
  EmtrEventRecorder *recorder = emtr_event_recorder_get_default ();
  GVariantDict dict;

  g_variant_dict_init (&dict, NULL);

  emtr_event_recorder_record_event (recorder,
                                    DEMO_MODE_ENTERED_METRIC,
                                    g_variant_dict_end (&dict));
}


static void
gis_endless_eula_page_save_data (GisPage *page)
{
  GisEndlessEulaPage *self = GIS_ENDLESS_EULA_PAGE (page);
  GisEndlessEulaPagePrivate *priv = gis_endless_eula_page_get_instance_private (self);
  gboolean metrics_active, demo_mode = FALSE;

  metrics_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->metrics_checkbutton));
  gis_page_util_set_endlessm_metrics (metrics_active);

  demo_mode = gis_driver_is_in_demo_mode (GIS_PAGE (page)->driver);

  if (demo_mode)
    report_demo_mode_metric ();
}

static void
gis_endless_eula_page_class_init (GisEndlessEulaPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-endless-eula-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisEndlessEulaPage, eula_viewer_container);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisEndlessEulaPage, metrics_separator);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisEndlessEulaPage, metrics_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisEndlessEulaPage, metrics_checkbutton);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisEndlessEulaPage, metrics_privacy_label);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_endless_eula_page_locale_changed;
  page_class->save_data = gis_endless_eula_page_save_data;
  object_class->constructed = gis_endless_eula_page_constructed;
}

static void
gis_endless_eula_page_init (GisEndlessEulaPage *page)
{
  GisEndlessEulaPagePrivate *priv = gis_endless_eula_page_get_instance_private (page);

  g_resources_register (endless_eula_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (page));

  priv->eula_viewer = g_object_new (GIS_TYPE_ENDLESS_EULA_VIEWER, NULL);
}

void
gis_prepare_endless_eula_page (GisDriver *driver)
{
  if (gis_driver_is_hack (driver))
    return;

  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_ENDLESS_EULA_PAGE,
                                     "driver", driver,
                                     NULL));
}

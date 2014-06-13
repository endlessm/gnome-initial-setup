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
#include "gis-endless-eula-page.h"
#include "gis-endless-eula-viewer.h"

#include "endless-eula-resources.h"

#include <eosmetrics/eosmetrics.h>
#include <evince-view.h>
#include <evince-document.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

typedef struct {
  GtkWidget *eula_viewer_container;
  GtkWidget *metrics_separator;
  GtkWidget *metrics_label;
  GtkWidget *metrics_checkbutton;
  GtkWidget *metrics_privacy_label;

  GisEndlessEulaViewer *eula_viewer;
  GDBusProxy *metrics_proxy;
} GisEndlessEulaPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisEndlessEulaPage, gis_endless_eula_page, GIS_TYPE_PAGE);

static void
sync_metrics_active_state (GisEndlessEulaPage *page)
{
  GisEndlessEulaPagePrivate *priv = gis_endless_eula_page_get_instance_private (page);
  GError *error = NULL;
  GtkWidget *widget;
  gboolean metrics_active;

  widget = priv->metrics_checkbutton;
  metrics_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

  if (!priv->metrics_proxy)
    return;

  g_dbus_proxy_call_sync (priv->metrics_proxy,
                          "SetEnabled",
                          g_variant_new ("(b)", metrics_active),
                          G_DBUS_CALL_FLAGS_NONE, -1,
                          NULL, &error);

  if (error != NULL)
    {
      g_critical ("Unable to set the enabled state of metrics daemon: %s", error->message);
      g_error_free (error);
    }
}

static void
gis_endless_eula_page_finalize (GObject *object)
{
  GisEndlessEulaPage *page = GIS_ENDLESS_EULA_PAGE (object);
  GisEndlessEulaPagePrivate *priv = gis_endless_eula_page_get_instance_private (page);

  g_clear_object (&priv->metrics_proxy);

  G_OBJECT_CLASS (gis_endless_eula_page_parent_class)->finalize (object);
}

static void
gis_endless_eula_page_constructed (GObject *object)
{
  GisEndlessEulaPage *page = GIS_ENDLESS_EULA_PAGE (object);
  GisEndlessEulaPagePrivate *priv = gis_endless_eula_page_get_instance_private (page);
  GError *error = NULL;

  G_OBJECT_CLASS (gis_endless_eula_page_parent_class)->constructed (object);

  priv->metrics_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       NULL,
                                                       "com.endlessm.Metrics",
                                                       "/com/endlessm/Metrics",
                                                       "com.endlessm.Metrics.EventRecorderServer",
                                                       NULL, &error);

  if (error != NULL)
    {
      g_critical ("Unable to create a DBus proxy for the metrics daemon: %s", error->message);
      g_error_free (error);
    }

  gtk_widget_show (GTK_WIDGET (page));

  gis_page_set_needs_accept (GIS_PAGE (page), TRUE);
  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_container_add (GTK_CONTAINER (priv->eula_viewer_container), GTK_WIDGET (priv->eula_viewer));
}

static void
gis_endless_eula_page_locale_changed (GisPage *page)
{
  GisEndlessEulaPage *self = GIS_ENDLESS_EULA_PAGE (page);
  GisEndlessEulaPagePrivate *priv = gis_endless_eula_page_get_instance_private (self);

  gis_page_set_title (page, _("Terms of Use"));
  gis_endless_eula_viewer_reload (priv->eula_viewer);
}

static gboolean
gis_endless_eula_page_save_data (GisPage  *page,
                                 GError  **error)
{
  sync_metrics_active_state (GIS_ENDLESS_EULA_PAGE (page));

  return TRUE;
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
  object_class->finalize = gis_endless_eula_page_finalize;
}

static void
gis_endless_eula_page_init (GisEndlessEulaPage *page)
{
  GisEndlessEulaPagePrivate *priv = gis_endless_eula_page_get_instance_private (page);

  g_resources_register (endless_eula_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (page));

  priv->eula_viewer = g_object_new (GIS_TYPE_ENDLESS_EULA_VIEWER, NULL);
}

GisPage *
gis_prepare_endless_eula_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_ENDLESS_EULA_PAGE,
                       "driver", driver,
                       NULL);
}

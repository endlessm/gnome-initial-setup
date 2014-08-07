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
 *     Emmanuele Bassi <emmanuele@endlessm.com>
 */

/* Display page {{{1 */

#define PAGE_ID "display"

#include "config.h"
#include "gis-display-page.h"

#include "display-resources.h"

#include "cc-display-config-manager-dbus.h"
#include "cc-display-config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

typedef struct {
  GtkWidget *overscan_on;
  GtkWidget *overscan_off;
  GtkWidget *overscan_default_selection;

  CcDisplayConfigManager *manager;
  CcDisplayConfig *current_config;
  CcDisplayMonitor *current_output;

  gulong screen_changed_id;
} GisDisplayPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisDisplayPage, gis_display_page, GIS_TYPE_PAGE);

static void
set_toggle_options_from_config (GisDisplayPage *page)
{
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  GtkWidget *default_toggle, *toggle;

  /* Do not set the toggle if we're on the default selection */
  default_toggle = priv->overscan_default_selection;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (default_toggle)))
    return;

  if (cc_display_monitor_get_underscanning (priv->current_output))
    toggle = priv->overscan_on;
  else
    toggle = priv->overscan_off;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), TRUE);
}

static gboolean
should_display_overscan (GisDisplayPage *page)
{
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);

  if (!priv->current_output)
    return FALSE;

  return cc_display_monitor_supports_underscanning (priv->current_output);
}

static gboolean
read_screen_config (GisDisplayPage *page)
{
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  CcDisplayConfig *current;
  GList *outputs, *l;

  g_clear_object (&priv->current_config);
  priv->current_output = NULL;

  current = cc_display_config_manager_get_current (priv->manager);
  if (!current)
    {
      g_critical ("Could not get primary output information.");
      return FALSE;
    }

  priv->current_config = current;

  outputs = cc_display_config_get_monitors (priv->current_config);
  for (l = outputs; l != NULL; l = l->next)
    {
      CcDisplayMonitor *output = l->data;

      if (!cc_display_monitor_is_active (output))
        continue;

      priv->current_output = output;
      break;
    }

  return TRUE;
}

static void
toggle_overscan (GisDisplayPage *page,
                 gboolean value)
{
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  GError *error = NULL;

  if (value == cc_display_monitor_get_underscanning (priv->current_output))
    return;

  cc_display_monitor_set_underscanning (priv->current_output, value);

  if (!cc_display_config_apply (priv->current_config, &error))
    {
      g_warning ("Error applying configuration: %s", error->message);
      g_clear_error (&error);
      return;
    }
}

static void
update_overscan (GisDisplayPage *page)
{
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  GtkWidget *widget;
  gboolean value;

  widget = priv->overscan_on;
  value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  toggle_overscan (page, value);
}

static void
overscan_radio_toggled (GtkWidget *radio,
                        GisDisplayPage *page)
{
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  GtkWidget *widget;

  widget = priv->overscan_default_selection;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    {
      gis_page_set_complete (GIS_PAGE (page), FALSE);
      return;
    }

  /* user has made a choice, page is complete */
  gis_page_set_complete (GIS_PAGE (page), TRUE);
}

static void
gis_display_page_dispose (GObject *gobject)
{
  GisDisplayPage *page = GIS_DISPLAY_PAGE (gobject);
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);

  if (priv->screen_changed_id != 0)
    {
      g_signal_handler_disconnect (priv->manager, priv->screen_changed_id);
      priv->screen_changed_id = 0;
    }

  g_clear_object (&priv->manager);
  g_clear_object (&priv->current_config);

  G_OBJECT_CLASS (gis_display_page_parent_class)->dispose (gobject);
}

static void
on_screen_changed (GisDisplayPage *page)
{
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  gboolean visible = FALSE;

  if (!priv->manager)
    return;

  if (!read_screen_config (page))
    {
      g_warning ("Could not read screen configuration. Hiding overscan page.");
      goto out;
    }

  if (should_display_overscan (page))
    {
      g_debug ("Overscanning supported on primary display. Showing overscan page.");
      visible = TRUE;

      set_toggle_options_from_config (page);
    }

 out:
  gtk_widget_set_visible (GTK_WIDGET (page), visible);
}

static void
gis_display_page_constructed (GObject *object)
{
  GisDisplayPage *page = GIS_DISPLAY_PAGE (object);
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  GtkWidget *widget;

  G_OBJECT_CLASS (gis_display_page_parent_class)->constructed (object);

  priv->manager = cc_display_config_manager_dbus_new ();
  priv->screen_changed_id = g_signal_connect_object (priv->manager, "changed",
                                                     G_CALLBACK (on_screen_changed),
                                                     page,
                                                     G_CONNECT_SWAPPED);
  widget = priv->overscan_on;
  g_signal_connect (widget, "toggled",
                    G_CALLBACK (overscan_radio_toggled), page);

  widget = priv->overscan_off;
  g_signal_connect (widget, "toggled",
                    G_CALLBACK (overscan_radio_toggled), page);

  gtk_widget_show (GTK_WIDGET (page));
}

static gboolean
gis_display_page_apply (GisPage *gis_page,
                        GCancellable *cancellable)
{
  update_overscan (GIS_DISPLAY_PAGE (gis_page));

  return FALSE;
}

static void
gis_display_page_locale_changed (GisPage *page)
{
  gis_page_set_title (page, _("Display"));
}

static void
gis_display_page_class_init (GisDisplayPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-display-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisDisplayPage, overscan_on);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisDisplayPage, overscan_off);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisDisplayPage, overscan_default_selection);

  page_class->page_id = PAGE_ID;
  page_class->apply = gis_display_page_apply;
  page_class->locale_changed = gis_display_page_locale_changed;
  object_class->constructed = gis_display_page_constructed;
  object_class->dispose = gis_display_page_dispose;
}

static void
gis_display_page_init (GisDisplayPage *page)
{
  g_resources_register (display_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_display_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_DISPLAY_PAGE,
                       "driver", driver,
                       NULL);
}

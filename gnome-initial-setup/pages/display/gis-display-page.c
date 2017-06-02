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

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libgnome-desktop/gnome-rr.h>
#include <libgnome-desktop/gnome-rr-config.h>

typedef struct {
  GtkWidget *overscan_on;
  GtkWidget *overscan_off;
  GtkWidget *overscan_default_selection;

  GnomeRRScreen *screen;
  GnomeRRConfig *current_config;
  GnomeRROutputInfo *current_output;

  guint screen_changed_id;
  guint next_page_id;
} GisDisplayPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisDisplayPage, gis_display_page, GIS_TYPE_PAGE);

static void
set_toggle_options_from_config (GisDisplayPage *page)
{
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  GtkWidget *default_toggle, *toggle;
  gboolean is_underscanning;

  /* Do not set the toggle if we're on the default selection */
  default_toggle = priv->overscan_default_selection;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (default_toggle)))
    return;

  is_underscanning = gnome_rr_output_info_get_underscanning  (priv->current_output);

  if (is_underscanning)
    toggle = priv->overscan_on;
  else
    toggle = priv->overscan_off;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), TRUE);
}

static gboolean
read_screen_config (GisDisplayPage *page)
{
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  GnomeRRConfig *current;
  GnomeRROutputInfo **outputs;
  GnomeRROutputInfo *output;
  int i;

  gnome_rr_screen_refresh (priv->screen, NULL);

  g_clear_object (&priv->current_config);

  current = gnome_rr_config_new_current (priv->screen, NULL);
  gnome_rr_config_ensure_primary (current);
  priv->current_config = current;

  outputs = gnome_rr_config_get_outputs (current);
  output = NULL;

  /* we take the primary and active display */
  for (i = 0; outputs[i] != NULL; i++)
    {
      if (gnome_rr_output_info_is_active (outputs[i]) &&
          gnome_rr_output_info_get_primary (outputs[i]))
        {
          output = outputs[i];
          break;
        }
    }

  priv->current_output = output;
  if (priv->current_output == NULL)
    return FALSE;

  set_toggle_options_from_config (page);

  return TRUE;
}

static void
toggle_overscan (GisDisplayPage *page,
                 gboolean value)
{
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  GError *error;

  if (value == gnome_rr_output_info_get_underscanning  (priv->current_output))
    return;

  gnome_rr_output_info_set_underscanning (priv->current_output, value);

  gnome_rr_config_sanitize (priv->current_config);
  gnome_rr_config_ensure_primary (priv->current_config);

  error = NULL;
  gnome_rr_config_apply_persistent (priv->current_config, priv->screen, &error);

  read_screen_config (page);

  if (error != NULL)
    {
      g_warning ("Error applying configuration: %s", error->message);
      g_error_free (error);
    }
}

static gboolean
should_display_overscan (GisDisplayPage *page)
{
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  GnomeRROutput *output;
  char *output_name;

  output_name = gnome_rr_output_info_get_name (priv->current_output);
  output = gnome_rr_screen_get_output_by_name (priv->screen, output_name);

  if (!output)
    return FALSE;

  return gnome_rr_output_supports_underscanning (output);
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
next_page_cb (GisAssistant *assistant,
              GisPage *which_page,
              GisPage *this_page)
{
  if (which_page == this_page)
    update_overscan (GIS_DISPLAY_PAGE (this_page));
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
      g_signal_handler_disconnect (priv->screen, priv->screen_changed_id);
      priv->screen_changed_id = 0;
    }

  if (priv->next_page_id != 0)
    {
      g_signal_handler_disconnect (gis_driver_get_assistant (GIS_PAGE (page)->driver),
                                   priv->next_page_id);
      priv->next_page_id = 0;
    }

  g_clear_object (&priv->current_config);
  g_clear_object (&priv->screen);

  G_OBJECT_CLASS (gis_display_page_parent_class)->dispose (gobject);
}

static void
gis_display_page_constructed (GObject *object)
{
  GisDisplayPage *page = GIS_DISPLAY_PAGE (object);
  GisDisplayPagePrivate *priv = gis_display_page_get_instance_private (page);
  GtkWidget *widget;
  GError *error = NULL;
  gboolean visible = FALSE;

  G_OBJECT_CLASS (gis_display_page_parent_class)->constructed (object);

  gtk_widget_show (GTK_WIDGET (page));

  priv->screen = gnome_rr_screen_new (gdk_screen_get_default (), &error);
  if (priv->screen == NULL)
    {
      g_critical ("Could not get screen information: %s. Hiding overscan page.",
                  error->message);
      g_error_free (error);
      goto out;
    }

  if (!read_screen_config (page))
    {
      g_critical ("Could not get primary output information. Hiding overscan page.");
      goto out;
    }

  if (!should_display_overscan (page))
    {
      g_debug ("Not using an HD resolution on HDMI. Hiding overscan page.");
      goto out;
    }

  visible = TRUE;
  priv->screen_changed_id = g_signal_connect_swapped (priv->screen,
                                                      "changed",
                                                      G_CALLBACK (read_screen_config),
                                                      page);

  priv->next_page_id = g_signal_connect (gis_driver_get_assistant (GIS_PAGE (page)->driver),
                                         "next-page",
                                         G_CALLBACK (next_page_cb),
                                         page);

  widget = priv->overscan_on;
  g_signal_connect (widget, "toggled",
                    G_CALLBACK (overscan_radio_toggled), page);

  widget = priv->overscan_off;
  g_signal_connect (widget, "toggled",
                    G_CALLBACK (overscan_radio_toggled), page);

 out:
  gtk_widget_set_visible (GTK_WIDGET (page), visible);
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

void
gis_prepare_display_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_DISPLAY_PAGE,
                                     "driver", driver,
                                     NULL));
}

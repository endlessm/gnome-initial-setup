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
#include "gis-endless-eula-viewer.h"

#include <evince-document.h>
#include <evince-view.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <webkit2/webkit2.h>

struct _GisEndlessEulaViewer {
  GtkScrolledWindow parent;
};

G_DEFINE_TYPE (GisEndlessEulaViewer, gis_endless_eula_viewer, GTK_TYPE_SCROLLED_WINDOW)

#define LICENSE_SERVICE_URI "http://localhost:3010"

static void
present_view_in_modal (GisEndlessEulaViewer *viewer,
                       GtkWidget *view,
                       const gchar *title,
                       gboolean needs_scrolled_window)
{
  GtkWidget *widget, *modal, *content_area, *toplevel;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (viewer));
  modal = gtk_dialog_new_with_buttons (title, GTK_WINDOW (toplevel),
                                       GTK_DIALOG_MODAL |
                                       GTK_DIALOG_USE_HEADER_BAR,
                                       NULL,
                                       NULL);
  gtk_window_set_default_size (GTK_WINDOW (modal), 600, 500);

  if (needs_scrolled_window)
    {
      widget = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_add (GTK_CONTAINER (widget), view);
    }
  else
    {
      widget = view;
    }

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (modal));
  gtk_container_add (GTK_CONTAINER (content_area), widget);
  gtk_widget_show_all (modal);

  g_signal_connect (modal, "response",
                    G_CALLBACK (gtk_widget_destroy), NULL);
}

static char *
find_terms_document_for_language (const gchar *product_name,
                                  const gchar *language)
{
  const gchar * const * data_dirs;
  gchar *path = NULL;
  gint i;

  data_dirs = g_get_system_data_dirs ();

  for (i = 0; data_dirs[i] != NULL; i++)
    {
      path = g_build_filename (data_dirs[i],
                               "eos-license-service",
                               "terms",
                               product_name,
                               language,
                               "Terms-of-Use.pdf",
                               NULL);

      if (g_file_test (path, G_FILE_TEST_EXISTS))
        return path;

      g_free (path);
    }

  return NULL;
}

static char *
find_terms_document_for_languages (const gchar *product_name,
                                   const gchar * const *languages)
{
  int i;
  gchar *path = NULL;

  if (product_name == NULL)
    return NULL;

  for (i = 0; languages[i] != NULL; i++)
    {
      path = find_terms_document_for_language (product_name, languages[i]);

      if (path != NULL)
        return path;
    }

  /* Use "C" as a fallback. */
  return find_terms_document_for_language (product_name, "C");
}

static GFile *
get_terms_document (void)
{
  GisDriver *driver;
  gchar **locale_variants;
  const gchar *product_name;
  const gchar *language;
  gchar *path = NULL;
  GFile *file;

  driver = GIS_DRIVER (g_application_get_default ());
  language = gis_driver_get_user_language (driver);
  locale_variants = g_get_locale_variants (language);
  product_name = gis_driver_get_product_name (driver);

  path = find_terms_document_for_languages (product_name, (const gchar * const *)locale_variants);
  if (path == NULL)
    path = find_terms_document_for_languages ("eos", (const gchar * const *)locale_variants);

  g_strfreev (locale_variants);

  if (path == NULL)
    {
      g_critical ("Unable to find terms and conditions PDF on the system");
      return NULL;
    }

  file = g_file_new_for_path (path);
  g_free (path);

  return file;
}

static GtkWidget *
load_license_service_view (void)
{
  GtkWidget *view;

  view = webkit_web_view_new ();
  webkit_web_view_load_uri (WEBKIT_WEB_VIEW (view), LICENSE_SERVICE_URI);
  gtk_widget_set_hexpand (view, TRUE);
  gtk_widget_set_vexpand (view, TRUE);

  return view;
}

static EvDocument *
load_evince_document_for_file (GFile *file)
{
  EvDocument *document;
  GError *error = NULL;
  gchar *path;

  document = ev_document_factory_get_document_for_gfile (file,
                                                         EV_DOCUMENT_LOAD_FLAG_NONE,
                                                         NULL,
                                                         &error);
  if (error != NULL)
    {
      path = g_file_get_path (file);
      g_critical ("Unable to load EvDocument for file %s: %s", path, error->message);
      g_error_free (error);
      g_free (path);

      return NULL;
    }

  return document;
}

static GtkWidget *
load_evince_view_for_file (GFile *file)
{
  EvDocument *document;
  EvDocumentModel *model;
  GtkWidget *view;

  document = load_evince_document_for_file (file);
  g_return_val_if_fail (document != NULL, NULL);

  model = ev_document_model_new_with_document (document);
  view = ev_view_new ();
  ev_view_set_model (EV_VIEW (view), model);

  g_object_unref (document);
  g_object_unref (model);

  gtk_widget_set_hexpand (view, TRUE);
  gtk_widget_set_vexpand (view, TRUE);

  return view;
}

static void
evince_view_external_link_cb (EvView *ev_view,
                              EvLinkAction *action,
                              GisEndlessEulaViewer *viewer)
{
  const gchar *uri;
  GFile *file;
  GtkWidget *view;

  uri = ev_link_action_get_uri (action);
  file = g_file_new_for_uri (uri);

  if (g_file_has_uri_scheme (file, "file"))
    {
      view = load_evince_view_for_file (file);
      if (view != NULL)
        present_view_in_modal (viewer, view, _("Terms of Use"), TRUE);
    }
  else if (g_str_has_prefix (uri, LICENSE_SERVICE_URI))
    {
      view = load_license_service_view ();
      present_view_in_modal (viewer, view, _("Third Party Software"), FALSE);
    }

  g_object_unref (file);
}

static void
load_terms_view (GisEndlessEulaViewer *viewer)
{
  GtkWidget *evince_view;
  GFile *file;

  file = get_terms_document ();
  if (file == NULL)
    return;

  evince_view = load_evince_view_for_file (file);
  g_object_unref (file);

  if (evince_view == NULL)
    return;

  gtk_container_add (GTK_CONTAINER (viewer), evince_view);

  g_signal_connect (evince_view, "external-link",
                    G_CALLBACK (evince_view_external_link_cb), viewer);
}

void
gis_endless_eula_viewer_reload (GisEndlessEulaViewer *viewer)
{
  GtkWidget *evince_view;
  EvDocument *document;
  EvDocumentModel *model;
  GFile *file;

  evince_view = gtk_bin_get_child (GTK_BIN (viewer));

  if (evince_view == NULL)
    return;

  file = get_terms_document ();
  if (file == NULL)
    return;

  document = load_evince_document_for_file (file);
  g_return_if_fail (document != NULL);

  model = ev_document_model_new_with_document (document);
  ev_view_set_model (EV_VIEW (evince_view), model);

  g_object_unref (document);
  g_object_unref (model);
}

static void
load_css_overrides (GisEndlessEulaViewer *viewer)
{
  GtkCssProvider *provider;
  GError *error = NULL;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/initial-setup/endless-eula-page.css");

  if (error != NULL)
    {
      g_warning ("Unable to load CSS overrides for the endless-eula page: %s",
                 error->message);
      g_error_free (error);
    }
  else
    {
      gtk_style_context_add_provider_for_screen (gtk_widget_get_screen (GTK_WIDGET (viewer)),
                                                 GTK_STYLE_PROVIDER (provider),
                                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  g_object_unref (provider);
}

static void
gis_endless_eula_viewer_constructed (GObject *object)
{
  GisEndlessEulaViewer *viewer = GIS_ENDLESS_EULA_VIEWER (object);

  G_OBJECT_CLASS (gis_endless_eula_viewer_parent_class)->constructed (object);

  load_css_overrides (viewer);
  load_terms_view (viewer);
  gtk_widget_show_all (GTK_WIDGET (viewer));
}

static void
gis_endless_eula_viewer_class_init (GisEndlessEulaViewerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = gis_endless_eula_viewer_constructed;
}

static void
gis_endless_eula_viewer_init (GisEndlessEulaViewer *viewer)
{
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (viewer), GTK_SHADOW_IN);
}

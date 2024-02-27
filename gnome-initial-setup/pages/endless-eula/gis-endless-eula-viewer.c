/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2018–2024 Endless OS Foundation LLC
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
 *     Will Thompson <wjt@endlessos.org>
 */

#include "config.h"
#include "gis-endless-eula-viewer.h"

#include <gtk/gtk.h>

struct _GisEndlessEulaViewer {
  WebKitWebView parent;

  char *terms_uri;
};

G_DEFINE_TYPE (GisEndlessEulaViewer, gis_endless_eula_viewer, WEBKIT_TYPE_WEB_VIEW);

#define LICENSE_SERVICE_URI "http://localhost:3010"

static void
notify_progress_cb (GObject    *object,
                    GParamSpec *pspec,
                    gpointer    user_data)
{
  GtkWidget *progress_bar = GTK_WIDGET (user_data);
  WebKitWebView *web_view = WEBKIT_WEB_VIEW (object);
  gdouble progress;

  progress = webkit_web_view_get_estimated_load_progress (web_view);

  if (progress == 1.0)
    gtk_widget_hide (progress_bar);
  else
    gtk_widget_show (progress_bar);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar), progress);
}

static void
notify_title_cb (GObject    *object,
                 GParamSpec *pspec,
                 gpointer    user_data)
{
  WebKitWebView *web_view = WEBKIT_WEB_VIEW (object);
  GtkWindow *dialog = GTK_WINDOW (user_data);
  const gchar *title = webkit_web_view_get_title (web_view);
  if (title != NULL)
    gtk_window_set_title (dialog, title);
}

static void
show_uri_in_modal (GisEndlessEulaViewer *viewer,
                   const gchar          *uri,
                   const gchar          *title)
{
  GtkWidget *dialog;
  GtkWidget *overlay;
  GtkWidget *view;
  GtkWidget *progress_bar;

  dialog = gtk_dialog_new_with_buttons (title ? title : "",
                                        GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (viewer))),
                                        GTK_DIALOG_MODAL |
                                        GTK_DIALOG_USE_HEADER_BAR |
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        NULL,
                                        NULL);
  overlay = gtk_overlay_new ();
  gtk_box_append (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), overlay);

  progress_bar = gtk_progress_bar_new ();
  gtk_widget_add_css_class (progress_bar, "osd");
  gtk_widget_set_halign (progress_bar, GTK_ALIGN_FILL);
  gtk_widget_set_valign (progress_bar, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), progress_bar);

  view = webkit_web_view_new ();
  gtk_widget_set_size_request (view, 800, 600);
  gtk_widget_set_hexpand (view, TRUE);
  gtk_widget_set_vexpand (view, TRUE);
  g_signal_connect (view, "notify::estimated-load-progress",
                    G_CALLBACK (notify_progress_cb), progress_bar);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), view);

  g_signal_connect (view, "notify::title",
                    G_CALLBACK (notify_title_cb), dialog);

  gtk_window_present (GTK_WINDOW (dialog));

  webkit_web_view_load_uri (WEBKIT_WEB_VIEW (view), uri);
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
  g_auto(GStrv) locale_variants = NULL;
  const gchar *product_name;
  const gchar *language;
  g_autofree gchar *path = NULL;

  driver = GIS_DRIVER (g_application_get_default ());
  language = gis_driver_get_user_language (driver);
  locale_variants = g_get_locale_variants (language);
  product_name = gis_driver_get_product_name (driver);

  path = find_terms_document_for_languages (product_name, (const gchar * const *)locale_variants);
  if (path == NULL)
    path = find_terms_document_for_languages ("eos", (const gchar * const *)locale_variants);

  if (path == NULL)
    {
      g_critical ("Unable to find terms and conditions PDF on the system");
      return NULL;
    }

  return g_file_new_for_path (path);
}

static gboolean
gis_endless_eula_viewer_decide_policy_cb (GisEndlessEulaViewer     *self,
                                          WebKitPolicyDecision     *decision,
                                          WebKitPolicyDecisionType  decision_type,
                                          gpointer                  user_data)
{
  g_return_val_if_fail (GIS_IS_ENDLESS_EULA_VIEWER (self), FALSE);

  if (decision_type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
    return FALSE;

  WebKitNavigationPolicyDecision *navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
  WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action (navigation_decision);
  WebKitURIRequest *request = webkit_navigation_action_get_request (action);
  const gchar *uri = webkit_uri_request_get_uri (request);

  if (g_strcmp0 (uri, self->terms_uri) == 0)
    {
      webkit_policy_decision_use (decision);
      return TRUE;
    }

  webkit_policy_decision_ignore (decision);

  const gchar *title = NULL;

  if (g_str_has_prefix (uri, "file:"))
    title = _("Terms of Use");
  else if (g_str_has_prefix (uri, LICENSE_SERVICE_URI))
    title = _("Third Party Software");
  else
    /* Ignore attempts to navigate to remote pages: at this point in Initial
     * Setup the user has not had a chance to configure Wi-Fi so is almost
     * certainly offline.
     */
    return TRUE;

  show_uri_in_modal (self, uri, title);
  return TRUE;
}

static gboolean
gis_endless_eula_viewer_context_menu_cb (WebKitWebView       *self,
                                         WebKitContextMenu   *context_menu,
                                         WebKitHitTestResult *hit_test_result,
                                         gpointer             user_data)
{
  /* Hide default context menu: all useful actions are available in the PDF.js
   * toolbar.
   */
  return TRUE;
}

static void
load_terms_view (GisEndlessEulaViewer *self)
{
  g_autoptr(GFile) file = NULL;

  file = get_terms_document ();
  if (file == NULL)
    return;

  g_assert (self->terms_uri == NULL);
  self->terms_uri = g_file_get_uri (file);
  webkit_web_view_load_uri (WEBKIT_WEB_VIEW (self), self->terms_uri);
}

static void
gis_endless_eula_viewer_constructed (GObject *object)
{
  GisEndlessEulaViewer *self = GIS_ENDLESS_EULA_VIEWER (object);

  G_OBJECT_CLASS (gis_endless_eula_viewer_parent_class)->constructed (object);

  load_terms_view (self);
}


static void
gis_endless_eula_viewer_finalize (GObject *object)
{
  GisEndlessEulaViewer *self = GIS_ENDLESS_EULA_VIEWER (object);

  g_clear_pointer (&self->terms_uri, g_free);

  G_OBJECT_CLASS (gis_endless_eula_viewer_parent_class)->finalize (object);
}

static void
gis_endless_eula_viewer_class_init (GisEndlessEulaViewerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_ensure (WEBKIT_TYPE_WEB_VIEW);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-endless-eula-viewer.ui");
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), gis_endless_eula_viewer_decide_policy_cb);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), gis_endless_eula_viewer_context_menu_cb);

  object_class->constructed = gis_endless_eula_viewer_constructed;
  object_class->finalize = gis_endless_eula_viewer_finalize;
}

static void
gis_endless_eula_viewer_init (GisEndlessEulaViewer *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

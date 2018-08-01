/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "gnome-initial-setup.h"

#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <evince-document.h>

#ifdef HAVE_CHEESE
#include <cheese-gtk.h>
#endif

#include "pages/branding-welcome/gis-branding-welcome-page.h"
#include "pages/language/gis-language-page.h"
#include "pages/region/gis-region-page.h"
#include "pages/keyboard/gis-keyboard-page.h"
#include "pages/display/gis-display-page.h"
#include "pages/endless-eula/gis-endless-eula-page.h"
#include "pages/eulas/gis-eula-pages.h"
#include "pages/network/gis-network-page.h"
#include "pages/timezone/gis-timezone-page.h"
#include "pages/privacy/gis-privacy-page.h"
#include "pages/software/gis-software-page.h"
#include "pages/live-chooser/gis-live-chooser-page.h"
#include "pages/goa/gis-goa-page.h"
#include "pages/account/gis-account-pages.h"
#include "pages/password/gis-password-page.h"
#include "pages/site/gis-site-page.h"
#include "pages/summary/gis-summary-page.h"

#define VENDOR_PAGES_GROUP "pages"
#define VENDOR_PAGES_SKIP_KEY "skip"

static gboolean force_existing_user_mode;

typedef void (*PreparePage) (GisDriver *driver);

typedef struct {
  const gchar *page_id;
  PreparePage prepare_page_func;
  gboolean new_user_only;
} PageData;

#define PAGE(name, new_user_only) { #name, gis_prepare_ ## name ## _page, new_user_only }

static PageData page_table[] = {
  PAGE (branding_welcome, TRUE),
  PAGE (language, FALSE),
  PAGE (live_chooser, TRUE),
#ifdef ENABLE_REGION_PAGE
  PAGE (region,   FALSE),
#endif /* ENABLE_REGION_PAGE */
  PAGE (keyboard, FALSE),
  PAGE (display, TRUE),
  PAGE (eula,     FALSE),
  PAGE (endless_eula, TRUE),
  PAGE (network,  FALSE),
  /* PAGE (privacy,  FALSE), */
  PAGE (timezone, TRUE),
  PAGE (software, TRUE),
  PAGE (goa,      FALSE),
  PAGE (account,  TRUE),
  PAGE (password, TRUE),
  PAGE (site, TRUE),
  PAGE (summary,  FALSE),
  { NULL },
};

#undef PAGE

static gboolean
should_skip_page (GisDriver    *driver,
                  const gchar  *page_id,
                  gchar       **skip_pages)
{
  guint i = 0;

  /* check through our skip pages list for pages we don't want */
  if (skip_pages) {
    while (skip_pages[i]) {
      if (g_strcmp0 (skip_pages[i], page_id) == 0)
        return TRUE;
      i++;
    }
  }
  return FALSE;
}

static gchar **
pages_to_skip_from_file (GisDriver *driver)
{
  GKeyFile *vendor_conf_file = gis_driver_get_vendor_conf_file (driver);
  gchar **skip_pages = NULL;

  /* VENDOR_CONF_FILE points to a keyfile containing vendor customization
   * options. This code will look for options under the "pages" group, and
   * supports the following keys:
   *   - skip (optional): list of pages to be skipped.
   *
   * This is how this file would look on a vendor image:
   *
   *   [pages]
   *   skip=language
   */

  if (vendor_conf_file == NULL)
    return NULL;

  skip_pages = g_key_file_get_string_list (vendor_conf_file, VENDOR_PAGES_GROUP,
                                           VENDOR_PAGES_SKIP_KEY, NULL, NULL);

  return skip_pages;
}

static void
destroy_pages_after (GisAssistant *assistant,
                     GisPage      *page)
{
  GList *pages, *l, *next;

  pages = gis_assistant_get_all_pages (assistant);

  for (l = pages; l != NULL; l = l->next)
    if (l->data == page)
      break;

  l = l->next;
  for (; l != NULL; l = next) {
    next = l->next;
    gtk_widget_destroy (GTK_WIDGET (l->data));
  }
}

static gint
compare_pages_order (gconstpointer a,
                     gconstpointer b,
                     gpointer data)
{
  GHashTable *sort_table = data;
  const gchar *id_a = ((PageData *) a)->page_id;
  const gchar *id_b = ((PageData *) b)->page_id;
  gint order_a = GPOINTER_TO_INT (g_hash_table_lookup (sort_table, id_a));
  gint order_b = GPOINTER_TO_INT (g_hash_table_lookup (sort_table, id_b));

  return order_a - order_b;
}

static void
reorder_pages (GisDriver *driver)
{
  gint num_pages;
  GKeyFile *key_file = gis_driver_get_vendor_conf_file (driver);
  gsize num_ordered_pages = 0;
  g_auto(GStrv) conf_pages_order = NULL;
  g_autoptr(GHashTable) sort_table = NULL; /* (owned) (element-type utf8 uint) */
  g_autoptr(GError) error = NULL;

  if (key_file == NULL)
    return;

  conf_pages_order = g_key_file_get_string_list (key_file, "pages", "order",
                                                 &num_ordered_pages, &error);
  if (conf_pages_order == NULL) {
    /* if we have the pages order key but there was an issue getting it */
    if (!g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND) &&
        !g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND))
      g_warning ("Couldn't get pages order from the conf file: %s", error->message);

    return;
  }

  if (*conf_pages_order == NULL) {
    g_warning ("No pages defined in the pages order from the conf file");
    return;
  }

  /* sanitize the number of pages to be reordered; this should not be frequently
   * reached as it's extremely unlikely to have that many pages */
  if (num_ordered_pages > G_MAXINT) {
    g_warning ("Too many pages to be reordered from the conf file... skipping!");
    return;
  }

  /* assign the same sort weight to every page (this will make sure) that any
   * page to be sorted differently will show up before pages that aren't in the
   * custom sorting list */
  sort_table = g_hash_table_new (g_str_hash, g_str_equal);

  for (num_pages = 0; page_table[num_pages].page_id != NULL; ++num_pages) {
    g_hash_table_insert (sort_table, (gchar *) page_table[num_pages].page_id,
                         GINT_TO_POINTER (G_MAXINT));
  }

  /* assign the new sort weights from the conf file */
  for (guint i = 0; conf_pages_order[i] != NULL; ++i) {
    gchar *page_id = conf_pages_order[i];
    if (!g_hash_table_contains (sort_table, page_id)) {
      g_warning ("No page with id '%s' found while sorting the pages!", page_id);
      continue;
    }

    g_hash_table_insert (sort_table, page_id, GINT_TO_POINTER (i));
  }

  /* use a stable sort algorithm (this implementation is stable since
   * glib 2.32) */
  g_qsort_with_data (page_table,
                     num_pages,
                     sizeof (PageData),
                     compare_pages_order,
                     sort_table);

  g_debug ("Pages have been reordered: ");
  for (guint i = 0; i < num_pages; ++i) {
    g_debug (" - %s", page_table[i].page_id);
  }
}

static void
rebuild_pages_cb (GisDriver *driver)
{
  PageData *page_data;
  GisAssistant *assistant;
  GisPage *current_page;
  gchar **skip_pages;
  gboolean is_new_user;

  assistant = gis_driver_get_assistant (driver);
  current_page = gis_assistant_get_current_page (assistant);

  skip_pages = pages_to_skip_from_file (driver);

  page_data = page_table;

  if (current_page != NULL) {
    destroy_pages_after (assistant, current_page);

    for (page_data = page_table; page_data->page_id != NULL; ++page_data)
      if (g_str_equal (page_data->page_id, GIS_PAGE_GET_CLASS (current_page)->page_id))
        break;

    ++page_data;
  }

  is_new_user = (gis_driver_get_mode (driver) == GIS_DRIVER_MODE_NEW_USER);
  for (; page_data->page_id != NULL; ++page_data) {
    if (page_data->new_user_only && !is_new_user)
      continue;

    if (should_skip_page (driver, page_data->page_id, skip_pages))
      continue;

    page_data->prepare_page_func (driver);
  }

  g_strfreev (skip_pages);
}

static gboolean
is_running_as_user (const gchar *username)
{
  struct passwd pw, *pwp;
  char buf[4096];

  getpwnam_r (username, &pw, buf, sizeof (buf), &pwp);
  if (pwp == NULL)
    return FALSE;

  return pw.pw_uid == getuid ();
}

static GisDriverMode
get_mode (void)
{
  if (force_existing_user_mode)
    return GIS_DRIVER_MODE_EXISTING_USER;
  else
    return GIS_DRIVER_MODE_NEW_USER;
}

int
main (int argc, char *argv[])
{
  GisDriver *driver;
  int status;
  GOptionContext *context;
  GisDriverMode mode;

  GOptionEntry entries[] = {
    { "existing-user", 0, 0, G_OPTION_ARG_NONE, &force_existing_user_mode,
      _("Force existing user mode"), NULL },
    { NULL }
  };

  g_unsetenv ("GIO_USE_VFS");

  context = g_option_context_new (_("— GNOME initial setup"));
  g_option_context_add_main_entries (context, entries, NULL);

  g_option_context_parse (context, &argc, &argv, NULL);

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* If initial-setup has been automatically launched and this is the Shared
     Account user, just quit quietly */
  if (is_running_as_user ("shared")) {
    gis_ensure_stamp_files ();
    return EXIT_SUCCESS;
  }

  /* Upstream has "existing user" mode for new user accounts, but we don't
     want that in Endless, so skip it. In the future, we should be launching
     the tutorial right from here, once it's again available. */
  if (get_mode () == GIS_DRIVER_MODE_EXISTING_USER) {
    gis_ensure_stamp_files ();
    return EXIT_SUCCESS;
  }

#ifdef HAVE_CHEESE
  cheese_gtk_init (NULL, NULL);
#endif

  gtk_init (&argc, &argv);
  ev_init ();

  mode = get_mode ();

  /* When we are running as the gnome-initial-setup user we
   * dont have a normal user session and need to initialize
   * the keyring manually so that we can pass the credentials
   * along to the new user in the handoff.
   */
  if (mode == GIS_DRIVER_MODE_NEW_USER)
    gis_ensure_login_keyring ();

  driver = gis_driver_new (mode);

  reorder_pages (driver);

  g_signal_connect (driver, "rebuild-pages", G_CALLBACK (rebuild_pages_cb), NULL);
  status = g_application_run (G_APPLICATION (driver), argc, argv);

  g_object_unref (driver);
  g_option_context_free (context);
  ev_shutdown ();

  return status;
}

void
gis_ensure_stamp_files (void)
{
  gchar *file;
  GError *error = NULL;

  file = g_build_filename (g_get_user_config_dir (), "gnome-initial-setup-done", NULL);
  if (!g_file_set_contents (file, "yes", -1, &error)) {
      g_warning ("Unable to create %s: %s", file, error->message);
      g_clear_error (&error);
  }
  g_free (file);
}

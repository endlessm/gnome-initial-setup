/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Copyright (C) 2008 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Travis Reitter <travis.reitter@endlessm.com>
 */

#include <string.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gis-site-search-entry.h"

/**
 * SECTION:GisSiteSearchEntry
 * @Title: GisSiteSearchEntry
 *
 * A subclass of #GtkSearchEntry that provides autocompletion on
 * #GisSite<!-- -->s
 */

typedef struct {
  GisSite          *site;
  GPtrArray        *sites;
} GisSiteSearchEntryPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisSiteSearchEntry, gis_site_search_entry,
                            GTK_TYPE_SEARCH_ENTRY)

enum {
  PROP_0,

  PROP_SITES_AVAILABLE,
  PROP_SITE,

  LAST_PROP
};

static void set_property (GObject *object, guint prop_id,
                          const GValue *value, GParamSpec *pspec);
static void get_property (GObject *object, guint prop_id,
                          GValue *value, GParamSpec *pspec);

static void set_site_internal (GisSiteSearchEntry *entry,
                               GtkTreeIter        *iter,
                               GisSite            *site);

static void fill_store (GisSite *site, GtkListStore *store);

enum STORE_COL
{
  GIS_SITE_SEARCH_ENTRY_COL_DISPLAY_NAME = 0,
  GIS_SITE_SEARCH_ENTRY_COL_SITE,
  GIS_SITE_SEARCH_ENTRY_COL_LOCAL_COMPARE_NAME,
};

static gboolean matcher (GtkEntryCompletion *completion, const char *key,
                         GtkTreeIter *iter, gpointer user_data);
static gboolean match_selected (GtkEntryCompletion *completion,
                                GtkTreeModel       *model,
                                GtkTreeIter        *iter,
                                gpointer            entry);
static void     entry_changed (GisSiteSearchEntry *entry);

static void
gis_site_search_entry_init (GisSiteSearchEntry *entry)
{
  GtkEntryCompletion *completion;

  completion = gtk_entry_completion_new ();

  gtk_entry_completion_set_popup_set_width (completion, FALSE);
  gtk_entry_completion_set_text_column (completion,
                                        GIS_SITE_SEARCH_ENTRY_COL_DISPLAY_NAME);
  gtk_entry_completion_set_match_func (completion, matcher, NULL, NULL);
  gtk_entry_completion_set_inline_completion (completion, TRUE);

  g_signal_connect (completion, "match-selected", G_CALLBACK (match_selected),
                    entry);

  gtk_entry_set_completion (GTK_ENTRY (entry), completion);
  g_object_unref (completion);

  g_signal_connect (entry, "changed",
                    G_CALLBACK (entry_changed), NULL);
}

static void
finalize (GObject *object)
{
  GisSiteSearchEntryPrivate *priv = gis_site_search_entry_get_instance_private (
    GIS_SITE_SEARCH_ENTRY (object));

  g_clear_object (&priv->site);
  g_clear_pointer (&priv->sites, g_ptr_array_unref);

  G_OBJECT_CLASS (gis_site_search_entry_parent_class)->finalize (object);
}

static void
gis_site_search_entry_class_init (
        GisSiteSearchEntryClass *site_search_entry_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (site_search_entry_class);

  object_class->finalize = finalize;
  object_class->set_property = set_property;
  object_class->get_property = get_property;

  /* properties */
  g_object_class_install_property (
      object_class, PROP_SITES_AVAILABLE,
      g_param_spec_boxed (GIS_SITE_SEARCH_ENTRY_PROP_SITES_AVAILABLE,
                          "Full site list",
                          "The array of sites available",
                          G_TYPE_PTR_ARRAY,
                          G_PARAM_WRITABLE));

  g_object_class_install_property (
      object_class, PROP_SITE,
      g_param_spec_object (GIS_SITE_SEARCH_ENTRY_PROP_SITE,
                           "Site",
                           "The selected site",
                           GIS_TYPE_SITE,
                           G_PARAM_READWRITE));
}

static void
set_property_sites_available (GisSiteSearchEntry *self,
                              const GValue       *value)
{
  GisSiteSearchEntryPrivate *priv = gis_site_search_entry_get_instance_private (
    self);
  GtkListStore *store = NULL;
  GtkEntryCompletion *completion;

  store = gtk_list_store_new (3, G_TYPE_STRING, GIS_TYPE_SITE, G_TYPE_STRING);

  g_clear_pointer (&priv->sites, g_ptr_array_unref);
  priv->sites = g_value_dup_boxed (value);
  for (int i = 0; priv->sites && i < priv->sites->len; i++)
    {
      GisSite *site = g_ptr_array_index (priv->sites, i);
      fill_store (site, store);
    }

  completion = gtk_entry_get_completion (GTK_ENTRY (self));
  gtk_entry_completion_set_match_func (completion, matcher, NULL, NULL);
  gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
}

static void
set_property (GObject *object, guint prop_id,
              const GValue *value, GParamSpec *pspec)
{
  switch (prop_id)
    {
      case PROP_SITES_AVAILABLE:
        set_property_sites_available (GIS_SITE_SEARCH_ENTRY (object), value);
        break;
      case PROP_SITE:
        gis_site_search_entry_set_site (GIS_SITE_SEARCH_ENTRY (object),
                                        g_value_get_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
get_property (GObject *object, guint prop_id,
              GValue *value, GParamSpec *pspec)
{
  GisSiteSearchEntryPrivate *priv = gis_site_search_entry_get_instance_private (
    GIS_SITE_SEARCH_ENTRY (object));

  switch (prop_id)
    {
      case PROP_SITE:
        g_value_set_boxed (value, priv->site);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
entry_changed (GisSiteSearchEntry *entry)
{
  const gchar *text = gtk_entry_get_text (GTK_ENTRY (entry));

  if (!text || text[0] == '\0')
    set_site_internal (entry, NULL, NULL);
}

static void
set_site_internal (GisSiteSearchEntry *entry,
                   GtkTreeIter        *iter,
                   GisSite            *site)
{
  GisSiteSearchEntryPrivate *priv =
      gis_site_search_entry_get_instance_private (entry);

  g_clear_object (&priv->site);

  g_assert (iter == NULL || site == NULL);

  if (iter)
    {
      char *name;
      GtkEntryCompletion *completion = gtk_entry_get_completion (
          GTK_ENTRY (entry));
      GtkTreeModel *model = gtk_entry_completion_get_model (completion);

      gtk_tree_model_get (model, iter,
                          GIS_SITE_SEARCH_ENTRY_COL_DISPLAY_NAME, &name,
                          GIS_SITE_SEARCH_ENTRY_COL_SITE, &priv->site,
                          -1);
      gtk_entry_set_text (GTK_ENTRY (entry), name);
      g_free (name);
    }
  else if (site)
    {
      gchar *display_name = gis_site_get_display_name (site);
      priv->site = g_object_ref (site);
      gtk_entry_set_text (GTK_ENTRY (entry), display_name);

      g_free (display_name);
    }
  else
    {
      gtk_entry_set_text (GTK_ENTRY (entry), "");
    }

  gtk_editable_set_position (GTK_EDITABLE (entry), -1);
  g_object_notify (G_OBJECT (entry), GIS_SITE_SEARCH_ENTRY_PROP_SITE);
}

/**
 * gis_site_search_entry_set_site:
 * @entry: a #GisSiteSearchEntry
 * @site: (allow-none): a #GisSite in @entry, or %NULL to clear @entry
 *
 * Sets @entry's site to @site, and update the text of the entry accordingly.
 **/
void
gis_site_search_entry_set_site (GisSiteSearchEntry *entry,
                                GisSite            *site)
{
  g_return_if_fail (GIS_IS_SITE_SEARCH_ENTRY (entry));

  set_site_internal (entry, NULL, site);
}

/**
 * gis_site_search_entry_get_site:
 * @entry: a #GisSiteSearchEntry
 *
 * Gets the site that was set by a previous call to
 * gis_site_search_entry_set_site() or was selected by the user.
 *
 * Return value: (transfer none) (allow-none): the selected site or %NULL if no
 * site is selected.
 **/
GisSite *
gis_site_search_entry_get_site (GisSiteSearchEntry *entry)
{
  GisSiteSearchEntryPrivate *priv;

  g_return_val_if_fail (GIS_IS_SITE_SEARCH_ENTRY (entry), NULL);

  priv = gis_site_search_entry_get_instance_private (entry);

  return priv->site;
}

static char *
find_word (const char *full_name, const char *word, int word_len,
           gboolean whole_word, gboolean is_first_word)
{
  char *p;

  if (word == NULL || *word == '\0')
    return NULL;

  p = (char *)full_name - 1;
  while ((p = strchr (p + 1, *word)))
    {
      if (strncmp (p, word, word_len) != 0)
        continue;

      if (p > (char *)full_name)
        {
          char *prev = g_utf8_prev_char (p);

          /* Make sure p points to the start of a word */
          if (g_unichar_isalpha (g_utf8_get_char (prev)))
            continue;

          /* If we're matching the first word of the key, it has to
           * match the first word of the field.
           * Eg, it either matches the start of the string
           * (which we already know it doesn't at this point) or
           * it is preceded by the string ", " or "(" (which isn't actually
           * a perfect test.)
           */
          if (is_first_word)
            {
              if (prev == (char *)full_name ||
                  ((prev - 1 <= full_name && strncmp (prev - 1, ", ", 2) != 0)
                    && *prev != '('))
                continue;
            }
        }

      if (whole_word && g_unichar_isalpha (g_utf8_get_char (p + word_len)))
        continue;

      return p;
    }

  return NULL;
}

static gboolean
match_compare_name (const char *key, const char *name)
{
  gboolean is_first_word = TRUE;
  size_t len;

  /* Ignore whitespace before the string */
  key += strspn (key, " ");

  /* All but the last word in KEY must match a full word from NAME,
   * in order (but possibly skipping some words from NAME).
   */
  len = strcspn (key, " ");
  while (key[len])
    {
      name = find_word (name, key, len, TRUE, is_first_word);
      if (!name)
        return FALSE;

      key += len;
      while (*key && !g_unichar_isalnum (g_utf8_get_char (key)))
        key = g_utf8_next_char (key);
      while (*name && !g_unichar_isalnum (g_utf8_get_char (name)))
        name = g_utf8_next_char (name);

      len = strcspn (key, " ");
      is_first_word = FALSE;
    }

  /* The last word in KEY must match a prefix of a following word in NAME */
  if (len == 0)
    return TRUE;

  /* if we get here, key[len] == 0 */
  g_assert (len == strlen (key));

  return find_word (name, key, len, FALSE, is_first_word) != NULL;
}

static gboolean
matcher (GtkEntryCompletion *completion, const char *key,
         GtkTreeIter *iter, gpointer user_data)
{
  char *compare_name;
  gboolean match;

  gtk_tree_model_get (gtk_entry_completion_get_model (completion), iter,
                      GIS_SITE_SEARCH_ENTRY_COL_LOCAL_COMPARE_NAME, &compare_name,
                      -1);

  match = match_compare_name (key, compare_name);

  g_free (compare_name);

  return match;
}

static gboolean
match_selected (GtkEntryCompletion *completion,
                GtkTreeModel       *model,
                GtkTreeIter        *iter,
                gpointer            entry)
{
  set_site_internal (entry, iter, NULL);

  return TRUE;
}

static void
fill_store (GisSite *site, GtkListStore *store)
{
  char *display_name;
  char *normalized;
  char *compare_name;

  display_name = gis_site_get_display_name (site);
  normalized = g_utf8_normalize (display_name, -1, G_NORMALIZE_ALL);
  compare_name = g_utf8_casefold (normalized, -1);

  gtk_list_store_insert_with_values (store, NULL, -1,
                                     GIS_SITE_SEARCH_ENTRY_COL_DISPLAY_NAME, display_name,
                                     GIS_SITE_SEARCH_ENTRY_COL_SITE, site,
                                     GIS_SITE_SEARCH_ENTRY_COL_LOCAL_COMPARE_NAME, compare_name,
                                     -1);

  g_free (normalized);
  g_free (compare_name);
  g_free (display_name);
}

/**
 * gis_site_search_entry_new:
 * @sites: a #GPtrArray of #GisSite objects to serve as the data source for this
 * search widget. This array and its content must not be modified after this
 * function is called.
 *
 * Creates a new #GisSiteSearchEntry.
 *
 * To be nofified when the user makes a selection, add a signal handler for
 * "notify::site".
 *
 * Return value: the new #GisSiteSearchEntry
 **/
GtkWidget *
gis_site_search_entry_new (GPtrArray *sites)
{
  return g_object_new (GIS_TYPE_SITE_SEARCH_ENTRY,
                       GIS_SITE_SEARCH_ENTRY_PROP_SITES_AVAILABLE, sites,
                       NULL);
}

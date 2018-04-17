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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Travis Reitter <travis.reitter@endlessm.com>
 */

#include "gis-site.h"

/**
 * GisSite:
 *
 * All the details about a given deployment site needed for the purposes of
 * grouping computers at a given site for the purposes of metrics.
 *
 * #GisSite is an opaque data structure and can only be accessed
 * using the following functions.
 */

struct _GisSite
{
  GObject parent;
};

struct _GisSitePrivate
{
  gchar             *id;

  /* eg, name of a school */
  gchar             *facility;

  /* eg, "123 Main Street" */
  gchar             *street;

  /* city, township, or equivalent */
  gchar             *locality;

  /* municipal division between a county and country such as a state in the
   * United States or department in Colombia
   */
  gchar             *region;

  gchar             *country;
};
typedef struct _GisSitePrivate GisSitePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisSite, gis_site, G_TYPE_OBJECT)

static void
gis_site_finalize (GObject *object)
{
  GisSite *site = GIS_SITE (object);
  GisSitePrivate *priv = gis_site_get_instance_private (site);
  g_free (priv->id);
  g_free (priv->facility);
  g_free (priv->street);
  g_free (priv->locality);
  g_free (priv->region);
  g_free (priv->country);

  G_OBJECT_CLASS (gis_site_parent_class)->finalize (object);
}

static void
gis_site_init (GisSite *self)
{
}

static void
gis_site_class_init (GisSiteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gis_site_finalize;
}

/**
 * gis_site_new:
 * @id: (nullable): the ID, if any, the site's admins use for this site (such as
 *                  a school ID).
 * @facility: (nullable): name of the site (eg, the name of a school)
 * @street: (nullable): street address (eg, "123 Main Street")
 * @locality: (nullable): city, township, or equivalent
 * @region: (nullable): municipal division between a county and country such as
 *                      a state in the United States or department in Colombia
 * @country: (nullable): country
 *
 * Creates a new #GisSite instance.
 *
 * All fields are nullable but at least the @id, @facility, or @locality should
 * be set for this #GisSite to be useful.
 *
 * Returns: a new #GisSite instance
 **/
GisSite *
gis_site_new (const gchar *id,
              const gchar *facility,
              const gchar *street,
              const gchar *locality,
              const gchar *region,
              const gchar *country)
{
  GisSite *self;
  GisSitePrivate *priv;

  self = g_object_new (GIS_TYPE_SITE, NULL);

  priv = gis_site_get_instance_private (self);
  priv->id = g_strdup (id);
  priv->facility = g_strdup (facility);
  priv->street = g_strdup (street);
  priv->locality = g_strdup (locality);
  priv->region = g_strdup (region);
  priv->country = g_strdup (country);

  return self;
}

/**
 * gis_site_get_id:
 * @site: a #GisSite
 *
 * Gets the unique identifier for @site.
 *
 * Returns: (transfer none): the identifier or %NULL
 **/
const gchar *
gis_site_get_id (GisSite *site)
{
  GisSitePrivate *priv = gis_site_get_instance_private (site);

  g_return_val_if_fail (GIS_IS_SITE (site), NULL);

  return priv->id;
}

/**
 * gis_site_get_facility:
 * @site: a #GisSite
 *
 * Gets the name of the facility (eg, a school) for @site.
 *
 * Returns: (transfer none): the facility or %NULL
 **/
const gchar *
gis_site_get_facility (GisSite *site)
{
  GisSitePrivate *priv = gis_site_get_instance_private (site);

  g_return_val_if_fail (GIS_IS_SITE (site), NULL);

  return priv->facility;
}

/**
 * gis_site_get_street:
 * @site: a #GisSite
 *
 * Gets the street address (eg, 123 Main Street) for @site.
 *
 * Returns: (transfer none): the street address or %NULL
 **/
const gchar *
gis_site_get_street (GisSite *site)
{
  GisSitePrivate *priv = gis_site_get_instance_private (site);

  g_return_val_if_fail (GIS_IS_SITE (site), NULL);

  return priv->street;
}

/**
 * gis_site_get_locality:
 * @site: a #GisSite
 *
 * Gets the locality (city or equivalent) for @site.
 *
 * Returns: (transfer none): the locality or %NULL
 **/
const gchar *
gis_site_get_locality (GisSite *site)
{
  GisSitePrivate *priv = gis_site_get_instance_private (site);

  g_return_val_if_fail (GIS_IS_SITE (site), NULL);

  return priv->locality;
}

/**
 * gis_site_get_region:
 * @site: a #GisSite
 *
 * Gets the region (equivalent to a state in the US or department in Colombia)
 * for @site.
 *
 * Returns: (transfer none): the region or %NULL
 **/
const gchar *
gis_site_get_region (GisSite *site)
{
  GisSitePrivate *priv = gis_site_get_instance_private (site);

  g_return_val_if_fail (GIS_IS_SITE (site), NULL);

  return priv->region;
}

/**
 * gis_site_get_country:
 * @site: a #GisSite
 *
 * Gets the country for @site.
 *
 * Returns: (transfer none): the country or %NULL
 **/
const gchar *
gis_site_get_country (GisSite *site)
{
  GisSitePrivate *priv = gis_site_get_instance_private (site);

  g_return_val_if_fail (GIS_IS_SITE (site), NULL);

  return priv->country;
}

static void
string_append_safe (GString     *string,
                    const gchar *field)
{
  if (field && field[0] != '\0')
    g_string_append_printf (string, "%s%s", string->len > 0 ? ", " : "", field);
}

/**
 * Get a string for this #GisSite that's suitable to display to a user.
 *
 * NOTE: this assumes the values provided as arguments to this #GisSite are
 * already translated into the relevant local language (and that it's a single
 * language).
 *
 * @site: a #GisSite
 *
 * Returns: (transfer full): a display name suitable for display (which must be
 * freed) or %NULL if none could be created.
 */
gchar*
gis_site_get_display_name (GisSite *site)
{
  gchar *retval;
  const gchar *id = gis_site_get_id (site);
  const gchar *facility = gis_site_get_facility (site);
  const gchar *street = gis_site_get_street (site);
  const gchar *locality = gis_site_get_locality (site);
  const gchar *region = gis_site_get_region (site);
  const gchar *country = gis_site_get_country (site);

  GString *string = g_string_new ("");

  string_append_safe (string, id);
  string_append_safe (string, facility);
  string_append_safe (string, street);
  string_append_safe (string, locality);
  string_append_safe (string, region);
  string_append_safe (string, country);

  retval = g_string_free (string, FALSE);

  if (g_str_equal (retval, ""))
    g_clear_pointer (&retval, g_free);

  return retval;
}


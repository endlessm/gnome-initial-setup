/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Endless Mobile, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 * Written by:
 *     Mario Sanchez Prada <mario@endlessm.com>
 */

#include "config.h"

#include "gis-page-util.h"

#include <errno.h>
#include <gio/gio.h>
#include <glib.h>
#include <sys/types.h>
#include <sys/xattr.h>

#define EOS_IMAGE_VERSION_XATTR "user.eos-image-version"

static char *
get_image_version (const char *path,
                   GError **error)
{
  ssize_t attrsize;
  g_autofree gchar *value = NULL;

  g_return_val_if_fail (path != NULL, NULL);

  attrsize = getxattr (path, EOS_IMAGE_VERSION_XATTR, NULL, 0);
  if (attrsize >= 0)
    {
      value = g_malloc (attrsize + 1);
      value[attrsize] = 0;

      attrsize = getxattr (path, EOS_IMAGE_VERSION_XATTR, value,
                           attrsize);
    }

  if (attrsize >= 0)
    {
      return g_steal_pointer (&value);
    }
  else
    {
      int errsv = errno;
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                   "Error examining " EOS_IMAGE_VERSION_XATTR " on %s: %s",
                   path, g_strerror (errsv));
      return NULL;
    }
}

gchar *
gis_page_util_get_image_version (void)
{
  g_autoptr(GError) error_sysroot = NULL;
  g_autoptr(GError) error_root = NULL;
  char *image_version = get_image_version ("/sysroot", &error_sysroot);

  if (image_version == NULL)
    image_version = get_image_version ("/", &error_root);

  if (image_version == NULL)
    {
      g_warning ("%s", error_sysroot->message);
      g_warning ("%s", error_root->message);
    }

  return image_version;
}

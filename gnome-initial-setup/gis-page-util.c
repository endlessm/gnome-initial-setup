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

#include <gio/gio.h>
#include <glib.h>

#define EOS_IMAGE_VERSION_XATTR "xattr::eos-image-version"

static char *
get_image_version (const char *path,
                   GError **error)
{
  g_autoptr(GFile) file = g_file_new_for_path (path);
  g_autoptr(GFileInfo) info = NULL;

  info = g_file_query_info (file,
                            EOS_IMAGE_VERSION_XATTR,
                            G_FILE_QUERY_INFO_NONE,
                            NULL /* cancellable */,
                            error);

  if (info == NULL)
    return NULL;

  return g_strdup (g_file_info_get_attribute_string (info, EOS_IMAGE_VERSION_XATTR));
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
      if (error_sysroot != NULL)
        g_warning ("%s", error_sysroot->message);
      else if (error_root != NULL)
        g_warning ("%s", error_root->message);
      else
        g_debug ("Neither /sysroot nor / had " EOS_IMAGE_VERSION_XATTR);
    }

  return image_version;
}

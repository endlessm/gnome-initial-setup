/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2017 Endless Mobile, Inc.
 *
 * Authors:
 *   Joaquim Rocha <jrocha@endlessm.com>
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
 */

#define _GNU_SOURCE

#include <string.h>
#include <dlfcn.h>

#define WRAPPER_LOCALE_BUF_LEN 32

typedef char *(*setlocale_fn)(int category, const char *locale);
static setlocale_fn setlocale_orig_func = NULL;

static char wrapper_locale_current[WRAPPER_LOCALE_BUF_LEN] = "\0";

char *
setlocale (int category, const char *locale)
{
  setlocale_orig_func = (setlocale_fn) dlsym (RTLD_NEXT, "setlocale");

  if (locale != NULL) {
    char *setup_locale = setlocale_orig_func (category, locale);

    /* if the original setlocale function failed to set up the new locale, then
     * we also fail */
    if (setup_locale == NULL)
      return NULL;

    strncpy (wrapper_locale_current, setup_locale, WRAPPER_LOCALE_BUF_LEN);
  } else if (strlen (wrapper_locale_current) == 0) {
    return setlocale_orig_func (category, NULL);
  }
  return wrapper_locale_current;
}

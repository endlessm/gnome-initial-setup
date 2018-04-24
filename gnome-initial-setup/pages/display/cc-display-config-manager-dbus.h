/*
 * Copyright (C) 2017  Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef _CC_DISPLAY_CONFIG_MANAGER_DBUS_H
#define _CC_DISPLAY_CONFIG_MANAGER_DBUS_H

#include <glib-object.h>

#include "cc-display-config-manager.h"

G_BEGIN_DECLS

#define CC_TYPE_DISPLAY_CONFIG_MANAGER_DBUS (cc_display_config_manager_dbus_get_type ())
G_DECLARE_FINAL_TYPE (CcDisplayConfigManagerDBus, cc_display_config_manager_dbus,
                      CC, DISPLAY_CONFIG_MANAGER_DBUS, CcDisplayConfigManager)

CcDisplayConfigManager * cc_display_config_manager_dbus_new (void);

G_END_DECLS

#endif /* _CC_DISPLAY_CONFIG_MANAGER_DBUS_H */

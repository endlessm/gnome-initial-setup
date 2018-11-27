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

/* Account page {{{1 */

#define PAGE_ID "account"

#include "config.h"
#include "gis-page-util.h"
#include "account-resources.h"
#include "gis-account-page.h"
#include "gis-account-page-local.h"
#include "gis-account-page-enterprise.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#define CONFIG_ACCOUNT_GROUP "page.account"
#define CONFIG_ACCOUNT_SHOW_PASSWORD_SWITCH_KEY "show-password-switch"

struct _GisAccountPagePrivate
{
  GtkWidget *page_local;
  GtkWidget *page_enterprise;

  GtkWidget *page_toggle;
  GtkWidget *stack;
  GtkAccelGroup *accel_group;

  UmAccountMode mode;
};
typedef struct _GisAccountPagePrivate GisAccountPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisAccountPage, gis_account_page, GIS_TYPE_PAGE);

static void
set_password_page_visibility (GisAccountPage *page,
                              gboolean        visible)
{
  GisAssistant *assistant = gis_driver_get_assistant (GIS_PAGE (page)->driver);
  GisPage *password_page = gis_assistant_get_page_by_id (assistant, "password");

  gtk_widget_set_visible (GTK_WIDGET (password_page), visible);
}

static void
enterprise_apply_complete (GisPage  *dummy,
                           gboolean  valid,
                           gpointer  user_data)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (user_data);
  set_password_page_visibility (page, FALSE);
  gis_driver_set_username (GIS_PAGE (page)->driver, NULL);
  gis_page_apply_complete (GIS_PAGE (page), valid);
}

static gboolean
page_validate (GisAccountPage *page)
{
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (page);
  gboolean passwordless, local_valid;

  switch (priv->mode) {
  case UM_LOCAL:
    local_valid = gis_account_page_local_validate (GIS_ACCOUNT_PAGE_LOCAL (priv->page_local));
    if (local_valid) {
      passwordless = gis_account_page_local_is_passwordless (GIS_ACCOUNT_PAGE_LOCAL (priv->page_local));
      set_password_page_visibility (page, !passwordless);
    }
    return local_valid;
  case UM_ENTERPRISE:
    return gis_account_page_enterprise_validate (GIS_ACCOUNT_PAGE_ENTERPRISE (priv->page_enterprise));
  default:
    g_assert_not_reached ();
  }
}

static void
update_page_validation (GisAccountPage *page)
{
  gis_page_set_complete (GIS_PAGE (page), page_validate (page));
}

static void
on_validation_changed (gpointer        page_area,
                       GisAccountPage *page)
{
  update_page_validation (page);
}


static void
hide_password_switch_if_needed (GisAccountPage *page)
{
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (page);
  gboolean show_password_switch;

  /* check the conf file to see if the password switch should be shown/hidden */
  show_password_switch = gis_driver_conf_get_boolean (GIS_PAGE (page)->driver,
                                                      CONFIG_ACCOUNT_GROUP,
                                                      CONFIG_ACCOUNT_SHOW_PASSWORD_SWITCH_KEY,
                                                      TRUE);
  gis_account_page_local_show_password_switch (GIS_ACCOUNT_PAGE_LOCAL (priv->page_local),
                                               show_password_switch);
}

static void
set_mode (GisAccountPage *page,
          UmAccountMode   mode)
{
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (page);

  if (priv->mode == mode)
    return;

  priv->mode = mode;
  gis_driver_set_account_mode (GIS_PAGE (page)->driver, mode);

  switch (mode)
    {
    case UM_LOCAL:
      gtk_stack_set_visible_child (GTK_STACK (priv->stack), priv->page_local);
      gis_account_page_local_shown (GIS_ACCOUNT_PAGE_LOCAL (priv->page_local));
      hide_password_switch_if_needed (page);
      break;
    case UM_ENTERPRISE:
      gtk_stack_set_visible_child (GTK_STACK (priv->stack), priv->page_enterprise);
      gis_account_page_enterprise_shown (GIS_ACCOUNT_PAGE_ENTERPRISE (priv->page_enterprise));
      break;
    default:
      g_assert_not_reached ();
    }

  update_page_validation (page);
}

static void
switch_login_mode (GisAccountPage *page)
{
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (page);
  if (priv->mode == UM_LOCAL)
    set_mode (page, UM_ENTERPRISE);
  else
    set_mode (page, UM_LOCAL);
}

static gboolean
gis_account_page_apply (GisPage *gis_page,
                        GCancellable *cancellable)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (gis_page);
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (page);

  if (gis_driver_is_hack (gis_page->driver))
    gis_page_util_set_endlessm_metrics (TRUE);

  switch (priv->mode) {
  case UM_LOCAL:
    return gis_account_page_local_apply (GIS_ACCOUNT_PAGE_LOCAL (priv->page_local), gis_page);
  case UM_ENTERPRISE:
    return gis_account_page_enterprise_apply (GIS_ACCOUNT_PAGE_ENTERPRISE (priv->page_enterprise), cancellable,
                                              enterprise_apply_complete, page);
  default:
    g_assert_not_reached ();
    break;
  }
}

static void
gis_account_page_save_data (GisPage *gis_page)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (gis_page);
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (page);

  switch (priv->mode) {
  case UM_LOCAL:
    gis_account_page_local_create_user (GIS_ACCOUNT_PAGE_LOCAL (priv->page_local));
    break;
  case UM_ENTERPRISE:
    break;
  default:
    g_assert_not_reached ();
  }
}

static void
gis_account_page_shown (GisPage *gis_page)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (gis_page);
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (page);

  gis_account_page_local_shown (GIS_ACCOUNT_PAGE_LOCAL (priv->page_local));
}

static void
on_local_user_created (GtkWidget      *page_local,
                       ActUser        *user,
                       char           *password,
                       GisAccountPage *page)
{
  const gchar *language;

  language = gis_driver_get_user_language (GIS_PAGE (page)->driver);
  if (language)
    act_user_set_language (user, language);

  gis_driver_set_user_permissions (GIS_PAGE (page)->driver, user, password);
}

static void
on_local_page_confirmed (GisAccountPageLocal *local,
                         GisAccountPage      *page)
{
  gis_assistant_next_page (gis_driver_get_assistant (GIS_PAGE (page)->driver));
}

static void
on_local_user_cached (GtkWidget      *page_local,
                      ActUser        *user,
                      char           *password,
                      GisAccountPage *page)
{
  const gchar *language;

  language = gis_driver_get_user_language (GIS_PAGE (page)->driver);
  if (language)
    act_user_set_language (user, language);

  gis_driver_set_user_permissions (GIS_PAGE (page)->driver, user, password);
}

static void
on_shared_user_created (GtkWidget       *page_local,
                        ActUser         *user,
                        char            *password,
                        GisAccountPage  *page)
{
    const gchar *language;

    language = gis_driver_get_user_language (GIS_PAGE (page)->driver);
    if (language)
        act_user_set_language (user, language);

    gis_driver_set_user_permissions (GIS_PAGE (page)->driver, user, password);
}

static void
gis_account_page_constructed (GObject *object)
{
  GisAccountPage *page = GIS_ACCOUNT_PAGE (object);
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (page);

  G_OBJECT_CLASS (gis_account_page_parent_class)->constructed (object);

  g_signal_connect (priv->page_local, "validation-changed",
                    G_CALLBACK (on_validation_changed), page);
  g_signal_connect (priv->page_local, "user-created",
                    G_CALLBACK (on_local_user_created), page);
  g_signal_connect (priv->page_local, "confirm",
                    G_CALLBACK (on_local_page_confirmed), page);

  g_signal_connect (priv->page_local, "shared-user-created",
                    G_CALLBACK (on_shared_user_created), page);

  g_signal_connect (priv->page_enterprise, "validation-changed",
                    G_CALLBACK (on_validation_changed), page);
  g_signal_connect (priv->page_enterprise, "user-cached",
                    G_CALLBACK (on_local_user_cached), page);

  update_page_validation (page);

  priv->accel_group = gtk_accel_group_new ();
  GClosure *closure = g_cclosure_new_swap (G_CALLBACK (switch_login_mode), page, NULL);

  /* Use Ctrl+Alt+e to activate the enterprise login mode */
  gtk_accel_group_connect (priv->accel_group, GDK_KEY_e, GDK_CONTROL_MASK | GDK_MOD1_MASK, 0, closure);

  g_object_bind_property (priv->page_enterprise, "visible", priv->page_toggle, "visible", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  /* force a refresh by setting to an invalid value */
  priv->mode = NUM_MODES;
  set_mode (page, UM_LOCAL);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_account_page_locale_changed (GisPage *page)
{
  if (gis_driver_is_hack (page->driver))
    gis_page_set_forward_text (GIS_PAGE (page), _("_Accept and Continue"));
  gis_page_set_title (GIS_PAGE (page), _("About You"));
}

static GtkAccelGroup *
gis_account_page_get_accel_group (GisPage *page)
{
  GisAccountPage *account_page = GIS_ACCOUNT_PAGE (page);
  GisAccountPagePrivate *priv = gis_account_page_get_instance_private (account_page);

  return priv->accel_group;
}

static void
gis_account_page_class_init (GisAccountPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-account-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPage, page_local);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPage, page_enterprise);

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPage, page_toggle);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisAccountPage, stack);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_account_page_locale_changed;
  page_class->get_accel_group = gis_account_page_get_accel_group;
  page_class->apply = gis_account_page_apply;
  page_class->save_data = gis_account_page_save_data;
  page_class->shown = gis_account_page_shown;
  object_class->constructed = gis_account_page_constructed;
}

static void
gis_account_page_init (GisAccountPage *page)
{
  g_resources_register (account_get_resource ());
  g_type_ensure (GIS_TYPE_ACCOUNT_PAGE_LOCAL);
  g_type_ensure (GIS_TYPE_ACCOUNT_PAGE_ENTERPRISE);

  gtk_widget_init_template (GTK_WIDGET (page));
}

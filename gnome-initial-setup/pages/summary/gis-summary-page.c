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

/* Summary page {{{1 */

#define PAGE_ID "summary"

#include "config.h"
#include "summary-resources.h"
#include "gis-summary-page.h"

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <errno.h>

#include <act/act-user-manager.h>

#include <gdm/gdm-client.h>

#define SERVICE_NAME "gdm-password"

struct _GisSummaryPagePrivate {
  GtkWidget *start_button;
  GtkWidget *start_button_label;
  GtkWidget *hack_box;
  GtkWidget *tagline;
  GtkWidget *title;
  GtkWidget *warning_box;

  ActUser *user_account;
  const gchar *user_password;

  GDBusProxy *clippy_proxy;
  gulong clippy_proxy_signal_id;
};
typedef struct _GisSummaryPagePrivate GisSummaryPagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisSummaryPage, gis_summary_page, GIS_TYPE_PAGE);

static gboolean
connect_to_gdm (GdmGreeter      **greeter,
                GdmUserVerifier **user_verifier)
{
  GdmClient *client;

  GError *error = NULL;
  gboolean res = FALSE;

  client = gdm_client_new ();

  *greeter = gdm_client_get_greeter_sync (client, NULL, &error);
  if (error != NULL)
    goto out;

  *user_verifier = gdm_client_get_user_verifier_sync (client, NULL, &error);
  if (error != NULL)
    goto out;

  res = TRUE;

 out:
  if (error != NULL) {
    g_warning ("Failed to open connection to GDM: %s", error->message);
    g_error_free (error);
  }

  return res;
}

static void
request_info_query (GisSummaryPage  *page,
                    GdmUserVerifier *user_verifier,
                    const char      *question,
                    gboolean         is_secret)
{
  /* TODO: pop up modal dialog */
  g_debug ("user verifier asks%s question: %s",
           is_secret ? " secret" : "",
           question);
}

static void
on_info (GdmUserVerifier *user_verifier,
         const char      *service_name,
         const char      *info,
         GisSummaryPage  *page)
{
  g_debug ("PAM module info: %s", info);
}

static void
on_problem (GdmUserVerifier *user_verifier,
            const char      *service_name,
            const char      *problem,
            GisSummaryPage  *page)
{
  g_warning ("PAM module error: %s", problem);
}

static void
on_info_query (GdmUserVerifier *user_verifier,
               const char      *service_name,
               const char      *question,
               GisSummaryPage  *page)
{
  request_info_query (page, user_verifier, question, FALSE);
}

static void
on_secret_info_query (GdmUserVerifier *user_verifier,
                      const char      *service_name,
                      const char      *question,
                      GisSummaryPage  *page)
{
  GisSummaryPagePrivate *priv = gis_summary_page_get_instance_private (page);
  gboolean should_send_password = priv->user_password != NULL;

  g_debug ("PAM module secret info query: %s", question);
  if (should_send_password) {
    g_debug ("sending password\n");
    gdm_user_verifier_call_answer_query (user_verifier,
                                         service_name,
                                         priv->user_password,
                                         NULL, NULL, NULL);
    g_clear_pointer (&priv->user_password, (GDestroyNotify) g_free);
  } else {
    request_info_query (page, user_verifier, question, TRUE);
  }
}

static void
on_session_opened (GdmGreeter     *greeter,
                   const char     *service_name,
                   GisSummaryPage *page)
{
  gdm_greeter_call_start_session_when_ready_sync (greeter, service_name,
                                                  TRUE, NULL, NULL);
}

static void
add_uid_file (uid_t uid)
{
  gchar *gis_uid_path;
  gchar *uid_str;
  GError *error = NULL;

  gis_uid_path = g_build_filename (g_get_home_dir (),
                                   "gnome-initial-setup-uid",
                                   NULL);
  uid_str = g_strdup_printf ("%u", uid);

  if (!g_file_set_contents (gis_uid_path, uid_str, -1, &error)) {
      g_warning ("Unable to create %s: %s", gis_uid_path, error->message);
      g_clear_error (&error);
  }

  g_free (uid_str);
  g_free (gis_uid_path);
}

static void
log_user_in (GisSummaryPage *page)
{
  GisSummaryPagePrivate *priv = gis_summary_page_get_instance_private (page);
  GError *error = NULL;
  GdmGreeter *greeter;
  GdmUserVerifier *user_verifier;

  if (!connect_to_gdm (&greeter, &user_verifier)) {
    g_warning ("No GDM connection; not initiating login");
    return;
  }

  g_signal_connect (user_verifier, "info",
                    G_CALLBACK (on_info), page);
  g_signal_connect (user_verifier, "problem",
                    G_CALLBACK (on_problem), page);
  g_signal_connect (user_verifier, "info-query",
                    G_CALLBACK (on_info_query), page);
  g_signal_connect (user_verifier, "secret-info-query",
                    G_CALLBACK (on_secret_info_query), page);

  g_signal_connect (greeter, "session-opened",
                    G_CALLBACK (on_session_opened), page);

  /* We are in NEW_USER mode and we want to make it possible for third
   * parties to find out which user ID we created.
   */
  add_uid_file (act_user_get_uid (priv->user_account));

  gdm_user_verifier_call_begin_verification_for_user_sync (user_verifier,
                                                           SERVICE_NAME,
                                                           act_user_get_user_name (priv->user_account),
                                                           NULL, &error);

  if (error != NULL) {
    g_warning ("Could not begin verification: %s", error->message);
    return;
  }
}

static void
finish_fbe (GisSummaryPage *page)
{
  gis_ensure_stamp_files ();

  switch (gis_driver_get_mode (GIS_PAGE (page)->driver))
    {
    case GIS_DRIVER_MODE_NEW_USER:
      gis_driver_hide_window (GIS_PAGE (page)->driver);
      log_user_in (page);
      break;
    case GIS_DRIVER_MODE_EXISTING_USER:
      g_application_quit (G_APPLICATION (GIS_PAGE (page)->driver));
    default:
      break;
    }
}

static void
initial_contact_app_signal (GDBusProxy *proxy,
                            gchar *sender,
                            gchar *signal_name,
                            GVariant *parameters,
                            gpointer user_data)
{
  GisSummaryPage *page = user_data;
  GisSummaryPagePrivate *priv = gis_summary_page_get_instance_private (page);
  const gchar *object, *property;
  g_autoptr(GVariant) value = NULL;

  if (g_strcmp0 (signal_name, "ObjectNotify") != 0)
    return;

  g_return_if_fail (g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(ssv)")));

  g_variant_get (parameters, "(&s&sv)", &object, &property, &value);

  if (g_strcmp0 (object, "view.JSContext.globalParameters") != 0)
    return;

  if (g_strcmp0 (property, "unlocked") != 0)
    return;

  g_return_if_fail (g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN));

  if (g_variant_get_boolean (value))
    {
      g_signal_handler_disconnect (priv->clippy_proxy,
                                   priv->clippy_proxy_signal_id);
      finish_fbe (page);
    }
}

static void
run_initial_contact_quest (GisSummaryPage *page)
{
  GDBusConnection *bus;
  g_autoptr(GDBusActionGroup) clubhouse_action_group = NULL;
  g_auto(GStrv) clubhouse_actions = NULL;

  bus = g_application_get_dbus_connection (G_APPLICATION (GIS_PAGE (page)->driver));
  clubhouse_action_group = g_dbus_action_group_get (bus,
                                                    "com.endlessm.Clubhouse",
                                                    "/com/endlessm/Clubhouse");

  /* GDBusActionGroup has a weird API and needs to be initialized like this before
   * its first use.
   */
  clubhouse_actions = g_action_group_list_actions (G_ACTION_GROUP (clubhouse_action_group));

  g_action_group_activate_action (G_ACTION_GROUP (clubhouse_action_group),
                                  "run-quest",
                                  g_variant_new ("(sb)",
                                                 "AdaQuestSet.FirstContact", /* quest name */
                                                 TRUE));                     /* run in shell */
}

static void
initial_contact_connect_to_clippy (GisSummaryPage *page)
{
  GisSummaryPagePrivate *priv = gis_summary_page_get_instance_private (page);
  g_autoptr(GError) error = NULL;

  g_dbus_proxy_call_sync (priv->clippy_proxy,
                          "Connect",
                          g_variant_new ("(sss)",
                                         "view.JSContext.globalParameters",
                                         "notify",
                                         "unlocked"),
                          G_DBUS_CALL_FLAGS_NONE,
                          G_MAXINT,
                          NULL,
                          &error);

  if (error != NULL)
    {
      g_critical ("Could not call Connect on DBus proxy for HackUnlock App: %s", error->message);

      finish_fbe (page);
      return;
    }

  priv->clippy_proxy_signal_id =
    g_signal_connect (priv->clippy_proxy, "g-signal",
                      G_CALLBACK (initial_contact_app_signal), page);

  run_initial_contact_quest (page);
}

static void
run_initial_contact_app (GisSummaryPage *page)
{
  GisSummaryPagePrivate *priv = gis_summary_page_get_instance_private (page);
  g_autoptr(GError) error = NULL;

  priv->clippy_proxy =
    g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                   G_DBUS_PROXY_FLAGS_NONE,
                                   NULL,
                                   "com.endlessm.HackUnlock",
                                   "/com/endlessm/Clippy",
                                   "com.endlessm.Clippy",
                                   NULL,
                                   &error);

  if (error != NULL)
    {
      g_critical ("Could not get DBus proxy for HackUnlock App: %s", error->message);

      finish_fbe (page);
      return;
    }

  initial_contact_connect_to_clippy (page);
}

static void
done_cb (GtkButton *button, GisSummaryPage *page)
{
  if (gis_driver_is_hack (GIS_PAGE (page)->driver) ||
      g_getenv ("HACK_DEBUG_INITIAL_CONTACT") != NULL)
    run_initial_contact_app (page);
  else
    finish_fbe (page);
}

static void
gis_summary_page_shown (GisPage *page)
{
  GisSummaryPage *summary = GIS_SUMMARY_PAGE (page);
  GisSummaryPagePrivate *priv = gis_summary_page_get_instance_private (summary);

  gis_driver_save_data (GIS_PAGE (page)->driver);

  gis_driver_get_user_permissions (GIS_PAGE (page)->driver,
                                   &priv->user_account,
                                   &priv->user_password);

  gtk_widget_grab_focus (priv->start_button);
}

static char *
get_item (const char *buffer, const char *name)
{
  char *label, *start, *end, *result;
  char end_char;

  result = NULL;
  start = NULL;
  end = NULL;
  label = g_strconcat (name, "=", NULL);
  if ((start = strstr (buffer, label)) != NULL)
    {
      start += strlen (label);
      end_char = '\n';
      if (*start == '"')
        {
          start++;
          end_char = '"';
        }

      end = strchr (start, end_char);
    }

    if (start != NULL && end != NULL)
      {
        result = g_strndup (start, end - start);
      }

  g_free (label);

  return result;
}

static void
update_distro_name (GisSummaryPage *page)
{
  GisSummaryPagePrivate *priv = gis_summary_page_get_instance_private (page);
  char *buffer;
  char *name;
  char *text;

  name = NULL;

  if (g_file_get_contents ("/etc/os-release", &buffer, NULL, NULL))
    {
      name = get_item (buffer, "NAME");
      g_free (buffer);
    }

  if (!name)
    name = g_strdup ("GNOME 3");

  /* Translators: the parameter here is the name of a distribution,
   * like "Fedora" or "Ubuntu". It falls back to "GNOME 3" if we can't
   * detect any distribution. */
  text = g_strdup_printf (_("_Start Using %s"), name);
  gtk_label_set_label (GTK_LABEL (priv->start_button_label), text);
  g_free (text);

  /* Translators: the parameter here is the name of a distribution,
   * like "Fedora" or "Ubuntu". It falls back to "GNOME 3" if we can't
   * detect any distribution. */

  g_free (name);
}

static void
gis_summary_page_constructed (GObject *object)
{
  GisSummaryPage *page = GIS_SUMMARY_PAGE (object);
  GisSummaryPagePrivate *priv = gis_summary_page_get_instance_private (page);

  G_OBJECT_CLASS (gis_summary_page_parent_class)->constructed (object);

  update_distro_name (page);
  g_signal_connect (priv->start_button, "clicked", G_CALLBACK (done_cb), page);

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_set_visible (priv->warning_box,
                          gis_driver_is_live_session (GIS_PAGE (object)->driver));

  if (gis_driver_is_live_session (GIS_PAGE (object)->driver))
  {
    gtk_label_set_label (GTK_LABEL (priv->title),
                         _("You're ready to try Endless OS"));
    gtk_label_set_markup (GTK_LABEL (priv->tagline),
                          _("<b>Any files you download or documents you create will be "
                            "lost forever when you restart or shutdown the computer.</b>"));
  }

  /* Show the right contents for Hack images */
  if (gis_driver_is_hack (GIS_PAGE (page)->driver))
    {
      gtk_widget_show_all (priv->hack_box);
      gtk_widget_hide (priv->title);
      gtk_label_set_label (GTK_LABEL (priv->start_button_label), _("Let's Go!"));
      gtk_widget_set_margin_start (priv->start_button_label, 12);
      gtk_widget_set_margin_end (priv->start_button_label, 12);
    }

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_summary_page_finalize (GObject *object)
{
  GisSummaryPage *page = GIS_SUMMARY_PAGE (object);
  GisSummaryPagePrivate *priv = gis_summary_page_get_instance_private (page);

  if (priv->clippy_proxy_signal_id)
    {
      g_signal_handler_disconnect (priv->clippy_proxy,
                                   priv->clippy_proxy_signal_id);
      priv->clippy_proxy_signal_id = 0;
    }

  g_clear_object (&priv->clippy_proxy);

  G_OBJECT_CLASS (gis_summary_page_parent_class)->finalize (object);
}

static void
gis_summary_page_locale_changed (GisPage *page)
{
  gis_page_set_title (page, _("Ready to Go"));
  update_distro_name (GIS_SUMMARY_PAGE (page));
}

static void
gis_summary_page_class_init (GisSummaryPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-summary-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSummaryPage, start_button);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSummaryPage, start_button_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSummaryPage, tagline);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSummaryPage, title);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSummaryPage, warning_box);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisSummaryPage, hack_box);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_summary_page_locale_changed;
  page_class->shown = gis_summary_page_shown;
  object_class->constructed = gis_summary_page_constructed;
  object_class->finalize = gis_summary_page_finalize;
}

static void
gis_summary_page_init (GisSummaryPage *page)
{
  g_resources_register (summary_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (page));
}

void
gis_prepare_summary_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_SUMMARY_PAGE,
                                     "driver", driver,
                                     NULL));
}

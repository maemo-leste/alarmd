/**
 * This file is part of alarmd
 *
 * Contact Person: David Weinehall <david.weinehall@nokia.com>
 *
 * Copyright (C) 2006 Nokia Corporation
 * alarmd and libalarm are free software; you can redistribute them
 * and/or modify them under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 *
 * alarmd and libalarm are distributed in the hope that they will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <dbus/dbus.h>
#include <glib/gthread.h>
#include <glib/gslist.h>
#include <glib/gstrfuncs.h>
#include <systemui/dbus-names.h>
#include <systemui/alarm_dialog-dbus-names.h>
#include <systemui/actingdead-dbus-names.h>
#include <string.h>

#include "debug.h"
#include "include/alarm_dbus.h"
#include "rpc-dbus.h"
#include "rpc-systemui.h"
#include "rpc-mce.h"

#define ALARMD_ACTION_DBUS_METHOD "action_dbus_return"

/* Must be in sync with GTK_RESPONSE_ACCEPT. */
enum response {
       RESPONSE_ACCEPT = -3
};

enum flag {
       FLAG_CAN_SNOOZE = 1 << 0,
       FLAG_POWERUP = 1 << 1,
};

typedef struct _SystemuiAlarmdDialog SystemuiAlarmdDialog;
struct _SystemuiAlarmdDialog {
       time_t alarm_time;
       gchar *title;
       gchar *message;
       gchar *sound;
       gchar *icon;
       gpointer user_data;
       SystemuiAlarmdDialogCallback cb;
       enum flag flags;
       gboolean status;
       guint count;
       guint timer_id;
};

static GStaticMutex queue_mutex = G_STATIC_MUTEX_INIT;
static GSList *dialog_queue = NULL;
static DBusConnection *system_bus = NULL;
static gboolean mce_dialog_visible = FALSE;
static void _dialog_free(SystemuiAlarmdDialog *dialog);

static gboolean _resend_message(gpointer user_data);
static DBusConnection *_dbus_connect(void);
static void _dbus_disconnect(DBusConnection *conn);
static void _queue_changed(void);
static void _close_dialog(void);
static gint _queue_compare(gconstpointer v1, gconstpointer v2);
static gboolean _dialog_do_cb(gpointer data);
static DBusHandlerResult _dialog_ackd(DBusConnection *connection,
               DBusMessage *message,
               void *user_data);
static void _systemui_dialog_queue_append(time_t alarm_time, const gchar *title, const gchar *message, const gchar *sound, const gchar *icon, enum flag flags, SystemuiAlarmdDialogCallback cb, gpointer user_data);

void systemui_powerup_dialog_queue_append(SystemuiAlarmdDialogCallback cb, gpointer user_data)
{
       _systemui_dialog_queue_append(0, NULL, NULL, NULL, NULL, FLAG_POWERUP, cb, user_data);
}

void systemui_alarm_dialog_queue_append(time_t alarm_time, const gchar *title, const gchar *message, const gchar *sound, const gchar *icon, gboolean can_snooze, SystemuiAlarmdDialogCallback cb, gpointer user_data)
{
       _systemui_dialog_queue_append(alarm_time, title, message, sound, icon, can_snooze ? FLAG_CAN_SNOOZE : 0, cb, user_data);
}

void systemui_powerup_dialog_queue_remove(SystemuiAlarmdDialogCallback cb, gpointer user_data)
{
       systemui_alarm_dialog_queue_remove(0, NULL, NULL, NULL, NULL, cb, user_data);
}

void systemui_alarm_dialog_queue_remove(time_t alarm_time, const gchar *title, const gchar *message, const gchar *sound, const gchar *icon, SystemuiAlarmdDialogCallback cb, gpointer user_data)
{
       SystemuiAlarmdDialog dlg = {
               .alarm_time = alarm_time,
               .title = (gchar *)title,
               .message = (gchar *)message,
               .sound = (gchar *)sound,
               .icon = (gchar *)icon,
               .user_data = user_data,
               .cb = cb,
               .flags = 0
       };
       GSList *iter = NULL;
       SystemuiAlarmdDialog *remove = NULL;
       ENTER_FUNC;

       g_static_mutex_lock(&queue_mutex);
       iter = g_slist_find_custom(dialog_queue, &dlg, _queue_compare);
       if (iter) {
               remove = (SystemuiAlarmdDialog *)iter->data;
               if (iter == dialog_queue && system_bus) {
                       _close_dialog();
               }
       } else {
               g_static_mutex_unlock(&queue_mutex);
               LEAVE_FUNC;
               return;
       }
       dialog_queue = g_slist_delete_link(dialog_queue, iter);
       g_static_mutex_unlock(&queue_mutex);

       if (remove->timer_id) {
               g_source_remove(remove->timer_id);
               remove->timer_id = 0;
       }
       _dialog_free(remove);
       _queue_changed();
       LEAVE_FUNC;
}

static gint _queue_compare(gconstpointer v1, gconstpointer v2)
{
       SystemuiAlarmdDialog *lval = (SystemuiAlarmdDialog *)v1;
       SystemuiAlarmdDialog *rval = (SystemuiAlarmdDialog *)v2;

       ENTER_FUNC;
       gint retval;

       if ((retval = lval->alarm_time - rval->alarm_time)) {
               LEAVE_FUNC;
               return retval;
       }
       if (lval->title && rval->title && (retval = strcmp(lval->title, rval->title))) {
               LEAVE_FUNC;
               return retval;
       }
       if (lval->message && rval->message && (retval = strcmp(lval->message, rval->message))) {
               LEAVE_FUNC;
               return retval;
       }
       if (lval->sound && rval->sound && (retval = strcmp(lval->sound, rval->sound))) {
               LEAVE_FUNC;
               return retval;
       }
       if (lval->icon && rval->icon && (retval = strcmp(lval->icon, rval->icon))) {
               LEAVE_FUNC;
               return retval;
       }
       if ((retval = (gint)lval->user_data - (gint)rval->user_data)) {
               LEAVE_FUNC;
               return retval;
       }
       LEAVE_FUNC;
       return lval->cb != rval->cb;
}

static void _dialog_free(SystemuiAlarmdDialog *dialog)
{
       ENTER_FUNC;
       if (!dialog) {
               LEAVE_FUNC;
               return;
       }

       if (dialog->title) {
               g_free(dialog->title);
       }
       if (dialog->message) {
               g_free(dialog->message);
       }
       if (dialog->sound) {
               g_free(dialog->sound);
       }
       if (dialog->icon) {
               g_free(dialog->icon);
       }
       g_free(dialog);
       LEAVE_FUNC;
}

static void _queue_changed(void)
{
       ENTER_FUNC;
       g_static_mutex_lock(&queue_mutex);
       if (dialog_queue && !system_bus) {
               SystemuiAlarmdDialog *dlg = (SystemuiAlarmdDialog *)dialog_queue->data;
               const gchar *my_service = ALARMD_SERVICE;
               const gchar *my_path = ALARMD_PATH;
               const gchar *my_interface = ALARMD_INTERFACE;
               const gchar *my_method = ALARMD_ACTION_DBUS_METHOD;
               DBusMessage *reply = NULL;
               system_bus = _dbus_connect();
               dbus_int32_t retval = 0;
               if (dlg->flags & FLAG_POWERUP) {
                       const int dialog_type = ALARM_MODE_SWITCHON;

                       dbus_do_call(system_bus,
                                       &reply,
                                       FALSE,
                                       SYSTEMUI_SERVICE,
                                       SYSTEMUI_REQUEST_PATH,
                                       SYSTEMUI_REQUEST_IF,
                                       SYSTEMUI_ALARM_OPEN_REQ,
                                       DBUS_TYPE_STRING, &my_service,
                                       DBUS_TYPE_STRING, &my_path,
                                       DBUS_TYPE_STRING, &my_interface,
                                       DBUS_TYPE_STRING, &my_method,
                                       DBUS_TYPE_UINT32, &dialog_type,
                                       DBUS_TYPE_INVALID);
               } else {
                       const gchar *my_sound = dlg->sound == NULL ? "" : dlg->sound;
                       const gchar *my_icon = dlg->icon == NULL ? "" : dlg->icon;
                       const gchar *my_message = dlg->message == NULL ? "" : dlg->message;
                       const int dialog_type = (dlg->flags & FLAG_CAN_SNOOZE) ? ALARM_MODE_NORMAL : ALARM_MODE_NOSNOOZE;

                       DEBUG("sound = %s, icon = %s, message = %s", my_sound, my_icon, my_message);
                       dbus_do_call(system_bus,
                                       &reply,
                                       FALSE,
                                       SYSTEMUI_SERVICE,
                                       SYSTEMUI_REQUEST_PATH,
                                       SYSTEMUI_REQUEST_IF,
                                       SYSTEMUI_ALARM_OPEN_REQ,
                                       DBUS_TYPE_STRING, &my_service,
                                       DBUS_TYPE_STRING, &my_path,
                                       DBUS_TYPE_STRING, &my_interface,
                                       DBUS_TYPE_STRING, &my_method,
                                       DBUS_TYPE_UINT32, &dialog_type,
                                       DBUS_TYPE_STRING, &my_message,
                                       DBUS_TYPE_STRING, &my_sound,
                                       DBUS_TYPE_STRING, &my_icon,
                                       DBUS_TYPE_INVALID);
                       dlg->timer_id = g_timeout_add(300000, _resend_message,
                                       dlg);
               }

               if (reply) {
                       dbus_message_get_args(reply, NULL, DBUS_TYPE_INT32, &retval, DBUS_TYPE_INVALID);
                       dbus_message_unref(reply);
                       reply = NULL;
               }
               if (retval != RESPONSE_ACCEPT) {
                       if (dlg->timer_id) {
                               g_source_remove(dlg->timer_id);
                               dlg->timer_id = 0;
                       }
                       _dbus_disconnect(system_bus);
                       system_bus = NULL;
               }
       }
       g_static_mutex_unlock(&queue_mutex);
       LEAVE_FUNC;
}

static void _close_dialog(void)
{
       ENTER_FUNC;
       g_static_mutex_lock(&queue_mutex);
       if (system_bus) {
               dbus_do_call(system_bus,
                               NULL,
                               FALSE,
                               SYSTEMUI_SERVICE,
                               SYSTEMUI_REQUEST_PATH,
                               SYSTEMUI_REQUEST_IF,
                               SYSTEMUI_ALARM_CLOSE_REQ,
                               DBUS_TYPE_INVALID);
               _dbus_disconnect(system_bus);
               system_bus = NULL;
       }
       g_static_mutex_unlock(&queue_mutex);
       LEAVE_FUNC;
}

static DBusConnection *_dbus_connect(void)
{
       ENTER_FUNC;
       DBusConnection *retval = get_dbus_connection(DBUS_BUS_SYSTEM);

       if (retval) {
               int status = dbus_bus_request_name(retval, ALARMD_SERVICE, 0, NULL);
               if (!(status == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER || DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER))
               {
                       dbus_connection_unref(retval);
                       LEAVE_FUNC;
                       return NULL;
               }
               dbus_connection_add_filter(retval, _dialog_ackd, NULL, NULL);
       }

       LEAVE_FUNC;
       return retval;
}

static void _dbus_disconnect(DBusConnection *conn)
{
       ENTER_FUNC;
       if (conn) {
               dbus_connection_remove_filter(conn, _dialog_ackd, NULL);
               dbus_connection_unref(conn);
       }
       LEAVE_FUNC;
}

static gboolean _dialog_do_cb(gpointer data)
{
       SystemuiAlarmdDialog *dlg = data;
       if (dlg->cb) {
               dlg->cb(dlg->user_data, dlg->status);
       }

       return FALSE;
}

static DBusHandlerResult _dialog_ackd(DBusConnection *connection,
               DBusMessage *message,
               void *user_data)
{
       ENTER_FUNC;
       (void)connection;
       (void)user_data;

       if (dbus_message_is_method_call(message,
                               ALARMD_INTERFACE,
                               ALARMD_ACTION_DBUS_METHOD)) {
               dbus_int32_t retval = 0;
               SystemuiAlarmdDialog *dlg = NULL;

               dbus_message_get_args(message, NULL, DBUS_TYPE_INT32, &retval, DBUS_TYPE_INVALID);

               g_static_mutex_lock(&queue_mutex);
               if (dialog_queue) {
                       dlg = (SystemuiAlarmdDialog *)dialog_queue->data;

                       DEBUG("Deleting link from queue.");
                       dialog_queue = g_slist_delete_link(dialog_queue, dialog_queue);
                       DEBUG("Queue now %p", dialog_queue);
               }
               _dbus_disconnect(system_bus);
               system_bus = NULL;
               g_static_mutex_unlock(&queue_mutex);

               dlg->status = (retval == ALARM_DIALOG_RESPONSE_SNOOZE
                       || retval == ALARM_DIALOG_RESPONSE_POWERUP);
               if (dlg->timer_id) {
                       g_source_remove(dlg->timer_id);
                       dlg->timer_id = 0;
               }

               _queue_changed();

               g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                               _dialog_do_cb,
                               dlg, (GDestroyNotify)_dialog_free);

               LEAVE_FUNC;
               return DBUS_HANDLER_RESULT_HANDLED;
       }

       LEAVE_FUNC;
       return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void _systemui_dialog_queue_append(time_t alarm_time, const gchar *title, const gchar *message, const gchar *sound, const gchar *icon, enum flag flags, SystemuiAlarmdDialogCallback cb, gpointer user_data)
{
       SystemuiAlarmdDialog *new_dialog = g_new0(SystemuiAlarmdDialog, 1);
       ENTER_FUNC;
       new_dialog->alarm_time = alarm_time;
       new_dialog->title = g_strdup(title);
       new_dialog->message = g_strdup(message);
       new_dialog->sound = g_strdup(sound);
       new_dialog->icon = g_strdup(icon);
       new_dialog->flags = flags;
       new_dialog->cb = cb;
       new_dialog->user_data = user_data;
       g_static_mutex_lock(&queue_mutex);
       dialog_queue = g_slist_append(dialog_queue, new_dialog);
       g_static_mutex_unlock(&queue_mutex);
       _queue_changed();
       LEAVE_FUNC;
}

void update_mce_alarm_visibility(void)
{
       DBusConnection *conn;

       conn = get_dbus_connection(DBUS_BUS_SYSTEM);
       g_static_mutex_lock(&queue_mutex);
       if ((dialog_queue && mce_dialog_visible)) {
               g_static_mutex_unlock(&queue_mutex);
               dbus_connection_unref(conn);
               return;
       }
       mce_dialog_visible = dialog_queue ? TRUE : FALSE;
       mce_set_alarm_visibility(conn, mce_dialog_visible);
       g_static_mutex_unlock(&queue_mutex);
       dbus_connection_unref(conn);
}

void systemui_ack_all_dialogs(void)
{
       GSList *my_list;

       ENTER_FUNC;
       g_static_mutex_lock(&queue_mutex);
       my_list = dialog_queue;
       dialog_queue = NULL;
       g_static_mutex_unlock(&queue_mutex);

       _close_dialog();

       while (my_list) {
               SystemuiAlarmdDialog *dlg = my_list->data;
               if (dlg->timer_id) {
                       g_source_remove(dlg->timer_id);
                       dlg->timer_id = 0;
               }
               dlg->cb(dlg->user_data, FALSE);
               g_free(dlg);

               my_list = g_slist_delete_link(my_list, my_list);
       }

       LEAVE_FUNC;
}

static gboolean _resend_message(gpointer user_data)
{
       SystemuiAlarmdDialog *dlg = user_data;

       const gchar *my_service = ALARMD_SERVICE;
       const gchar *my_path = ALARMD_PATH;
       const gchar *my_interface = ALARMD_INTERFACE;
       const gchar *my_method = ALARMD_ACTION_DBUS_METHOD;
       const gchar *my_sound = dlg->sound == NULL ? "" : dlg->sound;
       const gchar *my_icon = dlg->icon == NULL ? "" : dlg->icon;
       const gchar *my_message = dlg->message == NULL ? "" : dlg->message;
       const int dialog_type = (dlg->flags & FLAG_CAN_SNOOZE) ? ALARM_MODE_NORMAL : ALARM_MODE_NOSNOOZE;
       ENTER_FUNC;

       DEBUG("sound = %s, icon = %s, message = %s", my_sound, my_icon, my_message);
       g_static_mutex_unlock(&queue_mutex);
       if (system_bus) {
               dbus_do_call(system_bus,
                               NULL,
                               FALSE,
                               SYSTEMUI_SERVICE,
                               SYSTEMUI_REQUEST_PATH,
                               SYSTEMUI_REQUEST_IF,
                               SYSTEMUI_ALARM_OPEN_REQ,
                               DBUS_TYPE_STRING, &my_service,
                               DBUS_TYPE_STRING, &my_path,
                               DBUS_TYPE_STRING, &my_interface,
                               DBUS_TYPE_STRING, &my_method,
                               DBUS_TYPE_UINT32, &dialog_type,
                               DBUS_TYPE_STRING, &my_message,
                               DBUS_TYPE_STRING, &my_sound,
                               DBUS_TYPE_STRING, &my_icon,
                               DBUS_TYPE_INVALID);
       }
       g_static_mutex_unlock(&queue_mutex);
       dlg->count++;

       if (dlg->count < 3) {
               LEAVE_FUNC;
               return TRUE;
       }
       dlg->timer_id = 0;
       LEAVE_FUNC;
       return FALSE;
}

gboolean systemui_is_acting_dead(void)
{
       DBusMessage *reply = NULL;
       gboolean retval = FALSE;
       DBusConnection *system_bus;

       ENTER_FUNC;
       system_bus = get_dbus_connection(DBUS_BUS_SYSTEM);
       dbus_do_call(system_bus,
                       &reply,
                       FALSE,
                       SYSTEMUI_SERVICE,
                       SYSTEMUI_REQUEST_PATH,
                       SYSTEMUI_REQUEST_IF,
                       SYSTEMUI_ACTINGDEAD_GETSTATE_REQ,
                       DBUS_TYPE_INVALID);
       dbus_connection_unref(system_bus);

       if (reply) {
               dbus_message_get_args(reply, NULL,
                               DBUS_TYPE_BOOLEAN, &retval,
                               DBUS_TYPE_INVALID);
               dbus_message_unref(reply);
       }

       LEAVE_FUNC;
       return retval;
}

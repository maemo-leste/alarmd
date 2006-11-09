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
#include <libosso.h>
#include <osso-log.h>
#include "include/alarm_dbus.h"
#include "dbusobjectfactory.h"
#include "rpc-osso.h"
#include "rpc-systemui.h"
#include "queue.h"
#include "event.h"
#include "debug.h"

static gint _bogus_rpc_f(const gchar *interface, const gchar *method,
       GArray *arguments, gpointer data,
       osso_rpc_t *retval);
static void _osso_time_changed(gpointer data);
static DBusHandlerResult _dbus_message_in(DBusConnection *connection,
               DBusMessage *message,
               void *user_data);
static void _osso_hw_cb(osso_hw_state_t *state, gpointer data);

osso_context_t *init_osso()
{
       osso_context_t *retval = NULL;

       ENTER_FUNC;
       retval = osso_initialize(PACKAGE, VERSION, FALSE, NULL);

       if (!retval) {
               DLOG_ERR("OSSO initialization failed.");
       }
       LEAVE_FUNC;

       return retval;
}

void set_osso_callbacks(osso_context_t *osso, AlarmdQueue *queue)
{
       DBusConnection *bus = NULL;
       osso_hw_state_t state = { TRUE, FALSE, FALSE, FALSE, 0 };
       ENTER_FUNC;
       osso_rpc_set_default_cb_f(osso, _bogus_rpc_f, NULL);
       osso_time_set_notification_cb(osso, _osso_time_changed, queue);
       osso_hw_set_event_cb(osso, &state, _osso_hw_cb, NULL);

       bus = osso_get_dbus_connection(osso);
       g_object_ref(queue);
       dbus_connection_add_filter(bus, _dbus_message_in, queue, g_object_unref);
       bus = osso_get_sys_dbus_connection(osso);
       g_object_ref(queue);
       dbus_connection_add_filter(bus, _dbus_message_in, queue, g_object_unref);
       LEAVE_FUNC;
}

void deinit_osso(osso_context_t *osso, AlarmdQueue *queue)
{
       DBusConnection *bus = NULL;
       ENTER_FUNC;
       bus = osso_get_dbus_connection(osso);
       dbus_connection_remove_filter(bus, _dbus_message_in, queue);
       bus = osso_get_sys_dbus_connection(osso);
       dbus_connection_remove_filter(bus, _dbus_message_in, queue);
       osso_deinitialize(osso);
       LEAVE_FUNC;
}

static gint _bogus_rpc_f(const gchar *interface, const gchar *method,
       GArray *arguments, gpointer data,
       osso_rpc_t *retval)
{
       (void)interface;
       (void)method;
       (void)arguments;
       (void)data;
       (void)retval;

       return OSSO_INVALID;
}

static void _osso_time_changed(gpointer data)
{
       ENTER_FUNC;
       DLOG_DEBUG("Time changed.");
       alarmd_object_time_changed(ALARMD_OBJECT(data));
       LEAVE_FUNC;
}

static DBusHandlerResult _dbus_message_in(DBusConnection *connection,
               DBusMessage *message,
               void *user_data)
{
       ENTER_FUNC;
       if (dbus_message_is_method_call(message, ALARMD_INTERFACE, ALARM_EVENT_ADD)) {
               DBusMessageIter iter;
               GObject *new_obj;
               glong retval = 0;
               dbus_message_iter_init(message, &iter);
               new_obj = dbus_object_factory(&iter);
               if (new_obj) {
                       retval = alarmd_queue_add_event(ALARMD_QUEUE(user_data), ALARMD_EVENT(new_obj));
                       g_object_unref(new_obj);
               } else {
                       DLOG_ERR("DBus parameters were not an object.");
               }
               if (!dbus_message_get_no_reply(message)) {
                       DBusMessage *reply =
                               dbus_message_new_method_return(message);
                       if (reply) {
                               dbus_message_append_args(reply, DBUS_TYPE_INT32, &retval, DBUS_TYPE_INVALID);
                               dbus_connection_send(connection, reply, NULL);
                               dbus_message_unref(reply);
                       } else {
                               DLOG_ERR("Could not create reply message.");
                       }
               }
               LEAVE_FUNC;
               return DBUS_HANDLER_RESULT_HANDLED;
       }
       if (dbus_message_is_method_call(message, ALARMD_INTERFACE, ALARM_EVENT_DEL)) {
               dbus_int32_t event_id = 0;
               gboolean status = FALSE;
               dbus_message_get_args(message, NULL, DBUS_TYPE_INT32, &event_id, DBUS_TYPE_INVALID);
               if (event_id) {
                       status = alarmd_queue_remove_event(ALARMD_QUEUE(user_data), (glong)event_id);
               } else {
                       DLOG_WARN("Could not delete: no id supplied.");
               }
               if (!dbus_message_get_no_reply(message)) {
                       DBusMessage *reply = dbus_message_new_method_return(message);
                       if (reply) {
                               dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &status, DBUS_TYPE_INVALID);
                               dbus_connection_send(connection, reply, NULL);
                               dbus_message_unref(reply);
                       } else {
                               DLOG_ERR("Could not create reply message.");
                       }
               }
               LEAVE_FUNC;
               return DBUS_HANDLER_RESULT_HANDLED;
       }
       if (dbus_message_is_method_call(message, ALARMD_INTERFACE, ALARM_EVENT_QUERY)) {
               dbus_uint64_t start_time = 0;
               dbus_uint64_t end_time = 0;
               gint32 flag_mask = 0;
               gint32 flags = 0;
               glong *status = FALSE;
               guint n_params = 0;
               dbus_message_get_args(message, NULL, DBUS_TYPE_UINT64, &start_time, DBUS_TYPE_UINT64, &end_time, DBUS_TYPE_INT32, &flag_mask, DBUS_TYPE_INT32, &flags, DBUS_TYPE_INVALID);
               DEBUG("start: %lld, end: %lld", start_time, end_time);
               if (start_time < end_time) {
                       status = alarmd_queue_query_events(ALARMD_QUEUE(user_data), start_time, end_time, flag_mask, flags, &n_params);
               } else {
                       DLOG_ERR("Could not query, invalid arguments.");
               }
               if (!dbus_message_get_no_reply(message)) {
                       DBusMessage *reply = dbus_message_new_method_return(message);
                       if (reply) {
                               dbus_message_append_args(reply, DBUS_TYPE_ARRAY, DBUS_TYPE_INT32, &status, n_params, DBUS_TYPE_INVALID);
                               dbus_connection_send(connection, reply, NULL);
                               dbus_message_unref(reply);
                       } else {
                               DLOG_ERR("Could not create reply message.");
                       }
               } else {
                       DLOG_WARN("Events queried but no reply wanted.");
               }

               if (status) {
                       g_free(status);
               }
               LEAVE_FUNC;
               return DBUS_HANDLER_RESULT_HANDLED;
       }
       if (dbus_message_is_method_call(message, ALARMD_INTERFACE, ALARM_EVENT_GET)) {
               AlarmdEvent *event = NULL;
               dbus_int32_t event_id = 0;
               dbus_message_get_args(message, NULL, DBUS_TYPE_INT32, &event_id, DBUS_TYPE_INVALID);
               if (event_id) {
                       event = alarmd_queue_get_event(ALARMD_QUEUE(user_data), event_id);
               } else {
                       DLOG_WARN("Could not get event: no id.");
               }
               if (!dbus_message_get_no_reply(message)) {
                       DBusMessage *reply =
                               dbus_message_new_method_return(message);
                       if (reply) {
                               if (event) {
                                       DBusMessageIter iter;
                                       dbus_message_iter_init_append(reply,
                                                       &iter);
                                       alarmd_object_to_dbus(
                                                       ALARMD_OBJECT(event),
                                                       &iter);
                               }
                               dbus_connection_send(connection, reply, NULL);
                               dbus_message_unref(reply);
                       } else {
                               DLOG_ERR("Could not create reply message.");
                       }
               } else {
                       DLOG_WARN("Event queried but no reply wanted.");
               }
               LEAVE_FUNC;
               return DBUS_HANDLER_RESULT_HANDLED;
       }
       if (dbus_message_is_method_call(message, ALARMD_INTERFACE, ALARMD_SNOOZE_GET)) {
               dbus_uint32_t snooze = 0;

               g_object_get(G_OBJECT(user_data), "snooze", &snooze, NULL);
               if (!dbus_message_get_no_reply(message)) {
                       DBusMessage *reply =
                               dbus_message_new_method_return(message);

                       if (reply) {
                               dbus_message_append_args(reply,
                                               DBUS_TYPE_UINT32, &snooze,
                                               DBUS_TYPE_INVALID);
                               dbus_connection_send(connection, reply, NULL);
                               dbus_message_unref(reply);
                       } else {
                               DLOG_ERR("Could not create reply message.");
                       }
               } else {
                       DLOG_WARN("Snooze queried but no reply wanted.");
               }
               LEAVE_FUNC;
               return DBUS_HANDLER_RESULT_HANDLED;
       }
       if (dbus_message_is_method_call(message, ALARMD_INTERFACE, ALARMD_SNOOZE_SET)) {
               dbus_uint32_t snooze = 0;

               dbus_message_get_args(message, NULL, DBUS_TYPE_UINT32, &snooze, DBUS_TYPE_INVALID);

               if (snooze) {
                       g_object_set(G_OBJECT(user_data), "snooze", snooze, NULL);
               } else {
                       DLOG_WARN("Could not set snooze: no time specified.");
               }
               if (!dbus_message_get_no_reply(message)) {
                       DBusMessage *reply = dbus_message_new_method_return(message);

                       if (reply) {
                               dbus_bool_t status = snooze != 0;

                               dbus_message_append_args(reply,
                                               DBUS_TYPE_BOOLEAN, &status,
                                               DBUS_TYPE_INVALID);
                               dbus_connection_send(connection, reply, NULL);
                               dbus_message_unref(reply);
                       } else {
                               DLOG_ERR("Could not create reply message.");
                       }
               }
               LEAVE_FUNC;
               return DBUS_HANDLER_RESULT_HANDLED;
       }
       LEAVE_FUNC;
       return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void _osso_hw_cb(osso_hw_state_t *state, gpointer data)
{
       ENTER_FUNC;
       (void)data;

       if (state->shutdown_ind) {
               DLOG_DEBUG("Shutdown indication -- closing all dialogs.");
               systemui_ack_all_dialogs();
       }

       LEAVE_FUNC;
}

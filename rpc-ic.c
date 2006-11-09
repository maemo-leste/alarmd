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

#include <string.h>
#include <osso-ic-dbus.h>
#include "rpc-dbus.h"
#include "rpc-ic.h"
#include "debug.h"

#define ICD_CONNECTED_SIGNAL "type='signal',interface='" ICD_DBUS_INTERFACE "',path='" ICD_DBUS_PATH "',member='" ICD_STATUS_CHANGED_SIG "',arg2='CONNECTED'"

static DBusHandlerResult _ic_connected(DBusConnection *connection, DBusMessage *message, void *user_data);

typedef struct _ICNotify {
       ICConnectedNotifyCb cb;
       gpointer user_data;
} ICNotify;

static GSList *connection_notifies = NULL;
static DBusConnection *system_bus = NULL;
static GStaticMutex notify_mutex = G_STATIC_MUTEX_INIT;

static gboolean _ic_get_connected_bus(DBusConnection *bus) {
       DBusMessage *msg = NULL;
       ENTER_FUNC;

       if (!bus) {
               LEAVE_FUNC;
               return FALSE;
       }

       dbus_uint32_t state = 0;
       dbus_do_call(bus, &msg, FALSE, ICD_DBUS_SERVICE, ICD_DBUS_PATH,
                       ICD_DBUS_INTERFACE, ICD_GET_STATE_REQ,
                       DBUS_TYPE_INVALID);

       if (msg != NULL) {
               dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &state,
                               DBUS_TYPE_INVALID);
               dbus_message_unref(msg);
       }

       LEAVE_FUNC;
       return state;
}

gboolean ic_get_connected(void)
{
       DBusConnection *conn;
       gboolean state;
       ENTER_FUNC;
       conn = get_dbus_connection(DBUS_BUS_SYSTEM);

       if (!conn) {
               DEBUG("No connection.");
               LEAVE_FUNC;
               return FALSE;
       }

       state = _ic_get_connected_bus(conn);
       dbus_connection_unref(conn);

       DEBUG("Is connection: %d", state);
       LEAVE_FUNC;
       return state;
}

void ic_wait_connection(ICConnectedNotifyCb cb, gpointer user_data)
{
       ENTER_FUNC;
       gboolean connected;
       g_static_mutex_lock(&notify_mutex);

       if (system_bus == NULL) {
               DEBUG("Adding match %s and filter.", ICD_CONNECTED_SIGNAL);
               system_bus = get_dbus_connection(DBUS_BUS_SYSTEM);
               dbus_bus_add_match(system_bus, ICD_CONNECTED_SIGNAL, NULL);
               dbus_connection_add_filter(system_bus,
                               _ic_connected, NULL,
                               NULL);
       }
       connected = _ic_get_connected_bus(system_bus);
       if (!connected) {
               ICNotify *notify = g_new0(ICNotify, 1);
               notify->cb = cb;
               notify->user_data = user_data;
               connection_notifies = g_slist_append(connection_notifies,
                               notify);
       }

       if (connection_notifies == NULL && system_bus != NULL) {
               DEBUG("No notifies, removing match and filter.");
               dbus_connection_remove_filter(system_bus, _ic_connected, NULL);
               dbus_bus_remove_match(system_bus, ICD_CONNECTED_SIGNAL, NULL);
               dbus_connection_unref(system_bus);
               system_bus = NULL;
       }
       g_static_mutex_unlock(&notify_mutex);

       if (connected) {
               cb(user_data);
       }
       LEAVE_FUNC;
}

void ic_unwait_connection(ICConnectedNotifyCb cb, gpointer user_data)
{
       GSList *iter;
       ENTER_FUNC;
       g_static_mutex_lock(&notify_mutex);
       for (iter = connection_notifies; iter != NULL; iter = iter->next) {
               ICNotify *notify = iter->data;

               if (notify->cb == cb &&
                               notify->user_data == user_data) {
                       g_free(notify);
                       connection_notifies = g_slist_delete_link(connection_notifies,
                                       iter);
                       break;
               }
       }

       if (connection_notifies == NULL && system_bus != NULL) {
               dbus_connection_remove_filter(system_bus, _ic_connected, NULL);
               dbus_bus_remove_match(system_bus, ICD_CONNECTED_SIGNAL, NULL);
               dbus_connection_unref(system_bus);
               system_bus = NULL;
       }
       g_static_mutex_unlock(&notify_mutex);
       LEAVE_FUNC;
}

static DBusHandlerResult _ic_connected(DBusConnection *connection, DBusMessage *message, void *user_data)
{
       (void)connection;
       (void)user_data;

       ENTER_FUNC;
       if (dbus_message_is_signal(message, ICD_DBUS_INTERFACE, ICD_STATUS_CHANGED_SIG)) {
               GSList *notifies;
               const gchar *name;
               const gchar *type;
               const gchar *event;

               DEBUG("%s signal from %d", ICD_STATUS_CHANGED_SIG, ICD_DBUS_INTERFACE);

               dbus_message_get_args(message, NULL,
                               DBUS_TYPE_STRING, &name,
                               DBUS_TYPE_STRING, &type,
                               DBUS_TYPE_STRING, &event,
                               DBUS_TYPE_INVALID);

               DEBUG("Args: %s, %s, %s", name, type, event);

               if (strcmp(event, "CONNECTED") != 0) {
                       LEAVE_FUNC;
                       return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
               }

               g_static_mutex_lock(&notify_mutex);

               notifies = connection_notifies;
               connection_notifies = NULL;

               dbus_connection_remove_filter(system_bus, _ic_connected, NULL);
               dbus_bus_remove_match(system_bus, ICD_CONNECTED_SIGNAL, NULL);
               dbus_connection_unref(system_bus);
               system_bus = NULL;

               g_static_mutex_unlock(&notify_mutex);

               while (notifies) {
                       ICNotify *notify = notifies->data;
                       notify->cb(notify->user_data);
                       g_free(notify);
                       notifies = g_slist_delete_link(notifies, notifies);
               }
       }
       LEAVE_FUNC;
       return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

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
#include <glib/gtypes.h>
#include <glib/gthread.h>
#include <glib/gmain.h>
#include <string.h>
#include "rpc-dbus.h"
#include "rpc-statusbar.h"
#include "debug.h"

static guint users = 0;
static DBusConnection *conn = NULL;
static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

#define STATUSBAR_SERVICE "com.nokia.statusbar"
#define STATUSBAR_OBJECT_PATH "/com/nokia/statusbar"
#define STATUSBAR_INTERFACE "com.nokia.statusbar"
static const gchar * const STATUSBAR_ALARM_NAME = "alarm";
static const gint STATUSBAR_MAGIC_ON = 1;
static const gint STATUSBAR_MAGIC_OFF = 0;
#define STATUSBAR_STARTED_SIGNAL "type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='com.nokia.statusbar'"
static const gchar * const EMPTY = "";

static DBusHandlerResult _statusbar_started(DBusConnection *connection, DBusMessage *message, void *user_data);
static gboolean _display_icon(gpointer user_data);

void statusbar_show_icon(void)
{
	ENTER_FUNC;
	g_static_mutex_lock(&mutex);
	users++;
	if (users == 1) {
		conn = get_dbus_connection(DBUS_BUS_SYSTEM);
		if (conn) {
			dbus_do_call(conn, NULL, FALSE, STATUSBAR_SERVICE, STATUSBAR_OBJECT_PATH, STATUSBAR_INTERFACE,
					"event", DBUS_TYPE_STRING, &STATUSBAR_ALARM_NAME, DBUS_TYPE_INT32, &STATUSBAR_MAGIC_ON,
				       	DBUS_TYPE_INT32, &STATUSBAR_MAGIC_ON, DBUS_TYPE_STRING, &EMPTY, DBUS_TYPE_INVALID);
			dbus_bus_add_match(conn, STATUSBAR_STARTED_SIGNAL, NULL);
			dbus_connection_add_filter(conn, _statusbar_started, NULL, NULL);
		}
	}
	g_static_mutex_unlock(&mutex);
	LEAVE_FUNC;
}

void statusbar_hide_icon(void)
{
	ENTER_FUNC;
	g_static_mutex_lock(&mutex);
	users--;
	if (users == 0) {
		if (conn) {
			dbus_do_call(conn, NULL, FALSE, STATUSBAR_SERVICE, STATUSBAR_OBJECT_PATH, STATUSBAR_INTERFACE,
					"event", DBUS_TYPE_STRING, &STATUSBAR_ALARM_NAME, DBUS_TYPE_INT32, &STATUSBAR_MAGIC_ON,
				       	DBUS_TYPE_INT32, &STATUSBAR_MAGIC_OFF, DBUS_TYPE_STRING, &EMPTY, DBUS_TYPE_INVALID);
			dbus_connection_remove_filter(conn, _statusbar_started, NULL);
			dbus_bus_remove_match(conn, STATUSBAR_STARTED_SIGNAL, NULL);
			dbus_connection_unref(conn);
			conn = NULL;
		}
	}
	g_static_mutex_unlock(&mutex);
	LEAVE_FUNC;
}

static gboolean _display_icon(gpointer user_data)
{
	ENTER_FUNC;
	(void)user_data;
	g_static_mutex_lock(&mutex);
	if (users > 0) {
		dbus_do_call(conn, NULL, FALSE, STATUSBAR_SERVICE, STATUSBAR_OBJECT_PATH, STATUSBAR_INTERFACE,
				"event", DBUS_TYPE_STRING, &STATUSBAR_ALARM_NAME, DBUS_TYPE_INT32, &STATUSBAR_MAGIC_ON,
				DBUS_TYPE_INT32, &STATUSBAR_MAGIC_ON, DBUS_TYPE_STRING, &EMPTY, DBUS_TYPE_INVALID);
	}
	g_static_mutex_unlock(&mutex);
	LEAVE_FUNC;
	return FALSE;
}

static DBusHandlerResult _statusbar_started(DBusConnection *connection, DBusMessage *message, void *user_data)
{
	ENTER_FUNC;
	(void)connection;
	(void)user_data;
	if (dbus_message_is_signal(message, DBUS_SERVICE_DBUS, "NameOwnerChanged")) {
		const gchar *name = NULL;
		dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID);
		if (name != NULL && strcmp(name, STATUSBAR_SERVICE) == 0) {
			g_timeout_add(1000, _display_icon, NULL); 
		}
	}
	LEAVE_FUNC;
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

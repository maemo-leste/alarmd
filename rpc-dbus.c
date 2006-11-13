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

#include <glib/gslist.h>
#include <glib/gthread.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <string.h>
#include <libosso.h>
#include <osso-log.h>
#include "rpc-dbus.h"

#define SERVICE_STARTED_SIGNAL "type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='%s'"

static DBusHandlerResult _service_started(DBusConnection *connection, DBusMessage *message, void *user_data);
static osso_context_t *_osso = NULL;

typedef struct _DBusNameNotify {
	gchar *name;
	gchar *match;
	DBusNameNotifyCb cb;
	gpointer user_data;
} DBusNameNotify;

static GSList *service_notifies = NULL;
static DBusConnection *system_bus = NULL;
static GStaticMutex notify_mutex = G_STATIC_MUTEX_INIT;

void dbus_do_call(DBusConnection *bus, DBusMessage **reply, gboolean activation, const gchar *service, const gchar *path, const gchar *interface, const gchar *name, int first_arg_type, ...)
{
	DBusMessage *msg = NULL;
	va_list arg_list;
	DBusError error;

	if (!bus || !path || !interface) {
		DLOG_WARN("dbus_do_call called with NULL arguments.");
		return;
	}

	if (service) {
		msg = dbus_message_new_method_call(service,
				path,
				interface,
				name);
		if (reply == NULL) {
			dbus_message_set_no_reply(msg, TRUE);
		}
		dbus_message_set_auto_start(msg, activation);
	} else {
		msg = dbus_message_new_signal(path,
				interface,
				name);
	}

	if (!msg) {
		DLOG_WARN("DBus message creation failed.");
		return;
	}

	va_start(arg_list, first_arg_type);

	dbus_message_append_args_valist(msg, first_arg_type, arg_list);

	va_end(arg_list);

	dbus_error_init(&error);
	if (reply != NULL) {
		*reply = dbus_connection_send_with_reply_and_block(bus, msg, -1, &error);
	} else {
		dbus_connection_send(bus, msg, NULL);
	}
	if (dbus_error_is_set(&error)) {
		DLOG_WARN("DBus error: %s", error.message);
		dbus_message_unref(msg);
		dbus_error_free(&error);
		return;
	}
	dbus_message_unref(msg);
}

void dbus_watch_name(const gchar *name, DBusNameNotifyCb cb, gpointer user_data)
{
	g_static_mutex_lock(&notify_mutex);
	gboolean has_owner;

	if (system_bus == NULL) {
		system_bus = get_dbus_connection(DBUS_BUS_SYSTEM);
		dbus_connection_add_filter(system_bus, _service_started, NULL, NULL);
	}

	has_owner = dbus_bus_name_has_owner(system_bus, name, NULL);
	if (!has_owner) {
		DBusNameNotify *notify = g_new0(DBusNameNotify, 1);
		notify->cb = cb;
		notify->user_data = user_data;
		notify->name = g_strdup(name);
		notify->match = g_strdup_printf(SERVICE_STARTED_SIGNAL,
				name);
		service_notifies = g_slist_prepend(service_notifies, notify);
		dbus_bus_add_match(system_bus, notify->match, NULL);
	}
	
	if (service_notifies == NULL) {
		dbus_connection_remove_filter(system_bus, _service_started, NULL);
		dbus_connection_unref(system_bus);
		system_bus = NULL;
	}
	g_static_mutex_unlock(&notify_mutex);

	if (has_owner) {
		cb(user_data);
	}
}

void dbus_unwatch_name(const gchar *name, DBusNameNotifyCb cb, gpointer user_data)
{
	GSList *iter;
	g_static_mutex_lock(&notify_mutex);
	for (iter = service_notifies; iter != NULL; iter = iter->next) {
		DBusNameNotify *notify = (DBusNameNotify *)iter->data;

		if (notify->cb == cb &&
				notify->user_data == user_data &&
				strcmp(notify->name, name) == 0) {
			dbus_bus_remove_match(system_bus,
					notify->match, NULL);
			g_free(notify->match);
			g_free(notify->name);
			g_free(notify);
			service_notifies = g_slist_delete_link(service_notifies, iter);
			break;
		}
	}
	if (service_notifies == NULL && system_bus != NULL) {
		dbus_connection_remove_filter(system_bus, _service_started, NULL);
		dbus_connection_unref(system_bus);
		system_bus = NULL;
	}
	g_static_mutex_unlock(&notify_mutex);
}

static DBusHandlerResult _service_started(DBusConnection *connection, DBusMessage *message, void *user_data)
{
	(void)connection;
	(void)user_data;

	if (dbus_message_is_signal(message, DBUS_SERVICE_DBUS, "NameOwnerChanged")) {
		GSList *iter;
		GSList *found = NULL;
		const gchar *name = NULL;
		dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID);
		g_static_mutex_lock(&notify_mutex);
		iter = service_notifies;
		while (iter != NULL) {
			DBusNameNotify *notify = (DBusNameNotify *)iter->data;
			GSList *this = iter;

			iter = iter->next;

			if (strcmp(notify->name, name) == 0) {
				service_notifies = g_slist_remove_link(service_notifies, this);
				found = g_slist_concat(found, this);
				dbus_bus_remove_match(system_bus,
					       	notify->match, NULL);
				g_free(notify->match);
			}

			if (service_notifies == NULL) {
				dbus_connection_remove_filter(system_bus, _service_started, NULL);
				dbus_connection_unref(system_bus);
				system_bus = NULL;
			}
		}
		g_static_mutex_unlock(&notify_mutex);
		while (found) {
			DBusNameNotify *notify = (DBusNameNotify *)found->data;

			notify->cb(notify->user_data);
			g_free(notify->name);
			g_free(notify);

			found = g_slist_delete_link(found, found);
		}
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void dbus_set_osso(osso_context_t *osso) {
	_osso = osso;
}

DBusConnection *get_dbus_connection(DBusBusType type) {
	DBusConnection *conn;
	switch (type) {
	case DBUS_BUS_SYSTEM:
		conn = osso_get_sys_dbus_connection(_osso);
		break;
	default:
		conn = osso_get_dbus_connection(_osso);
		break;
	}
	dbus_connection_ref(conn);
	return conn;
}

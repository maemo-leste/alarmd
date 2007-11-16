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
#include "object.h"
#include "rpc-dbus.h"
#include "debug.h"

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
		g_warning("dbus_do_call called with NULL arguments.");
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
		g_warning("DBus message creation failed.");
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
		g_warning("DBus error: %s", error.message);
		dbus_message_unref(msg);
		dbus_error_free(&error);
		return;
	}
	dbus_message_unref(msg);
}

void dbus_do_call_gvalue(DBusConnection *bus, DBusMessage **reply, gboolean activation, const gchar *service, const gchar *path, const gchar *interface, const gchar *name, const GValueArray *args)
{
	DBusMessage *msg = NULL;
	DBusError error;

	if (!bus || !path || !interface) {
		g_warning("dbus_do_call called with NULL arguments.");
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
		g_warning("DBus message creation failed.");
		return;
	}

	if (args) {
		DBusMessageIter iter;
		guint i;

		dbus_message_iter_init_append(msg, &iter);
		
		for (i = 0; i < args->n_values; i++)
		{
			dbus_message_iter_append_gvalue(&iter, &args->values[i], FALSE);
		}
	}

	dbus_error_init(&error);
	if (reply != NULL) {
		*reply = dbus_connection_send_with_reply_and_block(bus, msg, -1, &error);
	} else {
		dbus_connection_send(bus, msg, NULL);
	}
	if (dbus_error_is_set(&error)) {
		g_warning("DBus error: %s", error.message);
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

void dbus_message_iter_append_gvalue(DBusMessageIter *iter, GValue *gvalue, gboolean as_variator)
{
	DBusMessageIter sub;
#define append_arg(iter, type, value) \
	do { DBusMessageIter temp; if (as_variator) dbus_message_iter_open_container((iter), DBUS_TYPE_VARIANT, type ## _AS_STRING, &temp); dbus_message_iter_append_basic(as_variator ? &temp : (iter), type, (value)); if (as_variator) dbus_message_iter_close_container((iter), &temp); } while (0)
	if (G_VALUE_TYPE(gvalue) == G_TYPE_VALUE_ARRAY) {
		GValueArray *array = g_value_get_boxed(gvalue);
		guint i;
		dbus_message_iter_open_container(iter,
				DBUS_TYPE_ARRAY,
				DBUS_TYPE_VARIANT_AS_STRING,
				&sub);
		if (array) {
			for (i = 0; i < array->n_values; i++)
			{
				dbus_message_iter_append_gvalue(&sub,
						&array->values[i],
						TRUE);
			}
		}
		dbus_message_iter_close_container(iter,
				&sub);
	} else switch (G_VALUE_TYPE(gvalue)) {
	case G_TYPE_CHAR:
		{
			gchar value = g_value_get_char(gvalue);
			append_arg(iter, DBUS_TYPE_BYTE, &value);
			break;
		}
	case G_TYPE_BOOLEAN:
		{
			dbus_bool_t value = g_value_get_boolean(gvalue);
			append_arg(iter, DBUS_TYPE_BOOLEAN, &value);
			break;
		}
	case G_TYPE_LONG:
		{
			dbus_int32_t value = g_value_get_long(gvalue);
			append_arg(iter, DBUS_TYPE_INT32, &value);
			break;
		}
	case G_TYPE_INT:
		{
			dbus_int32_t value = g_value_get_int(gvalue);
			append_arg(iter, DBUS_TYPE_INT32, &value);
			break;
		}
	case G_TYPE_UINT:
		{
			dbus_uint32_t value = g_value_get_uint(gvalue);
			append_arg(iter, DBUS_TYPE_UINT32, &value);
			break;
		}
#ifdef DBUS_HAVE_INT64
	case G_TYPE_INT64:
		{
			dbus_uint64_t value = g_value_get_int64(gvalue);
			append_arg(iter, DBUS_TYPE_INT64, &value);
			break;
		}
	case G_TYPE_UINT64:
		{
			dbus_uint64_t value = g_value_get_uint64(gvalue);
			append_arg(iter, DBUS_TYPE_UINT64, &value);
			break;
		}
#endif
	case G_TYPE_DOUBLE:
		{
			double value = g_value_get_double(gvalue);
			append_arg(iter, DBUS_TYPE_DOUBLE, &value);
			break;
		}
	case G_TYPE_STRING:
		{
			const gchar *value = g_value_get_string(gvalue);
			if (value == NULL) {
				value = "";
			}
			append_arg(iter, DBUS_TYPE_STRING, &value);
			break;
		}
	case G_TYPE_OBJECT:
		{
			GObject *value = g_value_get_object(gvalue);
			alarmd_object_to_dbus(ALARMD_OBJECT(value), iter);
			break;
		}
	default:
		{
			dbus_int32_t value = -1;
			DEBUG("Unsupported type.");
			append_arg(iter, DBUS_TYPE_INT32, &value);
			break;
		}
	}
#undef append_arg
}

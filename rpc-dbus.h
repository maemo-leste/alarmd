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

#ifndef _RPC_DBUS_H_
#define _RPC_DBUS_H_

#include <glib/gtypes.h>
#include <glib-object.h>
#include <libosso.h>
#include <dbus/dbus.h>

/**
 * SECTION:rpc-dbus
 * @short_description: Helpers for sending dbus messages and for wathching
 * for services.
 *
 * These functions are helpers for dbus communication.
 **/

/**
 * DBusNameNotifyCb:
 * @user_data: user data set when the signal handler was connected.
 *
 * Callback to be called when a watched name changes owner.
 **/
typedef void (*DBusNameNotifyCb)(gpointer user_data);

/**
 * dbus_do_call:
 * @bus: #DBusConnection to make the method call on.
 * @reply: Location to store reply or NULL for no replies.
 * @activation: Should the receiver be started if necessary.
 * @service: Service to be called.
 * @path: Path to be called.
 * @interface: Interface to be called.
 * @name: Name of the method call.
 * @first_arg_type: Type of the first argument.
 * @...: Value of first argument, type of second argument...
 *
 * Does a method call over dbus. End the list of argument type / value pairs
 * with a DBUS_TYPE_INVALID. The argument values must be passed by pointer.
 **/
void dbus_do_call(DBusConnection *bus, DBusMessage **reply, gboolean activation, const gchar *service, const gchar *path, const gchar *interface, const gchar *name, int first_arg_type, ...);

/**
 * dbus_do_call_gvalue:
 * @bus: #DBusConnection to make the method call on.
 * @reply: Location to store reply or NULL for no replies.
 * @activation: Should the receiver be started if necessary.
 * @service: Service to be called.
 * @path: Path to be called.
 * @interface: Interface to be called.
 * @name: Name of the method call.
 * @args: Arguments for the method call.
 *
 * Does a method call over dbus. 
 **/
void dbus_do_call_gvalue(DBusConnection *bus, DBusMessage **reply, gboolean activation, const gchar *service, const gchar *path, const gchar *interface, const gchar *name, const GValueArray *args);

/**
 * dbus_watch_name:
 * @name: Name to watch.
 * @cb: Callback to call when name changes owner.
 * @user_data: user data to pass to the @cb.
 *
 * Watches the name, when it changes owner, cb will be called.
 **/
void dbus_watch_name(const gchar *name, DBusNameNotifyCb cb, gpointer user_data);
/**
 * dbus_unwatch_name:
 * @name: Name to stop watching.
 * @cb: Callback that should no longer be called.
 * @user_data: user data to pass to the @cb.
 *
 * Stops watching for owner changes of @name.
 **/
void dbus_unwatch_name(const gchar *name, DBusNameNotifyCb cb, gpointer user_data);

/**
 * dbus_set_osso:
 * @osso: osso context to get connections from.
 *
 * Sets a singleton osso context that is used to get dbus connections wherever
 * they are needed, See #get_dbus_connection.
 **/
void dbus_set_osso(osso_context_t *osso);

/**
 * get_dbus_connection:
 * @type: DBUS_BUS_SESSION for session bus or DBUS_TYPE_SYSTEM for system bus.
 *
 * Gets system or session bus from the osso context registered with
 * #dbus_set_osso. #dbus_set_osso _must_ be called before this can be called.
 * Returns: A dbus connection to the bus specified by @type. Callers owns a
 * reference, unref with dbus_connection_unref.
 **/
DBusConnection *get_dbus_connection(DBusBusType type);

/**
 * dbus_message_iter_append_gvalue:
 * @iter: Iter into which the gvalue is written to.
 * @gvalue: The value which will be written.
 * @as_variator: If true, the value will be written inside a variator container.
 *
 * Adds a gvalue to a dbus message.
 **/
void dbus_message_iter_append_gvalue(DBusMessageIter *iter, GValue *gvalue, gboolean as_variator);

#endif /* _RPC_DBUS_H_ */

/**
 * This file is part of alarmd
 *
 * Contact Person: David Weinehall <david.weinehall@nokia.com>
 *
 * Copyright (C) 2006-2007 Nokia Corporation
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
#include <osso-log.h>
#include <conicconnection.h>
#include <conicconnectionevent.h>
#include "rpc-dbus.h"
#include "rpc-ic.h"
#include "debug.h"

static void _ic_connected(ConIcConnection *connection, ConIcConnectionEvent *event, void *user_data);

typedef struct _ICNotify {
	ICConnectedNotifyCb cb;
	gpointer user_data;
} ICNotify;

static GSList *connection_notifies = NULL;
static gpointer con_ic = NULL;
static GStaticMutex notify_mutex = G_STATIC_MUTEX_INIT;

void ic_wait_connection(ICConnectedNotifyCb cb, gpointer user_data)
{
	ENTER_FUNC;
	ConIcConnection *conn = NULL;
	ICNotify *notify = g_new0(ICNotify, 1);
	g_static_mutex_lock(&notify_mutex);

	notify->cb = cb;
	notify->user_data = user_data;
	connection_notifies = g_slist_append(connection_notifies,
			notify);

	if (con_ic == NULL) {
		conn = con_ic = con_ic_connection_new();
		g_object_add_weak_pointer(G_OBJECT(con_ic),
				&con_ic);
		g_signal_connect(con_ic,
				"connection-event",
				G_CALLBACK(_ic_connected),
				NULL);

	}

	g_static_mutex_unlock(&notify_mutex);

	if (conn && con_ic) {
		g_object_set(con_ic,
				"automatic-connection-events", TRUE,
				NULL);
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
			connection_notifies = g_slist_delete_link(
					connection_notifies,
					iter);
			break;
		}
	}

	if (connection_notifies == NULL && con_ic != NULL) {
		g_object_unref(con_ic);
	}
	g_static_mutex_unlock(&notify_mutex);
	LEAVE_FUNC;
}

static void _ic_connected(ConIcConnection *connection, ConIcConnectionEvent *event, void *user_data)
{
	(void)connection;
	(void)user_data;

	ENTER_FUNC;
	if (con_ic_connection_event_get_status(event) ==
			CON_IC_STATUS_CONNECTED) {
		GSList *notifies;

		DLOG_DEBUG("Got connection, no longer waiting.");

		g_static_mutex_lock(&notify_mutex);

		notifies = connection_notifies;
		connection_notifies = NULL;

		g_object_unref(con_ic);

		g_static_mutex_unlock(&notify_mutex);

		while (notifies) {
			ICNotify *notify = notifies->data;
			notify->cb(notify->user_data);
			g_free(notify);
			notifies = g_slist_delete_link(notifies, notifies);
		}
	}
	LEAVE_FUNC;
}

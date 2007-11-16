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

#include <glib/gmain.h>
#include <glib/gtypes.h>
#include <time.h>
#include <stdio.h>
#include <glib/gmessages.h>
#include "include/timer-interface.h"
#include "debug.h"

static gboolean gtimeout_set_event(TimerPlugin *plugin, time_t wanted_time, TimerCallback cb, TimerCancel cancel, gpointer user_data);
static gboolean gtimeout_remove_event(TimerPlugin *plugin);
static time_t gtimeout_get_time(TimerPlugin *plugin);
static void gtimeout_time_changed(TimerPlugin *plugin);
static void gtimeout_plugin_deinit(TimerPlugin *plugin);

typedef struct _GTimeoutTimerEvent GTimeoutTimerEvent;
struct _GTimeoutTimerEvent {
	guint timer_id;
	time_t timer_time;
	TimerCallback cb;
	TimerCancel cancel;
	gpointer cb_data;

	TimerPlugin *plugin;
};

typedef struct _GTimeoutIdleEvent GTimeoutIdleEvent;
struct _GTimeoutIdleEvent {
	TimerCallback cb;
	gpointer cb_data;
	gboolean delayed;
};

gboolean plugin_initialize(TimerPlugin *plugin)
{
	ENTER_FUNC;
	plugin->remove_event = gtimeout_remove_event;
	plugin->get_time = gtimeout_get_time;
	plugin->plugin_deinit = gtimeout_plugin_deinit;
	plugin->set_event = gtimeout_set_event;
	plugin->time_changed = gtimeout_time_changed;

	plugin->can_power_up = FALSE;
	plugin->priority = 10;
	plugin->plugin_data = NULL;

	LEAVE_FUNC;
	return TRUE;
}

static gboolean _gtimeout_event_fire(gpointer event)
{
	GTimeoutTimerEvent *ev = (GTimeoutTimerEvent *)event;
	ENTER_FUNC;

	ev->plugin->plugin_data = NULL;

	if (ev->cb) {
		ev->cb(ev->cb_data, FALSE);
	}

	g_free(event);

	LEAVE_FUNC;
	return FALSE;
}

static gboolean _gtimeout_idle_event_fire(gpointer event) {
	GTimeoutIdleEvent *ev = event;
	ENTER_FUNC;

	if (ev->cb) {
		ev->cb(ev->cb_data, ev->delayed);
	}

	g_free(event);

	LEAVE_FUNC;
	return FALSE;
}

static gboolean gtimeout_set_event(TimerPlugin *plugin, time_t wanted_time, TimerCallback cb, TimerCancel cancel, gpointer user_data)
{
	GTimeoutTimerEvent *event = NULL;
	time_t time_now = time(NULL);
	ENTER_FUNC;

	DEBUG("wanted time = %lu", wanted_time);

	if (plugin == NULL || cb == NULL) {
		DEBUG("plugin == NULL || cb == NULL");
		LEAVE_FUNC;
		return FALSE;
	}

	plugin->remove_event(plugin);

	if (time_now > wanted_time) {
		GTimeoutIdleEvent *ev = g_new0(GTimeoutIdleEvent, 1);
		ev->cb = cb;
		ev->cb_data = user_data;
		ev->delayed = time_now > wanted_time + 10 ? TRUE : FALSE;
		g_idle_add(_gtimeout_idle_event_fire, ev);

		LEAVE_FUNC;
		return TRUE;
	}

	event = g_new0(GTimeoutTimerEvent, 1);

	event->timer_id = g_timeout_add((wanted_time - time_now) * 1000, _gtimeout_event_fire, event);

	if (event->timer_id == 0) {
		g_warning("Adding timeout failed.");
		g_free(event);

		LEAVE_FUNC;
		return FALSE;
	}
	
	event->timer_time = wanted_time;
	event->cb = cb;
	event->cancel = cancel;
	event->cb_data = user_data;

	event->plugin = plugin;

	plugin->plugin_data = (gpointer)event;

	LEAVE_FUNC;
	return TRUE;
}

static gboolean gtimeout_remove_event(TimerPlugin *plugin)
{
	ENTER_FUNC;
	if (plugin && plugin->plugin_data) {
		GTimeoutTimerEvent *event = (GTimeoutTimerEvent *)plugin->plugin_data;

		g_source_remove(event->timer_id);

		plugin->plugin_data = NULL;

		if (event->cancel) {
			event->cancel(event->cb_data);
		}

		g_free(event);

		LEAVE_FUNC;
		return TRUE;
	}
	
	LEAVE_FUNC;
	return FALSE;
}

static time_t gtimeout_get_time(TimerPlugin *plugin)
{
	ENTER_FUNC;
	if (plugin->plugin_data) {
		GTimeoutTimerEvent *event = (GTimeoutTimerEvent *)plugin->plugin_data;

		LEAVE_FUNC;
		return event->timer_time;
	}
	
	LEAVE_FUNC;
	return 0;
}

static void gtimeout_plugin_deinit(TimerPlugin *plugin)
{
	ENTER_FUNC;
	plugin->remove_event(plugin);
	LEAVE_FUNC;
}

static void gtimeout_time_changed(TimerPlugin *plugin)
{
	ENTER_FUNC;
	if (plugin->plugin_data) {
		GTimeoutTimerEvent *event = (GTimeoutTimerEvent *)plugin->plugin_data;
		time_t now = time(NULL);
		g_source_remove(event->timer_id);
		if (now > event->timer_time) {
			plugin->plugin_data = NULL;

			if (event->cb) {
				event->cb(event->cb_data, FALSE);
			}

			g_free(event);
		} else {
			event->timer_id = g_timeout_add((event->timer_time - now) * 1000, _gtimeout_event_fire, event);
		}
	}
	LEAVE_FUNC;
}

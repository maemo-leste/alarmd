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
#include <dbus/dbus.h>
#include <time.h>
#include <osso-log.h>
#include <string.h>
#include <glib/gfileutils.h>
#include "rpc-retutime.h"
#include "include/timer-interface.h"
#include "debug.h"

#define RETUTIME_DBUS_SIGNAL_RULE "type='signal',interface='org.kernel.kevent',path='/org/kernel/bus/platform/drivers/retu_rtc/alarm_expired'"
/* 23:59 */
#define RETUTIME_MAX_TIME 86340

static gboolean retu_set_event(TimerPlugin *plugin, time_t wanted_time, TimerCallback cb, TimerCancel cancel, gpointer user_data);
static gboolean retu_remove_event(TimerPlugin *plugin);
static time_t retu_get_time(TimerPlugin *plugin);
static void retu_time_changed(TimerPlugin *plugin);
static void retu_plugin_deinit(TimerPlugin *plugin);
static void _clock_set_alarm(time_t alarm_time);

static gboolean _retu_event_fire(gpointer event);
static gboolean _retu_idle_event_fire(gpointer event);

typedef struct _RetuTimerEvent RetuTimerEvent;
struct _RetuTimerEvent {
	guint timer_id;
	time_t timer_time;
	TimerCallback cb;
	TimerCancel cancel;
	gpointer cb_data;

	TimerPlugin *plugin;
};

typedef struct _RetuIdleEvent RetuIdleEvent;
struct _RetuIdleEvent {
	TimerCallback cb;
	gpointer cb_data;
	gboolean delayed;
};


gboolean plugin_initialize(TimerPlugin *plugin)
{
	ENTER_FUNC;

	plugin->set_event = retu_set_event;
	plugin->remove_event = retu_remove_event;
	plugin->get_time = retu_get_time;
	plugin->plugin_deinit = retu_plugin_deinit;
	plugin->time_changed = retu_time_changed;

	if (!g_file_test("/mnt/initfs/usr/bin/retutime", G_FILE_TEST_IS_REGULAR)) {
		DEBUG("No retutime found.");
		LEAVE_FUNC;
		return FALSE;
	}

	plugin->can_power_up = TRUE;
	plugin->priority = 5;
	plugin->plugin_data = NULL;

	LEAVE_FUNC;
	return TRUE;
}

static gboolean _retu_idle_event_fire(gpointer event) {
	RetuIdleEvent *ev = event;
	ENTER_FUNC;

	if (ev->cb) {
		ev->cb(ev->cb_data, ev->delayed);
	}

	g_free(event);

	LEAVE_FUNC;
	return FALSE;
}

static gboolean _retu_event_fire(gpointer event) {
	RetuTimerEvent *ev = (RetuTimerEvent *)event;
	ENTER_FUNC;

	ev->plugin->plugin_data = NULL;

	if (ev->cb) {
		ev->cb(ev->cb_data, FALSE);
	}

	g_free(event);

	if (retutime_query_alarm()) {
		retutime_ack_alarm();
	} else {
		retutime_disable_alarm();
	}

	LEAVE_FUNC;
	return FALSE;
}

static gboolean retu_set_event(TimerPlugin *plugin, time_t wanted_time, TimerCallback cb, TimerCancel cancel, gpointer user_data)
{
	RetuTimerEvent *event = NULL;
	time_t time_now = time(NULL);

	ENTER_FUNC;

	if (plugin == NULL || cb == NULL) {
		DEBUG("plugin == NULL || cb == NULL");
		LEAVE_FUNC;
		return FALSE;
	}

	plugin->remove_event(plugin);

	if (time_now > wanted_time) {
		RetuIdleEvent *ev = g_new0(RetuIdleEvent, 1);
		ev->cb = cb;
		ev->cb_data = user_data;
		ev->delayed = time_now > wanted_time + 
			(plugin->is_startup ? 120 : 10) ? TRUE : FALSE;
		g_idle_add(_retu_idle_event_fire, ev);

		LEAVE_FUNC;
		return TRUE;
	}
	
	event = g_new0(RetuTimerEvent, 1);

	event->timer_id = g_timeout_add((wanted_time - time_now) * 1000, _retu_event_fire, event);

	if (event->timer_id == 0) {
		ULOG_WARN_F("Adding timeout failed.");
		g_free(event);

		LEAVE_FUNC;
		return FALSE;
	}
	
	event->timer_time = wanted_time;
	event->cb = cb;
	event->cb_data = user_data;
	event->cancel = cancel;
	event->plugin = plugin;

	plugin->plugin_data = (gpointer)event;

	_clock_set_alarm(wanted_time);

	LEAVE_FUNC;
	return TRUE;
}

static gboolean retu_remove_event(TimerPlugin *plugin)
{
	ENTER_FUNC;

	if (plugin && plugin->plugin_data) {
		RetuTimerEvent *event = (RetuTimerEvent *)plugin->plugin_data;

		g_source_remove(event->timer_id);

		if (event->cancel) {
			event->cancel(event->cb_data);
		}

		plugin->plugin_data = NULL;

		g_free(event);

		retutime_disable_alarm();

		LEAVE_FUNC;
		return TRUE;
	}
	
	LEAVE_FUNC;
	return FALSE;
}

static time_t retu_get_time(TimerPlugin *plugin)
{
	ENTER_FUNC;
	if (plugin && plugin->plugin_data) {
		RetuTimerEvent *event = (RetuTimerEvent *)plugin->plugin_data;

		LEAVE_FUNC;
		return event->timer_time;
	}

	LEAVE_FUNC;
	return 0;
}

static void retu_plugin_deinit(TimerPlugin *plugin)
{
	ENTER_FUNC;
	if (plugin && plugin->plugin_data) {
		RetuTimerEvent *event = (RetuTimerEvent *)plugin->plugin_data;

		g_source_remove(event->timer_id);

		if (event->cancel) {
			event->cancel(event->cb_data);
		}

		plugin->plugin_data = NULL;

		g_free(event);
	}
	LEAVE_FUNC;
}

static void retu_time_changed(TimerPlugin *plugin)
{
	ENTER_FUNC;
	if (plugin && plugin->plugin_data) {
		RetuTimerEvent *event = (RetuTimerEvent *)plugin->plugin_data;
		time_t now = time(NULL);
		g_source_remove(event->timer_id);
		if (now > event->timer_time) {
			plugin->plugin_data = NULL;

			if (event->cb) {
				event->cb(event->cb_data, TRUE);
			}

			g_free(event);
		} else {
			event->timer_id = g_timeout_add((event->timer_time - now) * 1000, _retu_event_fire, event);
		}
	}
	LEAVE_FUNC;
}

static void _clock_set_alarm(time_t alarm_time)
{
	time_t now = time(NULL);
	ENTER_FUNC;

	if (alarm_time - now > RETUTIME_MAX_TIME) {
		retutime_set_alarm_time(now + RETUTIME_MAX_TIME);
	} else {
		if (!retutime_set_alarm_time(alarm_time)) {
			retutime_set_alarm_time(alarm_time + 60);
		}
	}

	LEAVE_FUNC;
}

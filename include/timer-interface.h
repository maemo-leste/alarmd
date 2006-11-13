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

#ifndef TIMER_INTERFACE_H
#define TIMER_INTERFACE_H

#include <glib/gtypes.h>
#include <time.h>

/**
 * SECTION:timer-interface
 * @short_description: The API for timer plugins.
 *
 * Here is defined the API that the timer plugins need to implement.
 **/

/**
 * TimerCallback:
 * @user_data: user data set when the callback was connected.
 * @delayed: TRUE if the event is happening after supposed time.
 *
 * Callback notify for timed events.
 **/
typedef void (*TimerCallback)(gpointer user_data, gboolean delayed);

/**
 * TimerCancel:
 * @user_data: user data set when the callback was connected.
 *
 * Callback called when an event is taken off the timer.
 **/
typedef void (*TimerCancel)(gpointer user_data);

/**
 * TimerPlugin:
 * @is_startup: Boolean that indicates if alarmd is starting up.
 * @set_event: Function the events should call when they put self on the timer.
 * @remove_event: Function that removes the currently timed event.
 * @get_time: Gets the time of the currently timed event.
 * @time_changed: Indicates that the system time has changed.
 * @plugin_deinit: Deinitializes the plugin.
 * @priority: Priority of the plugin.
 * @can_power_up: TRUE if the plugin can power up the device.
 * @plugin_data: Plugins internal data.
 *
 * Struct that defines the API for a timer plugin.
 **/
typedef struct _TimerPlugin TimerPlugin;
struct _TimerPlugin {
	/* Filled in by alarmd: */
	gboolean is_startup;

	/* Filled by the plugin: */
	gboolean (*set_event)(TimerPlugin *plugin, time_t wanted_time, TimerCallback cb, TimerCancel cancel, gpointer user_data);
	gboolean (*remove_event)(TimerPlugin *plugin);
	time_t (*get_time)(TimerPlugin *plugin);
	void (*time_changed)(TimerPlugin *plugin);
	void (*plugin_deinit)(TimerPlugin *plugin);

	guint priority;
	gboolean can_power_up;

	gpointer plugin_data;
};

/**
 * plugin_initialize:
 * @plugin: Pointer to the struct the plugin should fill.
 *
 * Function all timer plugins should implement. Fills in the struct that
 * describes the plugin.
 * Returns: TRUE if plugin initialization was successful.
 **/
gboolean plugin_initialize(TimerPlugin *plugin);

#endif

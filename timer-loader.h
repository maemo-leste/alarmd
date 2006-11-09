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

#ifndef _TIMER_LOADER_H_
#define _TIMER_LOADER_H_

#include "include/timer-interface.h"

/**
 * SECTION:timer-loader
 * @short_description: Timer plugin loader subsystem
 *
 * These functions are used to un/load timer plugins and to get them.
 **/

/**
 * timer_plugins_set_startup:
 * @plugins: Plugin list as returned by #load_timer_plugins.
 * @is_startup: Boolean indicating the startup status of alarmd.
 *
 * Sets the is_startup field for each plugin.
 **/
void timer_plugins_set_startup(GSList *plugins, gboolean is_startup);

/**
 * close_timer_plugins:
 * @plugins: Plugin list as returned by #load_timer_plugins.
 *
 * Closes all plugins and frees all data.
 **/
void close_timer_plugins(GSList *plugins);

/**
 * load_timer_plugins:
 * @path: Directory to load the plugins from.
 *
 * Loads all timer plugins from given directory. Gets the plugin priorities
 * and closes all but the highest priority power up and non power up ones.
 * Returns: List of plugins.
 **/
GSList *load_timer_plugins(const gchar *path);

/**
 * timers_get_plugin:
 * @plugins: List of plugins as returned by #load_timer_plugins.
 * @need_power_up: TRUE if the plugin wanted should have the power up
 * functionality.
 *
 * Gets one plugin (and loads it if necessary) from the list. The one with
 * the highest priority is returned.
 * Returns: The highest priority plugin.
 **/
TimerPlugin *timers_get_plugin(GSList *plugins, gboolean need_power_up);

#endif /* _TIMER_LOADER_H_ */

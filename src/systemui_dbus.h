/**
 * @brief DBus constants for system ui alarm service
 *
 * @file systemui_dbus.h
 *
 * @author Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * Placeholder fo constants used for communication with systemui
 * daemon over dbus.
 *
 * <p>
 *
 * These should be defined in some system ui package.
 *
 * <p>
 * This file is part of Alarmd
 *
 * Copyright (C) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Contact: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * Alarmd is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * Alarmd is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Alarmd; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef ALARMD_SYSTEMUI_DBUS_H_
# define ALARMD_SYSTEMUI_DBUS_H_

# include <systemui/dbus-names.h>
# include <systemui/alarm_dialog-dbus-names.h>

# if 01 /* so called "new api" that was never taken to use
        * due to limitations in the way systemui passes
        * dbus messages to plugins */

/** @name DBus methods for alarmd
 **/

/*@{*/

/** @brief PLACEHOLDER
 *
 * Adds alarm events into dialog queue.
 *
 * @param cookies : ARRAY of INT32
 *
 * @returns inqueue : ARRAY of INT32
 **/
#  define SYSTEMUI_ALARM_ADD "systemui_alarm_add"

/** @brief PLACEHOLDER
 *
 * Removes alarm events from dialog queue.
 *
 * @param cookies : ARRAY of INT32
 *
 * @returns inqueue : ARRAY of INT32
 **/
#  define SYSTEMUI_ALARM_DEL "systemui_alarm_del"

/** @brief PLACEHOLDER
 *
 * Queries alarm events in dialog queue.
 *
 * @param n/a
 *
 * @returns cookies : ARRAY of INT32
 **/
#  define SYSTEMUI_ALARM_QUERY "systemui_alarm_query"

/*@}*/
# endif

#endif // ALARMD_SYSTEMUI_DBUS_H_

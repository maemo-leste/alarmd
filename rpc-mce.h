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

#ifndef RPC_MCE_H
#define RPC_MCE_H

#include <dbus/dbus.h>
#include <glib.h>

/**
 * SECTION:rpc-mce
 * @short_description: Helpers for communicating with the Mode Control Entity.
 *
 * These functions can be used to do common operations on the mce.
 **/

/**
 * mce_set_alarm_visibility:
 * @system_bus: The dbus (system) bus to send the mssage on.
 * @visible: TRUE if a alarm dialog is visible, false otherwise.
 *
 * Notifies mce about alarm dialog visibility.
 **/
void mce_set_alarm_visibility(DBusConnection *system_bus, gboolean visible);

/**
 * mce_request_powerup:
 * @system_bus: The dbus (system) bus to send the mssage on.
 *
 * Reuqests device power up from mce.
 **/
void mce_request_powerup(DBusConnection *system_bus);

/**
 * mce_request_shutdown:
 * @system_bus: The dbus (system) bus to send the mssage on.
 *
 * Requests device shutdown from mce.
 **/
void mce_request_shutdown(DBusConnection *system_bus);

#endif

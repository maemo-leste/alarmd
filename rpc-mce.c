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

#include <dbus/dbus.h>
#include <glib.h>
#include <mce/dbus-names.h>
#include <mce/mode-names.h>
#include "rpc-mce.h"
#include "rpc-dbus.h"
#include "debug.h"

void mce_set_alarm_visibility(DBusConnection *system_bus, gboolean visible)
{
       ENTER_FUNC;

       (void)system_bus;
       (void)visible;

       /* Noop in IT2006. */

       LEAVE_FUNC;
}

void mce_request_powerup(DBusConnection *system_bus)
{
       ENTER_FUNC;
       dbus_do_call(system_bus, NULL, FALSE,
                       MCE_SERVICE,
                       MCE_REQUEST_PATH,
                       MCE_REQUEST_IF,
                       MCE_POWERUP_REQ,
                       DBUS_TYPE_INVALID);
       LEAVE_FUNC;
}

void mce_request_shutdown(DBusConnection *system_bus)
{
       ENTER_FUNC;
       dbus_do_call(system_bus, NULL, FALSE,
                       MCE_SERVICE,
                       MCE_REQUEST_PATH,
                       MCE_REQUEST_IF,
                       MCE_SHUTDOWN_REQ,
                       DBUS_TYPE_INVALID);
       LEAVE_FUNC;
}

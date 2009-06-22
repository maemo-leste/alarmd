/* ========================================================================= *
 *
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
 *
 * ========================================================================= */

#include "ipc_statusbar.h"
#include "dbusif.h"

#include <clock-plugin-dbus.h>

int
ipc_statusbar_alarm_hide(DBusConnection *session_bus)
{
  return dbusif_method_call(session_bus, 0,
                            STATUSAREA_CLOCK_SERVICE,
                            STATUSAREA_CLOCK_PATH,
                            STATUSAREA_CLOCK_INTERFACE,
                            STATUSAREA_CLOCK_NO_ALARMS,
                            DBUS_TYPE_INVALID);
}

int
ipc_statusbar_alarm_show(DBusConnection *session_bus)
{
  return dbusif_method_call(session_bus, 0,
                            STATUSAREA_CLOCK_SERVICE,
                            STATUSAREA_CLOCK_PATH,
                            STATUSAREA_CLOCK_INTERFACE,
                            STATUSAREA_CLOCK_ALARM_SET,
                            DBUS_TYPE_INVALID);
}

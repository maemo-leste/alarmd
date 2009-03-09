/* ========================================================================= *
 * File: ipc_statusbar.c
 *
 * Copyright (C) 2008 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 18-Sep-2008 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#include "ipc_statusbar.h"
#include "dbusif.h"

#include <clock-plugin-dbus.h>

int
statusbar_alarm_hide(DBusConnection *session_bus)
{
  return dbusif_method_call(session_bus, 0,
                            STATUSAREA_CLOCK_SERVICE,
                            STATUSAREA_CLOCK_PATH,
                            STATUSAREA_CLOCK_INTERFACE,
                            STATUSAREA_CLOCK_NO_ALARMS,
                            DBUS_TYPE_INVALID);
}

int
statusbar_alarm_show(DBusConnection *session_bus)
{
  return dbusif_method_call(session_bus, 0,
                            STATUSAREA_CLOCK_SERVICE,
                            STATUSAREA_CLOCK_PATH,
                            STATUSAREA_CLOCK_INTERFACE,
                            STATUSAREA_CLOCK_ALARM_SET,
                            DBUS_TYPE_INVALID);
}

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

#include "alarmd_config.h"

#include "ipc_dsme.h"

#include "dbusif.h"
#include "logging.h"

#include "missing_dbus.h"

#include <string.h>

int
ipc_dsme_req_powerup(DBusConnection *system_bus)
{
  return dbusif_method_call(system_bus, 0,
                            DSME_SERVICE,
                            DSME_REQUEST_PATH,
                            DSME_REQUEST_IF,
                            DSME_REQ_POWERUP,
                            DBUS_TYPE_INVALID);
}

#ifdef DEAD_CODE
int
ipc_dsme_req_reboot(DBusConnection *system_bus)
{
  return dbusif_method_call(system_bus, 0,
                            DSME_SERVICE,
                            DSME_REQUEST_PATH,
                            DSME_REQUEST_IF,
                            DSME_REQ_REBOOT,
                            DBUS_TYPE_INVALID);
}
#endif

#ifdef DEAD_CODE
int
ipc_dsme_req_shutdown(DBusConnection *system_bus)
{
  return dbusif_method_call(system_bus, 0,
                            DSME_SERVICE,
                            DSME_REQUEST_PATH,
                            DSME_REQUEST_IF,
                            DSME_REQ_SHUTDOWN,
                            DBUS_TYPE_INVALID);
}
#endif

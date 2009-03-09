/* ========================================================================= *
 * File: ipc_dsme.c
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

#include "ipc_dsme.h"

#include "dbusif.h"
#include "logging.h"

#include "missing_dbus.h"

#include <string.h>

int
dsme_req_powerup(DBusConnection *system_bus)
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
dsme_set_alarm_visibility(DBusConnection *system_bus, int visible)
{
  const char *mode = visible ? "visible" : "off";

  return dbusif_method_call(system_bus, 0,
                            DSME_SERVICE,
                            DSME_REQUEST_PATH,
                            DSME_REQUEST_IF,
                            DSME_REQ_ALARM_MODE_CHANGE,
                            DBUS_TYPE_STRING, &mode,
                            DBUS_TYPE_INVALID);
}
#endif

#ifdef DEAD_CODE
int
dsme_req_reboot(DBusConnection *system_bus)
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
dsme_req_shutdown(DBusConnection *system_bus)
{
  return dbusif_method_call(system_bus, 0,
                            DSME_SERVICE,
                            DSME_REQUEST_PATH,
                            DSME_REQUEST_IF,
                            DSME_REQ_SHUTDOWN,
                            DBUS_TYPE_INVALID);
}
#endif

#ifdef DEAD_CODE
char *
dsme_get_version(DBusConnection *system_bus)
{
  char        *res = 0;
  DBusMessage *rsp = 0;

  /* NB: this uses dbus_connection_send_with_reply_and_block()
   *     to a service that might be making queries to us
   *     and might result in deadlock until dbus timeouts
   */
  dbusif_method_call(system_bus, &rsp,
                     DSME_SERVICE,
                     DSME_REQUEST_PATH,
                     DSME_REQUEST_IF,
                     DSME_GET_VERSION,
                     DBUS_TYPE_INVALID);

  if( rsp != 0 )
  {
    char *ver = 0;
    dbusif_reply_parse_args(rsp, DBUS_TYPE_STRING, &ver);
    if( ver != 0 )
    {
      res = strdup(ver);
    }
  }

  return res;
}
#endif

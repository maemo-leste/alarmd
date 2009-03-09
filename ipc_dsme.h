/* ========================================================================= *
 * File: ipc_dsme.h
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

#ifndef IPC_DSME_H_
#define IPC_DSME_H_

#include <dbus/dbus.h>

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

int   dsme_req_powerup         (DBusConnection *system_bus);
int   dsme_set_alarm_visibility(DBusConnection *system_bus, int visible);
int   dsme_req_reboot          (DBusConnection *system_bus);
int   dsme_req_shutdown        (DBusConnection *system_bus);
char *dsme_get_version         (DBusConnection *system_bus);

#ifdef __cplusplus
};
#endif

#endif /* IPC_DSME_H_ */

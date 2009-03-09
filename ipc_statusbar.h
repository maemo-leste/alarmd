/* ========================================================================= *
 * File: ipc_statusbar.h
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

#ifndef IPC_STATUSBAR_H_
#define IPC_STATUSBAR_H_

#include <dbus/dbus.h>

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

int statusbar_alarm_hide(DBusConnection *session_bus);
int statusbar_alarm_show(DBusConnection *session_bus);

#ifdef __cplusplus
};
#endif

#endif /* IPC_STATUSBAR_H_ */

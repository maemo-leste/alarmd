/* ========================================================================= *
 * File: ipc_systemui.h
 *
 * Copyright (C) 2008 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 21-May-2008 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#ifndef IPC_SYSTEMUI_H_
#define IPC_SYSTEMUI_H_

#include <dbus/dbus.h>
#include "libalarm.h"

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

int       systemui_add_dialog   (DBusConnection *conn,
                                 const cookie_t *cookie, int count);

int       systemui_cancel_dialog(DBusConnection *conn,
                                 const cookie_t *cookie, int count);

int       systemui_query_dialog (DBusConnection *conn);

void      systemui_set_ack_callback(void (*fn)(dbus_int32_t *, int));
void      systemui_set_service_callback(const char *(*fn)(void));

#ifdef __cplusplus
};
#endif

#endif /* IPC_SYSTEMUI_H_ */

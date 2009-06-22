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

#ifndef IPC_SYSTEMUI_H_
# define IPC_SYSTEMUI_H_

# include <dbus/dbus.h>
# include "libalarm.h"

# ifdef __cplusplus
extern "C" {
# elif 0
} /* fool JED indentation ... */
# endif

void ipc_systemui_set_ack_callback    (void (*fn)(dbus_int32_t *, int));
void ipc_systemui_set_service_callback(const char *(*fn)(void));

int  ipc_systemui_add_dialog          (DBusConnection *conn, const cookie_t *cookie, int count);
int  ipc_systemui_cancel_dialog       (DBusConnection *conn, const cookie_t *cookie, int count);
int  ipc_systemui_query_dialog        (DBusConnection *conn);

# ifdef __cplusplus
};
# endif

#endif /* IPC_SYSTEMUI_H_ */

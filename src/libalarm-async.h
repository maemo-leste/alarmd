/**
 * @brief DBus helpers for making asynchronous alarmd method calls
 *
 * @file libalarm-async.h
 *
 * @author Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * If client application must not be blocked by alarmd method calls,
 * it must use dbus_connection_send_with_reply() instead of normal
 * libalarm functions.
 *
 * These helper functions can be used for constructing method call
 * messages for use with pending dbus calls and parsing the replies
 * sent by alarmd or dbus daemon.
 *
 * See alarmd source package testing/asynctest.c for examples of use.
 *
 * Note: DBus system bus should be used for communication with
 * alarmd even though currently alarmd will handle method calls
 * via session bus too.
 *
 * <p>
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
 */

#ifndef LIBALARM_ASYNC_H_
#define LIBALARM_ASYNC_H_

#include "libalarm.h"

#include <dbus/dbus.h>

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

#pragma GCC visibility push(default)

/** @name Helpers for ALARMD_EVENT_UPDATE
 */

/*@{*/

/** \brief construct update method call message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_event_update() for details.
 */
DBusMessage *alarmd_event_update_encode_req (const alarm_event_t *event);

/** \brief parse update method reply message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_event_update() for details.
 */
cookie_t alarmd_event_update_decode_rsp (DBusMessage *rsp);

/*@}*/

/** @name Helpers for ALARMD_EVENT_ADD
 */

/*@{*/

/** \brief construct add method call message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_event_add_valist() for details.
 */
DBusMessage *alarmd_event_add_valist_encode_req (const alarm_event_t *event, int type, va_list va);

/** \brief parse update method reply message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_event_add_valist() for details.
 */
cookie_t alarmd_event_add_valist_decode_rsp (DBusMessage *rsp);

/** \brief construct add method call message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_event_add_with_dbus_params() for details.
 */
DBusMessage *alarmd_event_add_with_dbus_params_encode_req(const alarm_event_t *event, int type, ...);

/** \brief parse update method reply message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_event_add_with_dbus_params() for details.
 */
cookie_t alarmd_event_add_with_dbus_params_decode_rsp(DBusMessage *rsp);

/** \brief construct add method call message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_event_add() for details.
 */
DBusMessage *alarmd_event_add_encode_req (const alarm_event_t *event);

/** \brief parse update method reply message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_event_add() for details.
 */
cookie_t alarmd_event_add_decode_rsp (DBusMessage *rsp);

/*@}*/

/** @name Helpers for ALARMD_EVENT_GET
 */

/*@{*/

/** \brief construct get method call message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_event_get() for details.
 */
DBusMessage *alarmd_event_get_encode_req (cookie_t cookie);

/** \brief parse update method reply message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_event_get() for details.
 */
alarm_event_t *alarmd_event_get_decode_rsp (DBusMessage *rsp);

/*@}*/

/** @name Helpers for ALARMD_EVENT_DEL
 */

/*@{*/

/** \brief construct del method call message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_event_del() for details.
 */
DBusMessage *alarmd_event_del_encode_req (cookie_t cookie);

/** \brief parse update method reply message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_event_del() for details.
 */
int alarmd_event_del_decode_rsp (DBusMessage *rsp);

/*@}*/

/** @name Helpers for ALARMD_EVENT_QUERY
 */

/*@{*/

/** \brief construct query method call message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_event_query() for details.
 */
DBusMessage *alarmd_event_query_encode_req (const time_t first, const time_t last, int32_t flag_mask, int32_t flags, const char *appid);

/** \brief parse update method reply message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_event_query() for details.
 */
cookie_t *alarmd_event_query_decode_rsp (DBusMessage *rsp);

/*@}*/

/** @name Helpers for ALARMD_SNOOZE_GET
 */

/*@{*/

/** \brief construct get default snooze method call message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_get_default_snooze() for details.
 */
DBusMessage *alarmd_get_default_snooze_encode_req (void);

/** \brief parse update method reply message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_get_default_snooze() for details.
 */
int alarmd_get_default_snooze_decode_rsp (DBusMessage *rsp);

/*@}*/

/** @name Helpers for ALARMD_SNOOZE_SET
 */

/*@{*/

/** \brief construct set default snooze method call message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_set_default_snooze() for details.
 */
DBusMessage *alarmd_set_default_snooze_encode_req (unsigned int snooze);

/** \brief parse update method reply message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_set_default_snooze() for details.
 */
int alarmd_set_default_snooze_decode_rsp (DBusMessage *rsp);

/*@}*/

/** @name Helpers for ALARMD_DIALOG_RSP
 */

/*@{*/

/** \brief construct ack dialog message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_ack_dialog() for details.
 */
DBusMessage *alarmd_ack_dialog_encode_req (cookie_t cookie, int button);

/** \brief parse update method call message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_ack_dialog() for details.
 */
int alarmd_ack_dialog_decode_rsp (DBusMessage *rsp);

/*@}*/

/** @name Helpers for ALARMD_DIALOG_ACK
 */

/*@{*/

/** \brief construct ack queue message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_ack_queue() for details.
 */
DBusMessage *alarmd_ack_queue_encode_req (cookie_t *cookies, int count);

/** \brief parse update method reply message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_ack_queue() for details.
 */
int alarmd_ack_queue_decode_rsp (DBusMessage *rsp);

/*@}*/

/** @name Helpers for debug
 */

/*@{*/

/** \brief construct set debug message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_set_debug() for details.
 */
DBusMessage *alarmd_set_debug_encode_req (unsigned mask_set, unsigned mask_clr, unsigned flag_set, unsigned flag_clr);

/** \brief parse update method reply message
 *
 *  @since 1.1.8
 *
 *  See #alarmd_set_debug() for details.
 */
int alarmd_set_debug_decode_rsp (DBusMessage *rsp);

/*@}*/

#pragma GCC visibility pop

#ifdef __cplusplus
};
#endif

#endif /* LIBALARM_ASYNC_H_ */

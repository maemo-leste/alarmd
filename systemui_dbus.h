/**
 * @brief DBus constants for system ui alarm service
 *
 * @file systemui_dbus.h
 *
 * Placeholder fo constants used for communication with systemui
 * daemon over dbus.
 *
 * <p>
 *
 * These should be defined in some system ui package.
 *
 * <p>
 * Copyright (C) 2008 Nokia. All rights reserved.
 *
 * @author Simo Piiroinen <simo.piiroinen@nokia.com>
 */

#ifndef SYSTEMUI_DBUS_H
#define SYSTEMUI_DBUS_H

#include <systemui/dbus-names.h>

// QUARANTINE #define SYSTEMUI_SERVICE       "com.nokia.system_ui"
// QUARANTINE #define SYSTEMUI_REQUEST_IF    "com.nokia.system_ui.request"
// QUARANTINE #define SYSTEMUI_SIGNAL_IF     "com.nokia.system_ui.signal"
// QUARANTINE #define SYSTEMUI_REQUEST_PATH  "/com/nokia/system_ui/request"
// QUARANTINE #define SYSTEMUI_SIGNAL_PATH   "/com/nokia/system_ui/signal"
// QUARANTINE #define SYSTEMUI_QUIT_REQ      "quit"
// QUARANTINE #define SYSTEMUI_STARTED_SIG   "system_ui_started"

#define FAKESYSTEMUI_SERVICE SYSTEMUI_SERVICE".fake"

// QUARANTINE /** @name System UI D-Bus Daemon
// QUARANTINE  **/
// QUARANTINE
// QUARANTINE /*@{*/
// QUARANTINE
// QUARANTINE /** D-Bus service name */
// QUARANTINE #define SYSTEMUI_SERVICE "com.nokia.systemui"
// QUARANTINE
// QUARANTINE /** Alternate debugging D-Bus service name */
// QUARANTINE #define FAKESYSTEMUI_SERVICE SYSTEMUI_SERVICE".fake"
// QUARANTINE
// QUARANTINE /** D-Bus object path */
// QUARANTINE #define SYSTEMUI_REQUEST_PATH "/com/nokia/systemui"
// QUARANTINE
// QUARANTINE /** D-Bus interface name */
// QUARANTINE #define SYSTEMUI_REQUEST_IF "com.nokia.systemui"
// QUARANTINE
// QUARANTINE /*@}*/

#include <systemui/alarm_dialog-dbus-names.h>

// QUARANTINE #define SYSTEMUI_ALARM_OPEN_REQ "alarm_open"
// QUARANTINE #define SYSTEMUI_ALARM_CLOSE_REQ "alarm_close"

/** @name DBus methods for alarmd
 **/

/*@{*/

/** @brief PLACEHOLDER
 *
 * Adds alarm events into dialog queue.
 *
 * @param cookies : ARRAY of INT32
 *
 * @returns inqueue : ARRAY of INT32
 **/
#define SYSTEMUI_ALARM_ADD "systemui_alarm_add"

/** @brief PLACEHOLDER
 *
 * Removes alarm events from dialog queue.
 *
 * @param cookies : ARRAY of INT32
 *
 * @returns inqueue : ARRAY of INT32
 **/
#define SYSTEMUI_ALARM_DEL "systemui_alarm_del"

/** @brief PLACEHOLDER
 *
 * Queries alarm events in dialog queue.
 *
 * @param n/a
 *
 * @returns cookies : ARRAY of INT32
 **/
#define SYSTEMUI_ALARM_QUERY "systemui_alarm_query"

/*@}*/

#endif

/**
 * @brief DBus constants for alarm daemon
 *
 * @file alarm_dbus.h
 *
 * @author Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * The Constants used for communication with alarm daemon over dbus.
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

#ifndef ALARM_DBUS_H
#define ALARM_DBUS_H

/** @name Alarm D-Bus daemon
 **/

/*@{*/

/** D-Bus service name */
#define ALARMD_SERVICE "com.nokia.alarmd"

/** D-Bus object path */
#define ALARMD_PATH "/com/nokia/alarmd"

/** D-Bus interface name */
#define ALARMD_INTERFACE "com.nokia.alarmd"

/*@}*/

/** @name DBus methods for all clients
 **/

/*@{*/

/**
 * Adds an alarm event into the queue.
 * Returns unique identifier for queued event.
 *
 * @param actions        : UINT32
 * @param cookie         : INT32
 * @param trigger        : INT32
 * @param title          : STRING
 * @param message        : STRING
 * @param sound          : STRING
 * @param icon           : STRING
 * @param flags          : UINT32
 * @param alarm_time     : INT32
 * @param alarm_year     : INT32
 * @param alarm_mon      : INT32
 * @param alarm_mday     : INT32
 * @param alarm_hour     : INT32
 * @param alarm_min      : INT32
 * @param alarm_sec      : INT32
 * @param alarm_tz       : STRING
 * @param recur_secs     : INT32
 * @param recur_count    : INT32
 * @param snooze_secs    : INT32
 * @param snooze_total   : INT32
 * @param action         : ARRAY of STRUCT {
 * <br> flags            : UINT32 - repeats 'actions' times
 * <br> label            : STRING
 * <br> exec_command     : STRING
 * <br> dbus_interface   : STRING
 * <br> dbus_service     : STRING
 * <br> dbus_path        : STRING
 * <br> dbus_name        : STRING
 * <br> dbus_args        : STRING
 * <br> }
 *
 * @returns cookie : INT32, -1 = error
 **/
#define ALARMD_EVENT_ADD "add_event"

/**
 * Fetches alarm event details
 *
 * @param cookie : INT32
 *
 * @returns actions    : UINT32
 * <br> cookie         : INT32
 * <br> trigger        : INT32
 * <br> title          : STRING
 * <br> message        : STRING
 * <br> sound          : STRING
 * <br> icon           : STRING
 * <br> flags          : UINT32
 * <br> alarm_time     : INT32
 * <br> alarm_year     : INT32
 * <br> alarm_mon      : INT32
 * <br> alarm_mday     : INT32
 * <br> alarm_hour     : INT32
 * <br> alarm_min      : INT32
 * <br> alarm_sec      : INT32
 * <br> alarm_tz       : STRING
 * <br> recur_secs     : INT32
 * <br> recur_count    : INT32
 * <br> snooze_secs    : INT32
 * <br> snooze_total   : INT32
 * <br> action         : ARRAY of STRUCT {
 * <br> flags          : UINT32
 * <br> label          : STRING
 * <br> exec_command   : STRING
 * <br> dbus_interface : STRING
 * <br> dbus_service   : STRING
 * <br> dbus_path      : STRING
 * <br> dbus_name      : STRING
 * <br> dbus_args      : STRING
 * <br> }
 *
 **/
#define ALARMD_EVENT_GET "get_event"

/**
 * Removes event from the queue.
 *
 * @param cookie : INT32
 *
 * @returns deleted : BOOLEAN
 **/
#define ALARMD_EVENT_DEL "del_event"

/**
 * Queries the queue for matching events.
 *
 * Returns cookies for events that have
 * start_time <= trigger time <= stop_time
 * and (flags & flag_mask) == flag_want.
 *
 * Special case: start_time and stop_time
 * are both zero -> get all events.
 *
 * @param start_time : INT32 [time_t]
 * @param stop_time  : INT32 [time_t]
 * @param flag_mask  : INT32
 * @param flag_want  : INT32
 * @param app_name   : STRING
 *
 * @returns cookies : ARRAY of INT32
 *
 **/
#define ALARMD_EVENT_QUERY "query_event"

/**
 * Updates an existing event.
 *
 * Performs delete and add in one DBus transaction.
 * The cookie returned will differ from the original.
 *
 * @param actions        : UINT32
 * @param cookie         : INT32
 * @param trigger        : INT32
 * @param title          : STRING
 * @param message        : STRING
 * @param sound          : STRING
 * @param icon           : STRING
 * @param flags          : UINT32
 * @param alarm_time     : INT32
 * @param alarm_year     : INT32
 * @param alarm_mon      : INT32
 * @param alarm_mday     : INT32
 * @param alarm_hour     : INT32
 * @param alarm_min      : INT32
 * @param alarm_sec      : INT32
 * @param alarm_tz       : STRING
 * @param recur_secs     : INT32
 * @param recur_count    : INT32
 * @param snooze_secs    : INT32
 * @param snooze_total   : INT32
 * @param action         : ARRAY of STRUCT {
 * <br> flags            : UINT32 - repeats 'actions' times
 * <br> label            : STRING
 * <br> exec_command     : STRING
 * <br> dbus_interface   : STRING
 * <br> dbus_service     : STRING
 * <br> dbus_path        : STRING
 * <br> dbus_name        : STRING
 * <br> dbus_args        : STRING
 * <br> }
 *
 * @returns cookie : INT32, -1 = error
 **/
#define ALARMD_EVENT_UPDATE "update_event"

/**
 * Set default snooze time in seconds.
 *
 * @param    snooze : UINT32
 *
 * @returns success : BOOLEAN
 **/
#define ALARMD_SNOOZE_SET "set_snooze"

/**
 * Get default snooze time in seconds.
 *
 * @param n/a
 *
 * @returns snooze : UINT32
 **/
#define ALARMD_SNOOZE_GET "get_snooze"

/*@}*/

/** @name DBus methods for SystemUI
 **/

/*@{*/

/**
 * Acknowledge alarm events queued for dialog service.
 *
 * @param cookies : ARRAY of INT32
 *
 * @returns received : BOOLEAN
 **/
#define ALARMD_DIALOG_ACK "ack_dialog"

/**
 * Acknowledge alarm event action taken by the user
 *
 * @param cookie : INT32
 * @param action : INT32
 *
 * @returns received : BOOLEAN
 **/

#define ALARMD_DIALOG_RSP "rsp_dialog"

/*@}*/

/** @name DBus signals emitted by alarmd
 **/

/*@{*/

/**
 * Alarm queue status change indication.
 *
 * Number of currently triggered alarms
 * and trigger times for alarms that
 * power up to desktop, power up to
 * acting dead and alarms without power
 * up flags.
 *
 * @param alarms  : INT32
 * @param desktop : INT32
 * @param actdead : INT32
 * @param noboot  : INT32
 **/
#define ALARMD_QUEUE_STATUS_IND "queue_status_ind"

/**
 * System time or timezone change handled indication.
 *
 * @since v1.0.11
 *
 * When timezone or system time changes, some alarms
 * might need to be rescheduled.
 *
 * This signal is sent after all state transitions
 * due to system time or timezone changes are
 * handled by alarmd and all events hold valid
 * trigger time.
 **/
#define ALARMD_TIME_CHANGE_IND "time_change_ind"

/*@}*/

#endif

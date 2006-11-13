/**
 * This file is part of alarmd
 *
 * Contact Person: David Weinehall <david.weinehall@nokia.com>
 *
 * Copyright (C) 2006 Nokia Corporation
 * alarmd and libalarm are free software; you can redistribute them
 * and/or modify them under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 *
 * alarmd and libalarm are distributed in the hope that they will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef ALARM_DBUS_H
#define ALARM_DBUS_H

/**
 * SECTION:alarm_dbus
 * @short_description: The DBus API.
 *
 * Defines DBus service and method names to communicate with alarmd.
 **/

/**
 * ALARMD_SERVICE:
 *
 * The name of the alarmd service.
 **/
#define ALARMD_SERVICE "com.nokia.alarmd"

/**
 * ALARMD_PATH:
 *
 * The object path for the alarmd daemon.
 **/
#define ALARMD_PATH "/com/nokia/alarmd"

/**
 * ALARMD_INTERFACE:
 *
 * The interface the commands use.
 **/
#define ALARMD_INTERFACE "com.nokia.alarmd"

/**
 * ALARM_EVENT_ADD:
 *
 * Adds an event into the queue.
 *
 * Parameters:
 *
 *   OBJECT_PATH: Name of the object type.
 *
 *   UINT32: Count of arguments for the object.
 *
 *   STRING: Parameter 1 name.
 *
 *   TYPE: Parameter 1 value.
 *
 *   STRING: Parameter 2 name.
 *
 *   TYPE: Parameter 2 value.
 *   
 *   ...
 * 
 * Return:
 * 
 *  INT32: unique id for the alarm
 **/
#define ALARM_EVENT_ADD "add_event"

/**
 * ALARM_EVENT_DEL:
 *
 * Removes event from the queue.
 * 
 * Parameters:
 *
 *   INT32: The id of the alarm event.
 *
 * Return:
 *
 *   BOOL: TRUE on success.
 **/
#define ALARM_EVENT_DEL "del_event"

/**
 * ALARM_EVENT_QUERY:
 *
 * Queries the queue for matching events.
 *
 * Parameters:
 *
 *   UINT64: Start time of query (seconds since Jan 1 1970 00:00:00 UTC.
 *   
 *   UINT64: End time of query.
 *
 *   INT32: Flag mask to select events (0 to get all.
 *
 *   INT32: Wanted flag values.
 *
 * Return:
 *
 *   Array of INT32s: The event id's.
 **/
#define ALARM_EVENT_QUERY "query_event"

/**
 * ALARM_EVENT_GET:
 *
 * Parameters:
 *   INT32: The id of the alarm event.
 *   
 * Return:
 *
 *   OBJECT_PATH: Name of the object type.
 *
 *   UINT32: Count of arguments for the object.
 *
 *   STRING: Parameter 1 name.
 *
 *   TYPE: Parameter 1 value.
 *
 *   STRING: Parameter 2 name.
 *
 *   TYPE: Parameter 2 value.
 *   ...
 *   
 **/
#define ALARM_EVENT_GET "get_event"

/**
 * ALARMD_SNOOZE_SET:
 *
 * Parameters:
 *   UINT32: The amount of minutes the default snooze should be.
 *
 * Return:
 *
 *   BOOLEAN: Status of the request.
 **/
#define ALARMD_SNOOZE_SET "set_snooze"

/**
 * ALARMD_SNOOZE_GET:
 *
 * Return:
 *   UINT32: The amount of minutes the default snooze is.
 **/
#define ALARMD_SNOOZE_GET "get_snooze"

#endif

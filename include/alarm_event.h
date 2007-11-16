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

#ifndef _ALARM_EVENT_H_
#define _ALARM_EVENT_H_

#include <stdint.h>
#include <time.h>

/**
 * SECTION:alarm_event
 * @short_description: The C client API
 * 
 * Includes functions to communicate with the alarm daemon from another
 * program.
 **/

/**
 * cookie_t:
 *
 * Unique identifier type for the alarm events.
 **/
typedef long cookie_t;

/**
 * alarmeventflags:
 * @ALARM_EVENT_NO_DIALOG: The alarm event should not show a dialog.
 * @ALARM_EVENT_NO_SNOOZE: The alarm dialog shoud not have snooze enabled.
 * @ALARM_EVENT_SYSTEM: The dbus call should be done on system bus.
 * @ALARM_EVENT_BOOT: The event should boot up the system, if powered off.
 * @ALARM_EVENT_ACTDEAD: The device should only be powered to acting dead mode.
 * @ALARM_EVENT_SHOW_ICON: The event should show a icon in the status bar.
 * @ALARM_EVENT_RUN_DELAYED: The event should be launched on next startup,
 * if missed or immediately if jumped over.
 * @ALARM_EVENT_CONNECTED: The event should be launched only when connected to
 * the internet.
 * @ALARM_EVENT_ACTIVATION: The dbus action should use DBus activation.
 * @ALARM_EVENT_POSTPONE_DELAYED: The event should be postponed to next day
 * if miesed or jumped over.
 * @ALARM_EVENT_BACK_RESCHEDULE: The event should be moved backwards, if the
 * time is moved backwards. Applies only to recurring events.
 *
 * Describes alarm event. These should be bitwise orred to the flags field in
 * #alarm_event_t.
 **/
typedef enum {
	ALARM_EVENT_NO_DIALOG = 1 << 0,	/* Do not show the alarm dialog */
	ALARM_EVENT_NO_SNOOZE = 1 << 1,	/* Disable the snooze button */
	ALARM_EVENT_SYSTEM = 1 << 2,	/* Use the DBus system bus */
	ALARM_EVENT_BOOT = 1 << 3,	/* Boot up the system */
	ALARM_EVENT_ACTDEAD = 1 << 4,	/* Boot into alarm mode */
	ALARM_EVENT_SHOW_ICON = 1 << 5, /* Show alarm icon on statusbar */
	ALARM_EVENT_RUN_DELAYED = 1 << 6, /* Should the alarm be run on
					     startup if missed. */
	ALARM_EVENT_CONNECTED = 1 << 7, /* Run only when connected. */
	ALARM_EVENT_ACTIVATION = 1 << 8, /* Should DBus call use activation. */
	ALARM_EVENT_POSTPONE_DELAYED = 1 << 9, /* Should the alarm be postponed
						  if missed. */
	ALARM_EVENT_BACK_RESCHEDULE = 1 << 10, /* Should the alarm be moved
						  backwards, if time is changed
						  backwards. */
} alarmeventflags;

/**
 * alarm_error_t:
 * @ALARMD_SUCCESS: No error occurred during the operation.
 * @ALARMD_ERROR_DBUS: An error with DBus occurred, probably couldn't get a
 * DBus connection.
 * @ALARMD_ERROR_CONNECTION: An error occurred while trying to connect to
 * alarmd.
 * @ALARMD_ERROR_INTERNAL: An libalarm internal error occurred, possibly a
 * version mismatch.
 * @ALARMD_ERROR_MEMORY: Memory exhausted while running operation.
 * @ALARMD_ERROR_ARGUMENT: An argument given by caller was invalid.
 * @ALARMD_ERROR_NOT_RUNNING: Alarmd was not running.
 *
 * Error codes for an alarmd operation.
 **/
typedef enum {
	ALARMD_SUCCESS,
	ALARMD_ERROR_DBUS,
	ALARMD_ERROR_CONNECTION,
	ALARMD_ERROR_INTERNAL,
	ALARMD_ERROR_MEMORY,
	ALARMD_ERROR_ARGUMENT,
	ALARMD_ERROR_NOT_RUNNING
} alarm_error_t;
	
/**
 * alarm_event_t:
 * @alarm_time: Time of alarm.
 * @recurrence: Number of minutes between each recurrence.
 * @recurrence_count: Number of recurrences, -1 for infinite.
 * @snooze: Number of minutes an event is postponed on snooze.
 * @title: The title of the alarm dialog.
 * @message: The message in the alarm dialog.
 * @sound: The sound played while showin the alarm dialog.
 * @icon: The icon shown in the alarm dialog.
 * @dbus_interface: The interface for the dbus call.
 * @dbus_service: The service for the dbus call.
 * @dbus_path: The path for the dbus call.
 * @dbus_name: The name of the dbus call.
 * @exec_name: The command to be run.
 * @flags: Bitfield describing the event, see #alarmeventflags.
 * @snoozed: Amount of minutes the event has been snoozed.
 *
 * Describes an alarm event.
 **/
typedef struct {
	time_t alarm_time;		/* Time of alarm; UTC */
	uint32_t recurrence;		/* Number of minutes between
					 * each recurrence;
					 * 0 for one-shot alarms
					 */
	int32_t recurrence_count;	/* Number of recurrences, use -1 for
       					   infinite. */
	uint32_t snooze;		/* Number of minutes an alarm is
					 * potstponed on snooze. 0 for
					 * default */
	char *title;			/* Title of the alarm */
	char *message;			/* Alarm message to display */
	char *sound;			/* Alarm sound to play */
	char *icon;			/* Alarm icon to use */
	char *dbus_interface;		/* DBus callback: interface */
	char *dbus_service;		/* DBus callback: service
					 * set to NULL to send a signal
					 */
	char *dbus_path;		/* DBus callback: path */
	char *dbus_name;		/* DBus callback: method_call/signal */
	char *exec_name;		/* File callback: file to execute */
	int32_t flags;			/* Event specific behaviour */

	uint32_t snoozed;		/* How much the event has been
					   snoozed. */
} alarm_event_t;

/**
 * alarm_event_add:
 * @event: #alarm_event_t struct describing the event to be added.
 *
 * Adds an event to the alarm queue.
 * If event->exec_name and event->dbus_path are NULL, the alarm will be only
 * show a dialog (unless #ALARM_EVENT_NO_DIALOG is set in flags, in which case,
 * the event does nothing). If only event->dbus_path is NULL, the alarm will
 * run a program at given time (and show a dialog unless otherwise
 * instructed). If recurrence > 0, the alarm will repeat recurrence_count
 * times. If snooze_time is zero, default of 10 minutes is used.
 * Returns: Unique identifier for the alarm, or 0 on failure.
 **/
cookie_t alarm_event_add(alarm_event_t *event);

/**
 * alarm_event_add_with_dbus_params:
 * @event: #alarm_event_t struct describing the event to be added.
 * @first_arg_type: Type of first argument to the DBus actionn.
 * @...: Type-value pairs of the arguments to the dbus call; end with
 * DBUS_TYPE_INVALID.
 *
 * Just like #alarm_event_add, but with arguments for the DBus call.
 * The DBus arguments should be passed as pointers, like for
 * dbus_message_append_args. List should be terminated with
 * DBUS_TYPE_INVALID. Note, you need dbus/dbus-protocol.h for the type
 * macros.
 * Returns: Unique identifier for the alarm, or 0 on failure.
 **/
cookie_t alarm_event_add_with_dbus_params(alarm_event_t *event, 
		int first_arg_type, ...);

/**
 * alarm_event_del:
 * @event_cookie: Unique identifier of the alarm to be removed.
 *
 * Deletes alarm from the alarm queue.
 * The alarm with the event_cookie identifier will be removed from the
 * alarm queue, if it exists. For more details on errors, use
 * alarmd_get_eroror.
 * Returns: 1 If alarm was on the queue, 0 if not and -1 on errors.
 **/
int alarm_event_del(cookie_t event_cookie);

/**
 * alarm_event_query:
 * @first: Alarms happening after this time will be returned.
 * @last: Alarms: happening before this time will be returned.
 * @flag_mask: A mask describing which flags you're interested in.
 * 		Pass 0 to get all events.
 * @flags: Values for the flags you're querying.
 *
 * Queries alarms in given time span.
 * Finds every alarm whose _next_ occurence time is between first and last.
 * Returns: Newly allocated, zero-terminated list of alarm id's, should be
 *	   free()'d.
 **/
cookie_t *alarm_event_query(const time_t first, const time_t last,
		int32_t flag_mask, int32_t flags);

/**
 * alarm_event_get:
 * @event_cookie: Unique identifier of the alarm.
 *
 * Fetches alarm defails.
 * Finds an alarm with given identifier and returns alarm_event struct
 * describing it.
 * Returns: Newly allocated alarm_event struct, should be free'd with
 *	   alarm_event_free().
 **/
alarm_event_t *alarm_event_get(cookie_t event_cookie);

/**
 * alarm_event_free:
 * @event: The alarm_event struct to be free'd.
 *
 * Frees given alarm_event struct.
 * Will free all memory associated with the alarm_event struct.
 **/
void alarm_event_free(alarm_event_t *event);

/**
 * alarm_escape_string:
 * @string: The string that should be escaped.
 *
 * Escapes a string to be used as alarm dialog message or title. All { and }
 * characters will be escaped with backslash and all backslashes will be
 * duplicated.
 * Returns: Newly allocated string, should be freed with free().
 **/
char *alarm_escape_string(const char *string);

/**
 * alarm_unescape_string:
 * @string: The string that should be unescaped.
 *
 * Unescapes a string escaped with alarm_escape_string.
 * Returns: Newly allocated string, should be freed with free().
 **/
char *alarm_unescape_string(const char *string);

/**
 * alarm_unescape_string_noalloc:
 * @string: The string that should be unescaped.
 *
 * Unescapes a string escaped with alarm_escape_string. Note, @string is
 * modified.
 * Returns: @string.
 **/
char *alarm_unescape_string_noalloc(char *string);

/**
 * alarmd_get_error:
 * 
 * Gets the error code for previous action.
 * Returns: The error code.
 **/
alarm_error_t alarmd_get_error(void);

/**
 * alarmd_set_default_snooze:
 * @snooze: The preferred snooze time in minutes.
 *
 * Sets the amount events will be snoozed by default.
 * Returns: 1 on success, 0 on failure.
 **/
int alarmd_set_default_snooze(unsigned int snooze);

/**
 * alarmd_get_default_snooze:
 * 
 * Gets the amount events will be snoozed by default.
 * Returns: The preferred snooze time in minutes.
 **/
unsigned int alarmd_get_default_snooze(void);

#endif /* _ALARM_EVENT_H_ */

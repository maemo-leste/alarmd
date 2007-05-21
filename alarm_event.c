/**
 * This file is part of alarmd
 *
 * Contact Person: David Weinehall <david.weinehall@nokia.com>
 *
 * Copyright (C) 2006-2007 Nokia Corporation
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

#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>
#include <time.h>
#include "include/alarm_dbus.h"
#include "include/alarm_event.h"

#define _strdup(x) ((x) != NULL ? strdup(x) : NULL)
#define NULL0(x) ((x) != NULL ? 1 : 0)
#define ZERO0(x) ((x) != 0 ? 1 : 0)
#define APPEND_ARG(msg, value, type, code) \
while ((int)(value) != 0) { \
	if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, \
				&property[(code)], \
				(type), &(value), DBUS_TYPE_INVALID)) { \
		error_code = ALARMD_ERROR_MEMORY; \
		return 0; \
	} \
		break; \
}
	
static int _get_id(const char *name);

enum Properties {
	TIME,
	TITLE,
	MESSAGE,
	SOUND,
	ICON,
	INTERFACE,
	SERVICE,
	PATH,
	NAME,
	FLAGS,
	ACTION,
	RECURRENCE,
	RECURRENCE_COUNT,
	SNOOZE_INT,
	SNOOZE,
	COUNT
};

static const char * const property[COUNT] = {
	"time",
	"title",
	"message",
	"sound",
	"icon",
	"interface",
	"service",
	"path",
	"name",
	"flags",
	"action",
	"recurr_interval",
	"recurr_count",
	"snooze_interval",
	"snooze",
};

static alarm_error_t error_code;

static size_t strstrcount(const char *haystack, const char *needle)
{
	size_t retval = 0;
	size_t needlelen = strlen(needle);

	if (!haystack || !needle) {
		return 0;
	}

	for (haystack = strstr(haystack, needle);
		       	haystack;
		       	haystack = strstr(haystack + needlelen, needle)) {
		retval++;
	}

	return retval;
}

static size_t strchrcount(const char *haystack, const char needle)
{
	size_t retval = 0;

	if (!haystack) {
		return retval;
	}

	for (; *haystack; haystack++) {
		if (*haystack == needle) {
			retval++;
		}
	}

	return retval;
}

static size_t strspncount(const char *haystack, const char *needles)
{
	size_t count = 0;

	for (haystack = strpbrk(haystack, needles); haystack; haystack = strpbrk(haystack + 1, needles)) {
		count++;
	}

	return count;
}

static DBusMessage *_alarm_event_dbus_call(const char *method, int first_arg_type, ...)
{
  DBusMessage *msg = NULL;
  DBusMessage *reply = NULL;
  DBusConnection *conn = NULL;
  DBusError error;
  va_list arg_list;

  if (!method) {
    error_code = ALARMD_ERROR_INTERNAL;
    return NULL;
  }

  conn = dbus_bus_get_private(DBUS_BUS_SESSION, NULL);

  if (!conn) {
    error_code = ALARMD_ERROR_DBUS;
    return NULL;
  }

  va_start(arg_list, first_arg_type);

  /* Create new dbus method call to multimedia. */
  msg = dbus_message_new_method_call(ALARMD_SERVICE,
                                     ALARMD_PATH,
                                     ALARMD_INTERFACE,
                                     method);

  if (!msg) {
	  dbus_connection_close(conn);
	  dbus_connection_unref(conn);
	  error_code = ALARMD_ERROR_MEMORY;
	  return NULL;
  }

  /* Append given arguments. */
  dbus_message_append_args_valist(msg, first_arg_type, arg_list);
  /* Put the message to dbus queue. */
  dbus_error_init(&error);
  reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
  if (dbus_error_is_set(&error)) {
	  if (dbus_error_has_name(&error, DBUS_ERROR_NO_MEMORY)) {
		  error_code = ALARMD_ERROR_MEMORY;
	  } else if (dbus_error_has_name(&error, DBUS_ERROR_SERVICE_UNKNOWN) ||
			  dbus_error_has_name(&error, DBUS_ERROR_NAME_HAS_NO_OWNER)) {
		  error_code = ALARMD_ERROR_NOT_RUNNING;
	  } else if (dbus_error_has_name(&error, DBUS_ERROR_NO_REPLY)) {
		  error_code = ALARMD_ERROR_CONNECTION;
	  } else {
		  error_code = ALARMD_ERROR_DBUS;
	  }
	  dbus_error_free(&error);
  }
  /* Close the connection. */
  dbus_connection_close(conn);
  dbus_connection_unref(conn);
  /* Unref (free) the message. */
  dbus_message_unref(msg);

  va_end(arg_list);

  return reply;
}

cookie_t alarm_event_add(alarm_event_t *event)
{
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessage *reply;
	DBusError error;
	dbus_int32_t cookie;
	dbus_uint32_t event_arg_count = 0;
	dbus_uint32_t action_arg_count = 0;
	dbus_int64_t time64 = (dbus_int64_t)event->alarm_time;
	const char *event_name = NULL;
	const char *action_name = NULL;

	error_code = ALARMD_SUCCESS;
	
	if (!event) {
		error_code = ALARMD_ERROR_ARGUMENT;
		return 0;
	}

	event_arg_count = ZERO0(event->recurrence) +
	       	ZERO0(event->recurrence_count) +
		ZERO0(event->snooze) + 2;
	action_arg_count = NULL0(event->title) +
		NULL0(event->message) +
		NULL0(event->sound) +
		NULL0(event->icon) +
		NULL0(event->dbus_interface) +
		NULL0(event->dbus_service) +
		NULL0(event->dbus_path) +
		NULL0(event->dbus_name) +
		NULL0(event->exec_name) + 1;

	if (event->dbus_path == NULL && event->exec_name == NULL) {
		action_name = "/AlarmdActionDialog";
	} else if (event->dbus_path == NULL) {
		action_name = "/AlarmdActionExec";
	} else {
		action_name = "/AlarmdActionDbus";
	}

	if (event->recurrence != 0) {
		event_name = "/AlarmdEventRecurring";
	} else {
		event_name = "/AlarmdEvent";
	}

	/* Create new dbus method call to multimedia. */
	msg = dbus_message_new_method_call(ALARMD_SERVICE,
			ALARMD_PATH,
			ALARMD_INTERFACE,
			ALARM_EVENT_ADD);

	if (msg == NULL) {
		error_code = ALARMD_ERROR_MEMORY;
		return 0;
	}

	if (!dbus_message_append_args(msg, DBUS_TYPE_OBJECT_PATH, &event_name,
			DBUS_TYPE_UINT32, &event_arg_count,
			DBUS_TYPE_STRING, &property[TIME],
			DBUS_TYPE_INT64, &time64,
			DBUS_TYPE_STRING, &property[ACTION],
			DBUS_TYPE_OBJECT_PATH, &action_name,
			DBUS_TYPE_UINT32, &action_arg_count,
			DBUS_TYPE_STRING, &property[FLAGS],
			DBUS_TYPE_INT32, &event->flags,
		      	DBUS_TYPE_INVALID)) {
		error_code = ALARMD_ERROR_MEMORY;
		dbus_message_unref(msg);
		return 0;
	}

	/* Action arguments. */
	APPEND_ARG(msg, event->title, DBUS_TYPE_STRING, TITLE);
	APPEND_ARG(msg, event->message, DBUS_TYPE_STRING, MESSAGE);
	APPEND_ARG(msg, event->sound, DBUS_TYPE_STRING, SOUND);
	APPEND_ARG(msg, event->icon, DBUS_TYPE_STRING, ICON);
	APPEND_ARG(msg, event->dbus_interface, DBUS_TYPE_STRING, INTERFACE);
	APPEND_ARG(msg, event->dbus_service, DBUS_TYPE_STRING, SERVICE);
	APPEND_ARG(msg, event->dbus_path, DBUS_TYPE_STRING, PATH);
	APPEND_ARG(msg, event->dbus_name, DBUS_TYPE_STRING, NAME);
	APPEND_ARG(msg, event->exec_name, DBUS_TYPE_STRING, PATH);

	/* Event arguments. */
	APPEND_ARG(msg, event->recurrence, DBUS_TYPE_UINT32, RECURRENCE);
	APPEND_ARG(msg, event->recurrence_count, DBUS_TYPE_INT32, RECURRENCE_COUNT);
	APPEND_ARG(msg, event->snooze, DBUS_TYPE_UINT32, SNOOZE_INT);

	conn = dbus_bus_get_private(DBUS_BUS_SESSION, NULL);

	if (conn == NULL) {
		dbus_message_unref(msg);
		error_code = ALARMD_ERROR_DBUS;
		return 0;
	}

	reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, NULL);
	dbus_connection_close(conn);
	dbus_connection_unref(conn);
	dbus_message_unref(msg);

	if (reply == NULL) {
		error_code = ALARMD_ERROR_CONNECTION;
		return 0;
	}

	dbus_error_init(&error);
	dbus_message_get_args(reply, &error,
			DBUS_TYPE_INT32, &cookie,
			DBUS_TYPE_INVALID);
	
	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
		dbus_message_unref(reply);
		error_code = ALARMD_ERROR_INTERNAL;
		return 0;
	}

	dbus_message_unref(reply);

	return cookie;
}

int alarm_event_del(cookie_t event_cookie) 
{
	DBusMessage *reply;
	DBusError error;
	dbus_bool_t success = 0;

	error_code = ALARMD_SUCCESS;

	if (!event_cookie) {
		error_code = ALARMD_ERROR_ARGUMENT;
		return -1;
	}

	reply = _alarm_event_dbus_call(ALARM_EVENT_DEL,
			DBUS_TYPE_INT32, &event_cookie,
			DBUS_TYPE_INVALID);

	if (reply == NULL) {
		return -1;
	}

	dbus_error_init(&error);
	dbus_message_get_args(reply, &error,
			DBUS_TYPE_BOOLEAN, &success,
			DBUS_TYPE_INVALID);
	
	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
		dbus_message_unref(reply);
		error_code = ALARMD_ERROR_INTERNAL;
		return 0;
	}

	dbus_message_unref(reply);

	return success;
}

cookie_t *alarm_event_query(const time_t first, const time_t last,
		int32_t flag_mask, int32_t flags)
{
	DBusMessage *reply;
	DBusMessageIter iter;
	cookie_t *retval;

	dbus_int64_t first_64 = first;
	dbus_int64_t last_64 = last;

	error_code = ALARMD_SUCCESS;

	reply = _alarm_event_dbus_call(ALARM_EVENT_QUERY,
			DBUS_TYPE_UINT64, &first_64,
			DBUS_TYPE_UINT64, &last_64,
			DBUS_TYPE_INT32, &flag_mask,
			DBUS_TYPE_INT32, &flags,
			DBUS_TYPE_INVALID);

	if (reply == NULL) {
		return NULL;
	} else if (!dbus_message_iter_init(reply, &iter)) {
		dbus_message_unref(reply);
		error_code = ALARMD_ERROR_INTERNAL;
		return NULL;
	}

	if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
		const dbus_int32_t *array;
		int count, i;
		DBusMessageIter sub;
		dbus_message_iter_recurse(&iter, &sub);
		dbus_message_iter_get_fixed_array(&sub, &array, &count);
		retval = (cookie_t *)calloc(count + 1, sizeof(cookie_t));

		for (i = 0; i < count; i++) {
			retval[i] = (cookie_t)array[i];
		}
	} else {
		retval = (cookie_t *)calloc(1, sizeof(cookie_t));
	}

	dbus_message_unref(reply);

	return retval;
}

alarm_event_t *alarm_event_get(cookie_t event_cookie)
{
	DBusMessage *reply;
	DBusMessageIter iter;
	alarm_event_t *retval = NULL;

	const char *name;
	const char *value_string;
	dbus_uint64_t value_u64;
	dbus_uint32_t value_u32;
	dbus_uint32_t arg_count;
	unsigned int i;

	unsigned int is_exec = 0;

	error_code = ALARMD_SUCCESS;

	reply = _alarm_event_dbus_call(ALARM_EVENT_GET,
			DBUS_TYPE_INT32, &event_cookie,
			DBUS_TYPE_INVALID);

	if (reply == NULL) {
		return NULL;
	}

	if (!dbus_message_iter_init(reply, &iter)) {
		dbus_message_unref(reply);
		error_code = ALARMD_ERROR_INTERNAL;
		return NULL;
	}

	if (!dbus_message_iter_next(&iter)) {
		dbus_message_unref(reply);
		error_code = ALARMD_ERROR_INTERNAL;
		return NULL;
	}
	dbus_message_iter_get_basic(&iter, &arg_count);
	dbus_message_iter_next(&iter);

	retval = (alarm_event_t *)calloc(1, sizeof(alarm_event_t));

	for (i = 0; i < arg_count; i++) {
		dbus_message_iter_get_basic(&iter, &name);
		dbus_message_iter_next(&iter);
#define dbus_message_iter_get_string(iter, var) \
		dbus_message_iter_get_basic(iter, &value_string); \
			var = _strdup(value_string); \
			value_string = NULL;
		switch (_get_id(name)) {
			case TITLE:
				dbus_message_iter_get_string(&iter, retval->title);
				break;
			case MESSAGE:
				dbus_message_iter_get_string(&iter, retval->message);
				break;
			case SOUND:
				dbus_message_iter_get_string(&iter, retval->sound);
				break;
			case ICON:
				dbus_message_iter_get_string(&iter, retval->icon);
				break;
			case INTERFACE:
				dbus_message_iter_get_string(&iter, retval->dbus_interface);
				break;
			case PATH:
				if (is_exec) {
					dbus_message_iter_get_string(&iter, retval->exec_name);
				} else {
					dbus_message_iter_get_string(&iter, retval->dbus_path);
				}
				break;
			case NAME:
				dbus_message_iter_get_string(&iter, retval->dbus_name);
				break;
			case FLAGS:
				dbus_message_iter_get_basic(&iter, &retval->flags);
				break;
			case RECURRENCE:
				dbus_message_iter_get_basic(&iter, &retval->recurrence);
				break;
			case RECURRENCE_COUNT:
				dbus_message_iter_get_basic(&iter, &retval->recurrence_count);
				break;
			case SNOOZE_INT:
				dbus_message_iter_get_basic(&iter, &retval->snooze);
				break;
			case SNOOZE:
				dbus_message_iter_get_basic(&iter, &retval->snoozed);
				break;
			case TIME:
				dbus_message_iter_get_basic(&iter, &value_u64);
				retval->alarm_time = (time_t)value_u64;
				break;
			case ACTION:
				dbus_message_iter_get_basic(&iter, &value_string);
				if (strcmp(value_string, "/AlarmdActionExec") == 0) {
					is_exec = 1;
				}
				dbus_message_iter_next(&iter);
				dbus_message_iter_get_basic(&iter, &value_u32);
				arg_count += value_u32;
				break;
			default:
				break;
		}
		if (!dbus_message_iter_next(&iter)) {
			break;
		}
	}

	dbus_message_unref(reply);

	return retval;
}

void alarm_event_free(alarm_event_t *event)
{
	if (event == NULL) {
		return;
	}

	if (event->title) {
		free(event->title);
	}
	if (event->message) {
		free(event->message);
	}
	if (event->sound) {
		free(event->sound);
	}
	if (event->icon) {
		free(event->icon);
	}
	if (event->dbus_interface) {
		free(event->dbus_interface);
	}
	if (event->dbus_service) {
		free(event->dbus_service);
	}
	if (event->dbus_path) {
		free(event->dbus_path);
	}
	if (event->dbus_name) {
		free(event->dbus_name);
	}
	if (event->exec_name) {
		free(event->exec_name);
	}

	free(event);
}

static int _get_id(const char *name) {
	unsigned int i = 0;

	if (name == NULL) {
		return COUNT;
	}

	for (i = 0; i < COUNT; i++) {
		if (strcmp(name, property[i]) == 0) {
			break;
		}
	}

	return i;
}

char *alarm_escape_string(const char *string)
{
	size_t new_len;
	char *retval;
	char *iter;

	if (!string) {
		return NULL;
	}

	new_len = strlen(string) + strspncount(string, "\\{}");
	retval = malloc(new_len + 1);
	iter = retval;

	for (; *string; string++, iter++) {
		switch (*string) {
		case '\\':
		case '{':
		case '}':
			*iter = '\\';
			iter++;
		default:
			*iter = *string;
		}
	}
	*iter = '\0';

	return retval;
}

static void _alarm_do_unescape(const char *read, char *write)
{
	while (*read) {
		if (*read == '\\' && read[1]) {
			read++;
		}

		*write = *read;
		write++;
		read++;
	}
	*write = '\0';
}

char *alarm_unescape_string_noalloc(char *string)
{
	if (!string) {
		return string;
	}

	_alarm_do_unescape(string, string);

	return string;
}

char *alarm_unescape_string(const char *string)
{
	size_t len;
	char *retval;

	if (!string) {
		return NULL;
	}

	len = strlen(string)
	       	- strchrcount(string, '\\')
	       	+ strstrcount(string, "\\\\");
	retval = malloc(len + 1);

	_alarm_do_unescape(string, retval);

	return retval;
}

alarm_error_t alarmd_get_error(void)
{
	return error_code;
}

int alarmd_set_default_snooze(unsigned int snooze)
{
	DBusMessage *reply;
	dbus_bool_t success;
	DBusError error;

	error_code = ALARMD_SUCCESS;

	if (snooze == 0) {
		error_code = ALARMD_ERROR_ARGUMENT;
		return 0;
	}

	reply = _alarm_event_dbus_call(ALARMD_SNOOZE_SET, DBUS_TYPE_UINT32,
		       	&snooze, DBUS_TYPE_INVALID);

	if (reply == NULL) {
		return 0;
	}

	dbus_error_init(&error);
	dbus_message_get_args(reply, &error,
			DBUS_TYPE_BOOLEAN, &success,
			DBUS_TYPE_INVALID);

	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
		dbus_message_unref(reply);
		error_code = ALARMD_ERROR_INTERNAL;
		return 0;
	}
	dbus_message_unref(reply);

	if (!success) {
		error_code = ALARMD_ERROR_INTERNAL;
	}
	
	return success;
}

unsigned int alarmd_get_default_snooze(void)
{
	DBusMessage *reply;
	dbus_uint32_t snooze = 0;
	DBusError error;

	error_code = ALARMD_SUCCESS;

	reply = _alarm_event_dbus_call(ALARMD_SNOOZE_GET, DBUS_TYPE_INVALID);

	if (reply == NULL) {
		return 0;
	}

	dbus_error_init(&error);
	dbus_message_get_args(reply, &error,
			DBUS_TYPE_UINT32, &snooze,
			DBUS_TYPE_INVALID);

	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
		dbus_message_unref(reply);
		error_code = ALARMD_ERROR_INTERNAL;
		return 0;
	}
	dbus_message_unref(reply);
	
	if (snooze == 0) {
		error_code = ALARMD_ERROR_INTERNAL;
	}
	
	return snooze;
}

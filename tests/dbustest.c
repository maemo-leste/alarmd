#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>
#include <stdio.h>
#include "alarm_event.h"

static DBusHandlerResult my_filter(DBusConnection *conn,
		DBusMessage *msg,
		void *user_data)
{
	char *arg = NULL;

	(void)conn;

	if (!dbus_message_is_method_call(msg, "com.nokia.test", "method")) {
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &arg,
			DBUS_TYPE_INVALID);

	if (arg) {
		printf("Got argument '%s', expected 'magic'\n", arg);
	} else {
		printf("No argument!\n");
	}

	g_main_loop_quit(user_data);

	return DBUS_HANDLER_RESULT_HANDLED;
}

int main()
{
	DBusConnection *conn;
	GMainLoop *loop;
	cookie_t coo;
	const char *arg = "magic";
	alarm_event_t event = {
		.alarm_time = 0,
		.recurrence = 0,
		.recurrence_count = 0,
		.snooze = 5,
		.message = NULL,
		.sound = NULL,
		.icon = NULL,
		.dbus_interface = (char *)"com.nokia.test",
		.dbus_service = (char *)"com.nokia.test",
		.dbus_path = (char *)"/com/nokia/test",
		.dbus_name = (char *)"method",
		.exec_name = NULL,
		.flags = ALARM_EVENT_NO_DIALOG | ALARM_EVENT_ACTDEAD
	};
	event.alarm_time = time(NULL) + 10;

	loop = g_main_loop_new(NULL, FALSE);
	conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	dbus_connection_setup_with_g_main(conn, NULL);
	
	dbus_bus_request_name(conn,
			"com.nokia.test",
			0,
			NULL);
	dbus_bus_add_match(conn,
			"type=method_call,interface=com.nokia.test",
			NULL);
	dbus_connection_add_filter(conn,
			my_filter,
			g_main_loop_ref(loop),
			(DBusFreeFunction)g_main_loop_unref);
	
	coo = alarm_event_add_with_dbus_params(&event,
			DBUS_TYPE_STRING, &arg,
			DBUS_TYPE_INVALID);

	g_timeout_add(15000, (GSourceFunc)g_main_loop_quit, loop);

	g_main_loop_run(loop);

	dbus_connection_remove_filter(conn,
			my_filter,
			loop);

	g_main_loop_unref(loop);

	return 0;
}

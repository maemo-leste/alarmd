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

/* ========================================================================= *
 * tester for asynchronous alarmd method calls
 *
 * 18-Jun-2009 Simo Piiroinen
 * ========================================================================= */

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>

#include <dbus/dbus-glib-lowlevel.h>

#include <alarmd/libalarm-async.h>
#include <alarmd/alarm_dbus.h>

// logging is not exposed in libalarm.so, apart from that
// this code should compile also outside alarmd source tree
#if 0
# include "../src/libalarm-async.h"
# include "../src/alarm_dbus.h"
# include "../src/logging.h"
#else
# include <alarmd/libalarm-async.h>
# include <alarmd/alarm_dbus.h>

# define log_open(ident,driver,daemon) do{}while(0)
# define log_set_level(level)          do{}while(0)

# define log_error(FMT, ARG...)   printf("error: "  FMT, ## ARG)
# define log_info(FMT, ARG...)    printf("info: "  FMT, ## ARG)
# define log_debug(FMT, ARG...)   printf("debug: "  FMT, ## ARG)

# define log_error_F(FMT, ARG...) printf("error: %s: "  FMT, __FUNCTION__, ## ARG)
# define log_debug_F(FMT, ARG...) printf("debug: %s: "  FMT, __FUNCTION__, ## ARG)
#endif

#define MATCH_ALARMD_OWNERCHANGED\
  "type='signal'"\
  ",sender='"DBUS_SERVICE_DBUS"'"\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='NameOwnerChanged'"\
  ",arg0='"ALARMD_SERVICE"'"

#define MATCH_ALARMD_STATUS_IND\
  "type='signal'"\
  ",sender='"ALARMD_SERVICE"'"\
  ",interface='"ALARMD_INTERFACE"'"\
  ",path='"ALARMD_PATH"'"\
  ",member='"ALARMD_QUEUE_STATUS_IND"'"

#define APPID "alarmd.asynctest"

static DBusConnection *asynctest_session_bus  = 0;
static DBusConnection *asynctest_system_bus   = 0;
static GMainLoop      *asynctest_mainloop_hnd = 0;

/* ------------------------------------------------------------------------- *
 * asynctest_mainloop_stop
 * ------------------------------------------------------------------------- */

static
void
asynctest_mainloop_stop(void)
{
  if( asynctest_mainloop_hnd == 0 )
  {
    exit(EXIT_FAILURE);
  }
  g_main_loop_quit(asynctest_mainloop_hnd);
}

/* ------------------------------------------------------------------------- *
 * asynctest_sighnd_handler  --  handle trapped signals
 * ------------------------------------------------------------------------- */

static
void
asynctest_sighnd_handler(int sig)
{
  dprintf(STDERR_FILENO, "Got signal [%d] %s\n", sig, strsignal(sig));
  exit(EXIT_FAILURE);
}

/* ------------------------------------------------------------------------- *
 * asynctest_sighnd_init  --  setup signal trapping
 * ------------------------------------------------------------------------- */

static
void
asynctest_sighnd_init(void)
{
  static const int sig[] = { SIGHUP, SIGINT, SIGQUIT, SIGTERM, -1 };

  for( int i = 0; sig[i] != -1; ++i )
  {
    signal(sig[i], asynctest_sighnd_handler);
  }
}

/* ------------------------------------------------------------------------- *
 * asynctest_server_filter  -- handle requests coming via dbus
 * ------------------------------------------------------------------------- */

static
DBusHandlerResult
asynctest_server_filter(DBusConnection *conn,
                       DBusMessage *msg,
                       void *user_data)
{
  DBusHandlerResult   result    = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  const char         *interface = dbus_message_get_interface(msg);
  const char         *member    = dbus_message_get_member(msg);
  const char         *object    = dbus_message_get_path(msg);
  int                 type      = dbus_message_get_type(msg);
  DBusMessage        *rsp       = 0;

  if( !interface || !member || !object )
  {
    goto cleanup;
  }

  if( type == DBUS_MESSAGE_TYPE_SIGNAL &&
      !strcmp(interface, DBUS_INTERFACE_DBUS) &&
      !strcmp(object,DBUS_PATH_DBUS) &&
      !strcmp(member, "NameOwnerChanged") )
  {
    DBusError   err  = DBUS_ERROR_INIT;
    const char *prev = 0;
    const char *curr = 0;
    const char *serv = 0;

    dbus_message_get_args(msg, &err,
                          DBUS_TYPE_STRING, &serv,
                          DBUS_TYPE_STRING, &prev,
                          DBUS_TYPE_STRING, &curr,
                          DBUS_TYPE_INVALID);

    if( serv != 0 && !strcmp(serv, ALARMD_SERVICE) )
    {
      if( !curr || !*curr )
      {
        log_debug("ALARMD WENT DOWN\n");
      }
      else
      {
        log_debug("ALARMD CAME UP\n");
      }
    }
    dbus_error_free(&err);
  }

  if( type == DBUS_MESSAGE_TYPE_SIGNAL &&
      !strcmp(interface, ALARMD_INTERFACE) &&
      !strcmp(object,ALARMD_PATH) &&
      !strcmp(member, ALARMD_QUEUE_STATUS_IND) )
  {
    DBusError    err = DBUS_ERROR_INIT;
    dbus_int32_t ac = 0;
    dbus_int32_t dt = 0;
    dbus_int32_t ad = 0;
    dbus_int32_t nb = 0;

    dbus_message_get_args(msg, &err,
                          DBUS_TYPE_INT32, &ac,
                          DBUS_TYPE_INT32, &dt,
                          DBUS_TYPE_INT32, &ad,
                          DBUS_TYPE_INT32, &nb,
                          DBUS_TYPE_INVALID);

    log_debug("ALARM QUEUE: active=%d, desktop=%d, actdead=%d, no-boot=%d\n",
              ac,dt,ad,nb);
    dbus_error_free(&err);
  }

  if( rsp != 0 )
  {
    dbus_connection_send(conn, rsp, 0);
  }

  cleanup:

  if( rsp != 0 )
  {
    dbus_message_unref(rsp);
  }

  return result;
}

/* ------------------------------------------------------------------------- *
 * asynctest_server_init
 * ------------------------------------------------------------------------- */

static
int
asynctest_server_init(void)
{
  int         res = -1;
  DBusError   err = DBUS_ERROR_INIT;

  if( (asynctest_system_bus = dbus_bus_get(DBUS_BUS_SYSTEM, &err)) == 0 )
  {
    log_error("%s: %s\n", err.name, err.message);
    goto cleanup;
  }

  if( (asynctest_session_bus = dbus_bus_get(DBUS_BUS_SESSION, &err)) == 0 )
  {
    log_error("%s: %s\n", err.name, err.message);
    goto cleanup;
  }

  if( !dbus_connection_add_filter(asynctest_system_bus,
                                  asynctest_server_filter, 0, 0) )
  {
    goto cleanup;
  }

  if( !dbus_connection_add_filter(asynctest_session_bus,
                                  asynctest_server_filter, 0, 0) )
  {
    goto cleanup;
  }

  dbus_connection_setup_with_g_main(asynctest_system_bus, NULL);
  dbus_connection_set_exit_on_disconnect(asynctest_system_bus, 0);

  dbus_connection_setup_with_g_main(asynctest_session_bus, NULL);
  dbus_connection_set_exit_on_disconnect(asynctest_session_bus, 0);

  dbus_bus_add_match(asynctest_system_bus, MATCH_ALARMD_OWNERCHANGED, &err);
  dbus_bus_add_match(asynctest_system_bus, MATCH_ALARMD_STATUS_IND, &err);

  res = 0;

  cleanup:

  dbus_error_free(&err);
  return res;
}

/* ------------------------------------------------------------------------- *
 * asynctest_server_quit
 * ------------------------------------------------------------------------- */

static
void
asynctest_server_quit(void)
{
  if( asynctest_session_bus != 0 )
  {
    dbus_connection_remove_filter(asynctest_session_bus,
                                  asynctest_server_filter, 0);

    dbus_connection_unref(asynctest_session_bus);
    asynctest_session_bus = 0;
  }

  if( asynctest_system_bus != 0 )
  {
    dbus_connection_remove_filter(asynctest_system_bus,
                                  asynctest_server_filter, 0);

    DBusError err = DBUS_ERROR_INIT;
    dbus_bus_remove_match(asynctest_system_bus, MATCH_ALARMD_OWNERCHANGED, &err);
    dbus_bus_remove_match(asynctest_system_bus, MATCH_ALARMD_STATUS_IND, &err);
    dbus_error_free(&err);

    dbus_connection_unref(asynctest_system_bus);
    asynctest_system_bus = 0;
  }
}

/* ------------------------------------------------------------------------- *
 * asynctest_call
 * ------------------------------------------------------------------------- */

static
int
asynctest_call(DBusConnection *con,
               DBusMessage    *msg,
               void (*cb)(DBusPendingCall *, void *),
               void *user_data,
               void (*user_free)(void *))
{
  int              res = -1;
  DBusPendingCall *pen = 0;

  if( !dbus_connection_send_with_reply(con, msg, &pen, -1) )
  {
    log_error_F("%s: %s\n", "dbus_connection_send_with_reply", "failed");
    goto cleanup;
  }

  if( pen == 0 )
  {
    log_error_F("%s: %s\n", "dbus_connection_send_with_reply",
                "pending == NULL");
    goto cleanup;
  }

  if( !dbus_pending_call_set_notify(pen, cb, user_data, user_free) )
  {
    log_error_F("%s: %s\n", "dbus_pending_call_set_notify", "failed");
    goto cleanup;
  }

  res = 0;

  cleanup:

  if( pen != 0 ) dbus_pending_call_unref(pen);

  return res;
}

/* ------------------------------------------------------------------------- *
 * asynctest_get_snooze
 * ------------------------------------------------------------------------- */

static void
asynctest_get_snooze_cb(DBusPendingCall *pending, void *user_data)
{
  int          res = -1;
  DBusMessage *rsp = dbus_pending_call_steal_reply(pending);
  if( rsp != 0 )
  {
    res = alarmd_get_default_snooze_decode_rsp(rsp);
    dbus_message_unref(rsp);
  }
  log_debug_F("rsp = %d\n", res);
}

static
int
asynctest_get_snooze(DBusConnection *conn)
{
  log_debug_F("req\n");
  int          res = -1;
  DBusMessage *msg = alarmd_get_default_snooze_encode_req();
  if( msg != 0 )
  {
    res = asynctest_call(conn, msg, asynctest_get_snooze_cb, 0, 0);
    dbus_message_unref(msg);
  }
  return res;
}

/* ------------------------------------------------------------------------- *
 * asynctest_set_snooze
 * ------------------------------------------------------------------------- */

static void
asynctest_set_snooze_cb(DBusPendingCall *pending, void *user_data)
{
  int          res = -1;
  DBusMessage *rsp = dbus_pending_call_steal_reply(pending);
  if( rsp != 0 )
  {
    res = alarmd_set_default_snooze_decode_rsp(rsp);
    dbus_message_unref(rsp);
  }
  log_debug_F("rsp = %s\n", (res == -1) ? "NAK" : "ACK");
}

static
int
asynctest_set_snooze(DBusConnection *conn, int snooze)
{
  log_debug_F("req\n");
  int          res = -1;
  DBusMessage *msg = alarmd_set_default_snooze_encode_req(snooze);
  if( msg != 0 )
  {
    res = asynctest_call(conn, msg, asynctest_set_snooze_cb, 0, 0);
    dbus_message_unref(msg);
  }
  return res;
}

/* ------------------------------------------------------------------------- *
 * asynctest_event_get
 * ------------------------------------------------------------------------- */

static void
asynctest_event_get_cb(DBusPendingCall *pending, void *user_data)
{
  alarm_event_t *eve = 0;
  DBusMessage   *rsp = dbus_pending_call_steal_reply(pending);
  if( rsp != 0 )
  {
    eve = alarmd_event_get_decode_rsp(rsp);
    dbus_message_unref(rsp);
  }
  log_debug_F("rsp = %p\n", eve);
  if( eve != 0 )
  {
    log_debug("\tcookie=%d, title=%s, message=%s\n",
              (int)eve->cookie, eve->title, eve->message);
  }
  alarm_event_delete(eve);
}

static
int
asynctest_event_get(DBusConnection *conn, cookie_t cookie)
{
  log_debug_F("req\n");
  int          res = -1;
  DBusMessage *msg = alarmd_event_get_encode_req(cookie);
  if( msg != 0 )
  {
    res = asynctest_call(conn, msg, asynctest_event_get_cb, 0, 0);
    dbus_message_unref(msg);
  }
  return res;
}

/* ------------------------------------------------------------------------- *
 * asynctest_event_del
 * ------------------------------------------------------------------------- */

static void
asynctest_event_del_cb(DBusPendingCall *pending, void *user_data)
{
  int            res = -1;
  DBusMessage   *rsp = dbus_pending_call_steal_reply(pending);
  if( rsp != 0 )
  {
    res = alarmd_event_del_decode_rsp(rsp);
    dbus_message_unref(rsp);
  }
  log_debug_F("rsp = %s\n", (res == -1) ? "NAK" : "ACK");
}

static
int
asynctest_event_del(DBusConnection *conn, cookie_t cookie)
{
  log_debug_F("req\n");
  int          res = -1;
  DBusMessage *msg = alarmd_event_del_encode_req(cookie);
  if( msg != 0 )
  {
    res = asynctest_call(conn, msg, asynctest_event_del_cb, 0, 0);
    dbus_message_unref(msg);
  }
  return res;
}

/* ------------------------------------------------------------------------- *
 * asynctest_event_add
 * ------------------------------------------------------------------------- */

static void
asynctest_event_add_cb(DBusPendingCall *pending, void *user_data)
{
  cookie_t       res = -1;
  DBusMessage   *rsp = dbus_pending_call_steal_reply(pending);
  if( rsp != 0 )
  {
    res = alarmd_event_add_decode_rsp(rsp);
    dbus_message_unref(rsp);
  }
  log_debug_F("rsp = %d\n", (int)res);
}

static
int
asynctest_event_add(DBusConnection *conn,
                    time_t trigger,
                    const char *title,
                    const char *message,
                    const char *button1,
                    const char *button2)
{
  log_debug_F("req\n");
  int             res = -1;
  alarm_action_t *act = 0;
  alarm_event_t  *eve = 0;
  DBusMessage    *msg = 0;

  // event
  eve = alarm_event_create();
  alarm_event_set_alarm_appid(eve, APPID);
  alarm_event_set_title(eve, title ?: "title");
  alarm_event_set_message(eve, message ?: "message");
  eve->flags |= ALARM_EVENT_DISABLE_DELAYED;
  eve->flags |= ALARM_EVENT_ACTDEAD;
  eve->flags |= ALARM_EVENT_SHOW_ICON;
  eve->flags |= ALARM_EVENT_BACK_RESCHEDULE;

  // action
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_DISABLE;
  alarm_action_set_label(act, button1 ?: "Stop");

  // action
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_SNOOZE;
  alarm_action_set_label(act, button2 ?: "Snooze");

  // trigger

  if( trigger < 365 * 24 * 60 * 60 ) trigger += time(0);
  eve->alarm_time = trigger;

  // attributes
  alarm_event_set_attr_string(eve, "textdomain", "osso-clock");

  if( (msg = alarmd_event_add_encode_req(eve)) )
  {
    res = asynctest_call(conn, msg, asynctest_event_add_cb, 0, 0);
    dbus_message_unref(msg);
  }

  alarm_event_delete(eve);

  return res;
}

/* ------------------------------------------------------------------------- *
 * asynctest_event_query
 * ------------------------------------------------------------------------- */

static void
asynctest_event_query_cb(DBusPendingCall *pending, void *user_data)
{
  cookie_t    *res = 0;
  DBusMessage *rsp = dbus_pending_call_steal_reply(pending);
  if( rsp != 0 )
  {
    res = alarmd_event_query_decode_rsp(rsp);
    dbus_message_unref(rsp);
  }
  log_debug_F("rsp = %p\n", res);
  for( int i = 0; res && res[i]; ++i )
  {
    log_debug("\t[%d] %d\n", i, (int)res[i]);
  }
  free(res);
}

static
int
asynctest_event_query(DBusConnection *conn)
{
  log_debug_F("req\n");
  int          res = -1;
  DBusMessage *msg = alarmd_event_query_encode_req(0,0, 0,0, 0);
  if( msg != 0 )
  {
    res = asynctest_call(conn, msg, asynctest_event_query_cb, 0, 0);
    dbus_message_unref(msg);
  }
  return res;
}

/* ------------------------------------------------------------------------- *
 * asynctest_del_all
 * ------------------------------------------------------------------------- */

static void
asynctest_del_all_cb(DBusPendingCall *pending, void *user_data)
{
  cookie_t       *res  = 0;
  DBusConnection *conn = user_data;
  DBusMessage    *rsp  = dbus_pending_call_steal_reply(pending);
  if( rsp != 0 )
  {
    res = alarmd_event_query_decode_rsp(rsp);
    dbus_message_unref(rsp);
  }
  log_debug_F("rsp = %p\n", res);
  for( int i = 0; res && res[i]; ++i )
  {
    asynctest_event_del(conn, res[i]);
  }
  free(res);
}

static
int
asynctest_del_all(DBusConnection *conn)
{
  log_debug_F("req\n");
  int          res = -1;
  DBusMessage *msg = alarmd_event_query_encode_req(0,0, 0,0, APPID);
  if( msg != 0 )
  {
    res = asynctest_call(conn, msg, asynctest_del_all_cb, conn, 0);
    dbus_message_unref(msg);
  }
  return res;
}

/* ------------------------------------------------------------------------- *
 * asynctest_get_all
 * ------------------------------------------------------------------------- */

static void
asynctest_get_all_cb(DBusPendingCall *pending, void *user_data)
{
  cookie_t       *res = 0;
  DBusConnection *conn = user_data;
  DBusMessage    *rsp = dbus_pending_call_steal_reply(pending);
  if( rsp != 0 )
  {
    res = alarmd_event_query_decode_rsp(rsp);
    dbus_message_unref(rsp);
  }
  log_debug_F("rsp = %p\n", res);
  for( int i = 0; res && res[i]; ++i )
  {
    asynctest_event_get(conn, res[i]);
  }
  free(res);
}

static
int
asynctest_get_all(DBusConnection *conn)
{
  log_debug_F("req\n");
  int          res = -1;
  DBusMessage *msg = alarmd_event_query_encode_req(0,0, 0,0, 0);
  if( msg != 0 )
  {
    res = asynctest_call(conn, msg, asynctest_get_all_cb, conn, 0);
    dbus_message_unref(msg);
  }
  return res;
}

/* ------------------------------------------------------------------------- *
 * asynctest_mainloop_run
 * ------------------------------------------------------------------------- */

static
gboolean
asynctest_exit_timer_cb(gpointer data)
{
  log_debug("exit timer expired\n");
  asynctest_mainloop_stop();
  return FALSE;
}
static
void
asynctest_exit_timer(int secs)
{
  log_debug("exit timer set to %d secs\n", secs);
  g_timeout_add_seconds(secs, asynctest_exit_timer_cb, 0);
}

static
int
asynctest_mainloop_run(int argc, char* argv[])
{
  int exit_code = EXIT_FAILURE;

  if( asynctest_server_init() == -1 )
  {
    goto cleanup;
  }

  asynctest_mainloop_hnd = g_main_loop_new(NULL, FALSE);

  asynctest_get_snooze(asynctest_system_bus);
  asynctest_set_snooze(asynctest_system_bus, 123);
  asynctest_get_snooze(asynctest_system_bus);
  asynctest_set_snooze(asynctest_system_bus, 0);
  asynctest_get_snooze(asynctest_system_bus);

  asynctest_event_query(asynctest_system_bus);

  // these will fail
  asynctest_event_get(asynctest_system_bus, 0x7fffffff);
  asynctest_event_del(asynctest_system_bus, 0x7fffffff);

  asynctest_event_add(asynctest_system_bus, 10, 0,"alarm 1",0,0);
  asynctest_event_add(asynctest_system_bus, 20, 0,"alarm 2",0,0);
  asynctest_event_add(asynctest_system_bus, 30, 0,"alarm 3",0,0);

  asynctest_get_all(asynctest_system_bus);
  asynctest_del_all(asynctest_system_bus);

  asynctest_exit_timer(6);

  log_info("ENTER MAINLOOP\n");
  g_main_loop_run(asynctest_mainloop_hnd);
  log_info("LEAVE MAINLOOP\n");

  g_main_loop_unref(asynctest_mainloop_hnd);
  asynctest_mainloop_hnd = 0;

  exit_code = EXIT_SUCCESS;

  cleanup:

  asynctest_server_quit();

  return exit_code;
}

/* ========================================================================= *
 * MAIN ENTRY POINT
 * ========================================================================= */

int
main(int ac, char **av)
{
  asynctest_sighnd_init();
  signal(SIGPIPE, SIG_IGN);

  log_set_level(LOG_DEBUG);
  log_open("asynctest", LOG_TO_STDERR, 1);

  return asynctest_mainloop_run(ac, av);
}

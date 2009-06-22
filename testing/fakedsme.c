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
 * fake dsme service for alarmd testing
 *
 * 05-Sep-2008 Simo Piiroinen
 * ========================================================================= */

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include <dbus/dbus-glib-lowlevel.h>
#include "../src/logging.h"
#include "../src/missing_dbus.h"
#include "../src/alarm_dbus.h"

#include "../src/dbusif.h"

#define SEND_SAVEDATA_SIGNALS 0

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

static DBusConnection *fakedsme_session_bus  = 0;
static DBusConnection *fakedsme_system_bus  = 0;
static GMainLoop      *fakedsme_mainloop_hnd = 0;
#if SEND_SAVEDATA_SIGNALS
static guint           fakedsme_savedata_tmr = 0;
#endif

/* ------------------------------------------------------------------------- *
 * fakedsme_mainloop_stop
 * ------------------------------------------------------------------------- */

static
int
fakedsme_mainloop_stop(int force)
{
  if( fakedsme_mainloop_hnd == 0 )
  {
    log_warning("fakedsme_mainloop_stop: no main loop\n");
    exit(EXIT_FAILURE);
  }
  g_main_loop_quit(fakedsme_mainloop_hnd);
  return 1;
}

/* ------------------------------------------------------------------------- *
 * fakedsme_sighnd_terminate  --  terminate process from signal handler
 * ------------------------------------------------------------------------- */

static
void
fakedsme_sighnd_terminate(void)
{
  static int done = 0;

  switch( ++done )
  {
  case 1:
    if( fakedsme_mainloop_stop(0) )
    {
      break;
    }

  case 2:
    exit(1);

  default:
    _exit(1);
  }
}

/* ------------------------------------------------------------------------- *
 * fakedsme_sighnd_handler  --  handle trapped signals
 * ------------------------------------------------------------------------- */

static
void
fakedsme_sighnd_handler(int sig)
{
  log_error("Got signal [%d] %s\n", sig, strsignal(sig));

  switch( sig )
  {
  case SIGINT:
  case SIGHUP:
  case SIGTERM:
  case SIGQUIT:
    signal(sig, fakedsme_sighnd_handler);
    fakedsme_sighnd_terminate();
    break;
  }
}

/* ------------------------------------------------------------------------- *
 * fakedsme_sighnd_init  --  setup signal trapping
 * ------------------------------------------------------------------------- */

static
void
fakedsme_sighnd_init(void)
{
  static const int sig[] =
  {
    SIGHUP,
    SIGINT,
    SIGQUIT,
    SIGTERM,
    -1
  };

  for( int i = 0; sig[i] != -1; ++i )
  {
    signal(sig[i], fakedsme_sighnd_handler);
  }
}

/* ------------------------------------------------------------------------- *
 * fakedsme_savedata_cb  --  broadcast savedata signals
 * ------------------------------------------------------------------------- */

#if SEND_SAVEDATA_SIGNALS
static gboolean
fakedsme_savedata_cb(gpointer data)
{
  int          err = -1;
  DBusMessage *msg = 0;

  msg = dbus_message_new_signal(DSME_SIGNAL_PATH,
                                DSME_SIGNAL_IF,
                                DSME_DATA_SAVE_SIG);

  if( msg != 0 && fakedsme_system_bus != 0 )
  {
    if( dbus_connection_send(fakedsme_system_bus, msg, 0) )
    {
      err = 0;
    }
  }

  log_debug("DBUS: savedata signal sending: %s\n", err ? "FAILED":"OK");

  return TRUE;
}
#endif

/* ------------------------------------------------------------------------- *
 * fakedsme_server_filter  -- handle requests coming via dbus
 * ------------------------------------------------------------------------- */

static
DBusHandlerResult
fakedsme_server_filter(DBusConnection *conn,
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

  if( !strcmp(interface, DSME_REQUEST_IF ) &&
      !strcmp(object, DSME_REQUEST_PATH) )
  {
    if( type == DBUS_MESSAGE_TYPE_METHOD_CALL )
    {
      if( !strcmp(member, DSME_GET_VERSION) )
      {
        log_debug("got DSME_GET_VERSION\n");
      }
      else if( !strcmp(member, DSME_REQ_POWERUP) )
      {
        log_debug("got DSME_REQ_POWERUP\n");
      }
      else if( !strcmp(member, DSME_REQ_REBOOT) )
      {
        log_debug("got DSME_REQ_REBOOT\n");
      }
      else if( !strcmp(member, DSME_REQ_SHUTDOWN) )
      {
        log_debug("got DSME_REQ_SHUTDOWN\n");
      }
// QUARANTINE       else if( !strcmp(member, DSME_REQ_ALARM_MODE_CHANGE) )
// QUARANTINE       {
// QUARANTINE         const char *mode = NULL;
// QUARANTINE         DBusError   err = DBUS_ERROR_INIT;
// QUARANTINE
// QUARANTINE         dbus_message_get_args(msg, &err,
// QUARANTINE                               DBUS_TYPE_STRING, &mode,
// QUARANTINE                               DBUS_TYPE_INVALID);
// QUARANTINE         dbus_error_free(&err);
// QUARANTINE         log_debug("got DSME_REQ_ALARM_MODE_CHANGE: %s\n", mode);
// QUARANTINE       }
      else
      {
        log_debug("got UNKNOWN '%s'\n", member);
        rsp = dbus_message_new_error(msg, DBUS_ERROR_UNKNOWN_METHOD, member);
      }
    }
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
 * fakedsme_server_init
 * ------------------------------------------------------------------------- */

static
int
fakedsme_server_init(void)
{
  int         res = -1;
  DBusError   err = DBUS_ERROR_INIT;

  if( (fakedsme_system_bus = dbus_bus_get(DBUS_BUS_SYSTEM, &err)) == 0 )
  {
    log_error("%s: %s\n", err.name, err.message);
    goto cleanup;
  }

  if( (fakedsme_session_bus = dbus_bus_get(DBUS_BUS_SESSION, &err)) == 0 )
  {
    log_error("%s: %s\n", err.name, err.message);
    goto cleanup;
  }

  dbus_connection_setup_with_g_main(fakedsme_system_bus, NULL);
  dbus_connection_set_exit_on_disconnect(fakedsme_system_bus, 0);

  dbus_connection_setup_with_g_main(fakedsme_session_bus, NULL);
  dbus_connection_set_exit_on_disconnect(fakedsme_session_bus, 0);

  if( !dbus_connection_add_filter(fakedsme_system_bus, fakedsme_server_filter, 0, 0) )
  {
    goto cleanup;
  }

  if( !dbus_connection_add_filter(fakedsme_session_bus, fakedsme_server_filter, 0, 0) )
  {
    goto cleanup;
  }

  int ret = dbus_bus_request_name(fakedsme_system_bus,
                                  DSME_SERVICE,
                                  DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                  &err);

  if( ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
  {
    if( dbus_error_is_set(&err) )
    {
      log_error("Name Error (%s)\n", err.message);
    }
    else
    {
      log_error("not primary owner of connection\n");
    }
    goto cleanup;
  }

  dbus_bus_add_match(fakedsme_session_bus, MATCH_ALARMD_OWNERCHANGED, &err);
  dbus_bus_add_match(fakedsme_session_bus, MATCH_ALARMD_STATUS_IND, &err);

  res = 0;

  cleanup:

  dbus_error_free(&err);
  return res;
}

/* ------------------------------------------------------------------------- *
 * fakedsme_server_quit
 * ------------------------------------------------------------------------- */

static
void
fakedsme_server_quit(void)
{
  if( fakedsme_session_bus != 0 )
  {
    DBusError   err = DBUS_ERROR_INIT;
    dbus_bus_add_match(fakedsme_session_bus, MATCH_ALARMD_OWNERCHANGED, &err);
    dbus_bus_add_match(fakedsme_session_bus, MATCH_ALARMD_STATUS_IND, &err);
    dbus_error_free(&err);

    dbus_connection_unref(fakedsme_session_bus);
    fakedsme_session_bus = 0;
  }

  if( fakedsme_system_bus != 0 )
  {
    dbus_connection_unref(fakedsme_system_bus);
    fakedsme_system_bus = 0;
  }
}

/* ------------------------------------------------------------------------- *
 * fakedsme_mainloop_run
 * ------------------------------------------------------------------------- */

static
int
fakedsme_mainloop_run(int argc, char* argv[])
{
  int exit_code = EXIT_FAILURE;
  signal(SIGPIPE, SIG_IGN);
  fakedsme_sighnd_init();

  if( fakedsme_server_init() == -1 )
  {
    goto cleanup;
  }

  fakedsme_mainloop_hnd = g_main_loop_new(NULL, FALSE);

#if SEND_SAVEDATA_SIGNALS
  fakedsme_savedata_tmr = g_timeout_add(10*1000, fakedsme_savedata_cb, 0);
#endif

  log_info("ENTER MAINLOOP\n");
  g_main_loop_run(fakedsme_mainloop_hnd);
  log_info("LEAVE MAINLOOP\n");

  g_main_loop_unref(fakedsme_mainloop_hnd);
  fakedsme_mainloop_hnd = 0;

  exit_code = EXIT_SUCCESS;

  cleanup:

  fakedsme_server_quit();

  return exit_code;
}

/* ========================================================================= *
 * MAIN ENTRY POINT
 * ========================================================================= */

int
main(int ac, char **av)
{
  log_set_level(LOG_DEBUG);
  log_open("fakedsme", LOG_TO_STDERR, 1);

  return fakedsme_mainloop_run(ac, av);
}

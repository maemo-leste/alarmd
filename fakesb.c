/* ========================================================================= *
 * fake statusbar service for alarmd testing
 *
 * 05-Sep-2008 Simo Piiroinen
 * ========================================================================= */

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include <dbus/dbus-glib-lowlevel.h>
#include "logging.h"
#include "missing_dbus.h"
#include <clock-plugin-dbus.h>

// QUARANTINE // TODO: remove when available in dbus
// QUARANTINE #ifndef DBUS_ERROR_INIT
// QUARANTINE # define DBUS_ERROR_INIT { NULL, NULL, TRUE, 0, 0, 0, 0, NULL }
// QUARANTINE #endif

static DBusConnection *fakesb_session_bus  = 0;
static GMainLoop      *fakesb_mainloop_hnd = 0;

/* ------------------------------------------------------------------------- *
 * fakesb_mainloop_stop
 * ------------------------------------------------------------------------- */

static
int
fakesb_mainloop_stop(int force)
{
  if( fakesb_mainloop_hnd == 0 )
  {
    log_warning("fakesb_mainloop_stop: no main loop\n");
    exit(EXIT_FAILURE);
  }
  g_main_loop_quit(fakesb_mainloop_hnd);
  return 1;
}

/* ------------------------------------------------------------------------- *
 * fakesb_sighnd_terminate  --  terminate process from signal handler
 * ------------------------------------------------------------------------- */

static
void
fakesb_sighnd_terminate(void)
{
  static int done = 0;

  switch( ++done )
  {
  case 1:
    if( fakesb_mainloop_stop(0) )
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
 * fakesb_sighnd_handler  --  handle trapped signals
 * ------------------------------------------------------------------------- */

static
void
fakesb_sighnd_handler(int sig)
{
  log_error("Got signal [%d] %s\n", sig, strsignal(sig));

  switch( sig )
  {
  case SIGINT:
  case SIGHUP:
  case SIGTERM:
  case SIGQUIT:
    signal(sig, fakesb_sighnd_handler);
    fakesb_sighnd_terminate();
    break;
  }
}

/* ------------------------------------------------------------------------- *
 * fakesb_sighnd_init  --  setup signal trapping
 * ------------------------------------------------------------------------- */

static
void
fakesb_sighnd_init(void)
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
    signal(sig[i], fakesb_sighnd_handler);
  }
}

/* ------------------------------------------------------------------------- *
 * fakesb_server_filter  -- handle requests coming via dbus
 * ------------------------------------------------------------------------- */

static
DBusHandlerResult
fakesb_server_filter(DBusConnection *conn,
                   DBusMessage *msg,
                   void *user_data)
{
  DBusHandlerResult   result    = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  const char         *interface = dbus_message_get_interface(msg);
  const char         *member    = dbus_message_get_member(msg);
  const char         *object    = dbus_message_get_path(msg);
  int                 type      = dbus_message_get_type(msg);
  DBusMessage        *rsp       = 0;

// QUARANTINE   const char         *typestr   = dbusif_get_msgtype_name(type);
// QUARANTINE   log_debug("----------------------------------------------------------------\n");
// QUARANTINE   log_debug("@ %s: %s(%s, %s, %s)\n", __FUNCTION__, typestr, object, interface, member);

  if( !interface || !member || !object )
  {
    goto cleanup;
  }

  if( !strcmp(interface, STATUSAREA_CLOCK_INTERFACE) &&
      !strcmp(object, STATUSAREA_CLOCK_PATH) )
  {
    if( type == DBUS_MESSAGE_TYPE_METHOD_CALL )
    {
      if( !strcmp(member, STATUSAREA_CLOCK_ALARM_SET) )
      {
        log_debug("got STATUSAREA_CLOCK_ALARM_SET\n");
      }
      else if( !strcmp(member, STATUSAREA_CLOCK_NO_ALARMS) )
      {
        log_debug("got STATUSAREA_CLOCK_NO_ALARMS\n");
      }
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

// QUARANTINE   log_debug("----------------------------------------------------------------\n");

  return result;
}

/* ------------------------------------------------------------------------- *
 * fakesb_server_init
 * ------------------------------------------------------------------------- */

static
int
fakesb_server_init(void)
{
  int         res = -1;
  DBusError   err = DBUS_ERROR_INIT;

  if( (fakesb_session_bus = dbus_bus_get(DBUS_BUS_SESSION, &err)) == 0 )
  {
    goto cleanup;
  }

  dbus_connection_setup_with_g_main(fakesb_session_bus, NULL);
  dbus_connection_set_exit_on_disconnect(fakesb_session_bus, 0);

  if( !dbus_connection_add_filter(fakesb_session_bus, fakesb_server_filter, 0, 0) )
  {
    goto cleanup;
  }

  int ret = dbus_bus_request_name(fakesb_session_bus,
                                  STATUSAREA_CLOCK_SERVICE,
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

  res = 0;

  cleanup:

  dbus_error_free(&err);
  return res;
}

/* ------------------------------------------------------------------------- *
 * fakesb_server_quit
 * ------------------------------------------------------------------------- */

static
void
fakesb_server_quit(void)
{
  if( fakesb_session_bus != 0 )
  {
    dbus_connection_unref(fakesb_session_bus);
    fakesb_session_bus = 0;
  }
}

static
gboolean claim_sessionbus_service_cb(gpointer aptr)
{
  const char *name = aptr;
  DBusError   err  = DBUS_ERROR_INIT;

  log_debug("CLAIM SESSION BUS SERVICE: %s\n", name);

  int ret = dbus_bus_request_name(fakesb_session_bus, name,
                                  DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);

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
  }
  dbus_error_free(&err);

  return FALSE;
}

/* ------------------------------------------------------------------------- *
 * fakesb_mainloop_run
 * ------------------------------------------------------------------------- */

static
int
fakesb_mainloop_run(int argc, char* argv[])
{
  int exit_code = EXIT_FAILURE;
  signal(SIGPIPE, SIG_IGN);
  fakesb_sighnd_init();

  if( fakesb_server_init() == -1 )
  {
    goto cleanup;
  }

  // claim desktop service few secs after startup
  // (unless the real thing was already started
  //  by the af-sb-init.sh script)
  g_timeout_add(5000, claim_sessionbus_service_cb,
                "com.nokia.HildonDesktop.Home");

  fakesb_mainloop_hnd = g_main_loop_new(NULL, FALSE);
  log_info("ENTER MAINLOOP\n");
  g_main_loop_run(fakesb_mainloop_hnd);
  log_info("LEAVE MAINLOOP\n");

  exit_code = EXIT_SUCCESS;

  cleanup:

  fakesb_server_quit();

  return exit_code;
}

/* ========================================================================= *
 * MAIN ENTRY POINT
 * ========================================================================= */

int
main(int ac, char **av)
{
  log_set_level(LOG_DEBUG);
  log_open("fakesb", LOG_TO_STDERR, 1);

  return fakesb_mainloop_run(ac, av);
}

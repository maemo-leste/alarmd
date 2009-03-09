/* ========================================================================= *
 * File: alarmd.c
 *
 * Copyright (C) 2008 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 27-May-2008 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#include "alarmd_config.h"

#include "logging.h"
#include "mainloop.h"
#include "xutil.h"

#include <glib-object.h>

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

#if ALARMD_CUD_ENABLE || ALARMD_RFS_ENABLE
#include <dbus/dbus.h>
#include "alarm_dbus.h"
#endif

#if ALARMD_CUD_ENABLE
static int clear_user_data(void)
{
  const char *dest  = ALARMD_SERVICE;
  const char *path  = ALARMD_PATH;
  const char *iface = ALARMD_INTERFACE;
  const char *name  = "clear_user_data";

  dbus_int32_t    nak = -1;
  DBusConnection *con = 0;
  DBusMessage    *msg = 0;
  DBusMessage    *rsp = 0;
  DBusError       err = DBUS_ERROR_INIT;

  if( (con = dbus_bus_get(DBUS_BUS_SYSTEM, &err)) == 0 )
  {
    goto cleanup;
  }

  if( (msg = dbus_message_new_method_call(dest, path, iface, name)) == 0 )
  {
    goto cleanup;
  }

  dbus_message_set_auto_start(msg, FALSE);

  if( !(rsp = dbus_connection_send_with_reply_and_block(con, msg, -1, &err)) )
  {
    goto cleanup;
  }

  dbus_message_get_args(rsp, &err,
                        DBUS_TYPE_INT32, &nak,
                        DBUS_TYPE_INVALID);

  cleanup:

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);
  if( con != 0 ) dbus_connection_unref(con);

  dbus_error_free(&err);

  return (int)nak;
}
#else
# define clear_user_data() (-1)
#endif

#if ALARMD_RFS_ENABLE
static int restore_factory_settings(void)
{
  const char *dest  = ALARMD_SERVICE;
  const char *path  = ALARMD_PATH;
  const char *iface = ALARMD_INTERFACE;
  const char *name  = "restore_factory_settings";

  dbus_int32_t    nak = -1;
  DBusConnection *con = 0;
  DBusMessage    *msg = 0;
  DBusMessage    *rsp = 0;
  DBusError       err = DBUS_ERROR_INIT;

  if( (con = dbus_bus_get(DBUS_BUS_SYSTEM, &err)) == 0 )
  {
    goto cleanup;
  }

  if( (msg = dbus_message_new_method_call(dest, path, iface, name)) == 0 )
  {
    goto cleanup;
  }

  dbus_message_set_auto_start(msg, FALSE);

  if( !(rsp = dbus_connection_send_with_reply_and_block(con, msg, -1, &err)) )
  {
    goto cleanup;
  }

  dbus_message_get_args(rsp, &err,
                        DBUS_TYPE_INT32, &nak,
                        DBUS_TYPE_INVALID);

  cleanup:

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);
  if( con != 0 ) dbus_connection_unref(con);

  dbus_error_free(&err);

  return (int)nak;
}
#else
# define restore_factory_settings() (-1)
#endif

static const char *progname = "<unset>";

static void show_usage(void)
{
  char **targets = log_get_driver_names();
  char **levels  = log_get_level_names();

  printf("NAME\n"
         "  %s %s  --  alarm daemon\n",
         progname, VERS);

  printf("\n"
         "SYNOPSIS\n"
         "  %s [options]\n"
         "\n"
         "  or\n"
         "\n"
         "  /etc/init.d/alarmd start|stop|restart\n",
         progname);

  printf("\n"
         "DESCRIPTION\n"
         "  Alarm daemon manages queue of alarm events, executes\n"
         "  actions defined in events either automatically or\n"
         "  after user input via system ui dialog service.\n"
         );
  printf("\n"
         "OPTIONS\n"
         "  -h          --  this help output\n"
         "  -d          --  run as daemon\n"
         "  -l <target> --  where log is written:\n");

  for( int i = 0; targets[i]; ++i )
  {
    printf("                   * %s\n", targets[i]);
  }

  printf("  -L <level>  --  set verbosity of logging:\n");
  for( int i = 0; levels[i]; ++i )
  {
    printf("                   * %s\n", levels[i]);
  }

  printf("\n"
         "EXAMPLES\n"
         "  /etc/init.d/alarmd stop\n"
         "  alarmd -lstderr -Ldebug\n"
         "\n"
         "    Stop alarmd service and restart with full debug log\n"
         "    written to stderr\n"
         );

  printf("\n"
         "NOTES\n"
         "  The <target> and <level> names are case insensitive\n");

  printf("\n"
         "SEE ALSO\n"
         "  alarmclient\n");

  xfreev(targets);
  xfreev(levels);
}

int
main(int argc, char **argv)
{
  int log_driver = LOG_TO_SYSLOG;
  int log_level  = LOG_WARNING;

  static const char opt_spec[] = "dhl:L:X:";
  int opt_daemon = 0;
  int opt;
  char *s;

  progname = basename(*argv);

  // libconic uses gobjects
  g_type_init();

  while( (opt = getopt (argc, argv, opt_spec)) != -1 )
  {
    switch( opt )
    {
    case 'L':
      log_level = log_parse_level(optarg);
      break;

    case 'l':
      log_driver = log_parse_driver(optarg);
      break;

    case 'd':
      opt_daemon = 1;
      break;

    case 'h':
      show_usage();
      exit(0);

    case 'X':
      if( !strcmp(optarg, "cud") )
      {
        exit( clear_user_data() == -1 ? EXIT_FAILURE : EXIT_SUCCESS);
      }
      else if( !strcmp(optarg, "rfs") )
      {
        exit(restore_factory_settings() == -1 ? EXIT_FAILURE : EXIT_SUCCESS);
      }
      else
      {
        fprintf(stderr, "Unknwon option: -X%s\n", optarg);
      }
      exit(EXIT_FAILURE);
      break;

    case '?':
      if( (s = strchr(opt_spec, optopt)) && (s[1] == ':') )
      {
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      }
      else if( isprint(optopt) )
      {
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      }
      else
      {
        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
      }
      fprintf(stderr, "(use -h for usage)\n");
      exit(1);

    default:
      abort ();
    }
  }

  if( opt_daemon && daemon(0,0) )
  {
    perror("daemon"); exit(1);
  }

  log_set_level(log_level);
  log_open("alarmd", log_driver, 1);
  log_debug("INIT\n");
  int xc = mainloop_run();
  log_debug("EXIT\n");
  log_close();
  return xc;
}

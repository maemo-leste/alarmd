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

static const char *LGPL =
"  This file is part of Alarmd\n"
"\n"
"  Copyright (C) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).\n"
"\n"
"  Contact: Simo Piiroinen <simo.piiroinen@nokia.com>\n"
"\n"
"  Alarmd is free software; you can redistribute it and/or\n"
"  modify it under the terms of the GNU Lesser General Public License\n"
"  version 2.1 as published by the Free Software Foundation.\n"
"\n"
"  Alarmd is distributed in the hope that it will be useful, but\n"
"  WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU\n"
"  Lesser General Public License for more details.\n"
"\n"
"  You should have received a copy of the GNU Lesser General Public\n"
"  License along with Alarmd; if not, write to the Free Software\n"
"  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA\n"
"  02110-1301 USA\n"
;
#include "alarmd_config.h"

#include "logging.h"
#include "mainloop.h"
#include "xutil.h"

#include <glib-object.h>

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
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
         "  -h, --help\n"
         "      Print usage information and exit.\n"

         "  -V, --version\n"
         "      Print version information and exit.\n"

         "  -d, --daemon\n"
         "      Detach from control terminal and run as daemon.\n"

         "  -l <target>, --log-target=<target>\n"
         "      Set logging output, valid targets:\n"
         );

  for( int i = 0; targets[i]; ++i )
  {
    printf("                   * %s\n", targets[i]);
  }

  printf("  -L <level>. --log-level=<level>\n"
         "      Set logging verbosity, valid levels::\n");

  for( int i = 0; levels[i]; ++i )
  {
    printf("                   * %s\n", levels[i]);
  }

  printf("  -Xrfs\n"
         "      Restore factory settings.\n"
         "  -Xcud\n"
         "      Clear user data\n"
         );

  printf("\n"
         "EXAMPLES\n"
         "  /etc/init.d/alarmd stop\n"
         "  alarmd -lstderr -Ldebug\n"
         "\n"
         "    Stop alarmd service and restart with full debug log\n"
         "    written to stderr.\n"
         );

  printf("\n"
         "NOTES\n"
         "  The <target> and <level> names are case insensitive\n");

  printf("\n"
         "Author\n"
         "  Simo Piiroinen <simo.piiroinen@nokia.com>\n"
         );

  printf("\n"
         "COPYRIGHT\n%s", LGPL);

  printf("\n"
         "SEE ALSO\n"
         "  alarmclient(1)\n");

  xfreev(targets);
  xfreev(levels);
}

int
main(int argc, char **argv)
{
  static const char opt_s[] = "hVdl:L:X:";

  static const struct option opt_l[] =
  {
    {"help",       0, 0, 'h'},
    {"usage",      0, 0, 'h'},
    {"version",    0, 0, 'V'},
    {"daemon",     0, 0, 'd'},
    {"log-level",  1, 0, 'L'},
    {"log-target", 1, 0, 'l'},
    {0,            0, 0,  0 }
  };

  int log_driver = LOG_TO_SYSLOG;
  int log_level  = LOG_WARNING;
  int opt_daemon = 0;
  int opt;

  progname = *argv = basename(*argv);

  if( access("/root/alarmd.verbose", F_OK) == 0 )
  {
    log_driver = LOG_TO_STDERR;
    log_level  = LOG_DEBUG;
  }

  // libconic uses gobjects
  g_type_init();

  while( (opt = getopt_long(argc, argv, opt_s, opt_l, 0)) != -1 )
  {
    switch( opt )
    {
    case 'h':
      show_usage();
      exit(0);
    case 'V':
      printf("%s\n", VERS);
      break;

    case 'L':
      log_level = log_parse_level(optarg);
      break;

    case 'l':
      log_driver = log_parse_driver(optarg);
      break;

    case 'd':
      opt_daemon = 1;
      break;

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
      fprintf(stderr, "%s: (use -h for usage)\n", *argv);
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

  log_debug("-- startup --\n");
  int xc = mainloop_run();
  log_debug("-- exit --\n");
  log_close();
  return xc;
}

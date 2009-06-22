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

#include "libalarm.h"
#include "dbusif.h"
#include "logging.h"
#include "alarm_dbus.h"
#include "ticker.h"
#include "strbuf.h"
#include "clockd_dbus.h"

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbus/dbus.h>

/* ------------------------------------------------------------------------- *
 * alarmtool_append_string  --  overflow safe formatted append
 * ------------------------------------------------------------------------- */

static
void
alarmtool_append_string(char **ppos, char *end, char *fmt, ...)
__attribute__((format(printf,3,4)));

static
void
alarmtool_append_string(char **ppos, char *end, char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  vsnprintf(*ppos, end-*ppos, fmt, va);
  va_end(va);
  *ppos = strchr(*ppos,0);
}

/* ------------------------------------------------------------------------- *
 * alarmtool_break_seconds  --  seconds -> days, hours, mins and secs
 * ------------------------------------------------------------------------- */

static int
alarmtool_break_seconds(time_t secs, int *pd, int *ph, int *pm, int *ps)
{
  int d,h,m,s,t = (int)secs;

  s = (t<0) ? -t : t;
  m = s/60, s %= 60;
  h = m/60, m %= 60;
  d = h/24, h %= 24;

  if( pd ) *pd = d;
  if( ph ) *ph = h;
  if( pm ) *pm = m;
  if( ps ) *ps = s;

  return t;
}

/* ------------------------------------------------------------------------- *
 * alarmtool_format_seconds -- seconds -> "#d #h #m #s"
 * ------------------------------------------------------------------------- */

static
char *
alarmtool_format_seconds(char *buf, size_t size, time_t secs)
{
  char *pos = buf;
  char *end = buf + size;

  int d = 0, h = 0, m = 0, s = 0;

  alarmtool_break_seconds(secs, &d,&h,&m,&s);

  *pos = 0;

  if( d ) alarmtool_append_string(&pos,end,"%s%dd", pos>buf?" ":"", d);
  if( h ) alarmtool_append_string(&pos,end,"%s%dh", pos>buf?" ":"", h);
  if( m ) alarmtool_append_string(&pos,end,"%s%dm", pos>buf?" ":"", m);

  if( s || (!d && !h && !m) )
  {
    alarmtool_append_string(&pos,end,"%s%ds", pos>buf?" ":"", s);
  }

  return buf;
}

/* ------------------------------------------------------------------------- *
 * alarmtool_parse_seconds  --  "1d2h3m4s" -> 93784
 * ------------------------------------------------------------------------- */

static
int
alarmtool_parse_seconds(const char *str)
{
  int sgn = 1;
  int tot = 0;
  const char *s = str;

  if( *s == '-' )
  {
    sgn = -1, ++s;
  }

  while( s && *s )
  {
    char *e = (char *)s;
    int   a = strtol(e,&e,10);

    switch( *e )
    {
    case 's': s=e+1, a *= 1; break;
    case 'm': s=e+1, a *= 60; break;
    case 'h': s=e+1, a *= 60 * 60; break;
    case 'd': s=e+1, a *= 60 * 60 * 24; break;
    default: s = ""; break;
    }
    tot += a;
  }
  tot *= sgn;

  return tot;
}

/* ------------------------------------------------------------------------- *
 * alarmtool_show_usage  --  runtime help
 * ------------------------------------------------------------------------- */

static
void
alarmtool_show_usage(const char *progname)
{
  progname = basename(progname);

  printf("NAME\n"
         "  %s %s  --  alarmd test & debugging tool\n",
         progname, VERS);

  printf("\n"
         "SYNOPSIS\n"
         "  %s <options>\n", progname);

  printf("\n"
         "DESCRIPTION\n"
         "  Can be used to add, remove, list and monitor alarm events\n"
         "  held in alarmd queue.\n");

  printf("\n"
         "OPTIONS\n"
         "  -h               -- this help text\n"
         "\n"
         "  -c               --  clear all events\n"
         "  -r[app_id]       --  clear events with given app id\n"
         "  -S[seconds]      --  set default snooze\n"
         );
  printf("\n"
         "EXAMPLES\n"
         "  alarmtool -c\n"
         "     Clear all events in the queue\n"
         );
}

/* ------------------------------------------------------------------------- *
 * alarmtool_clear_events  --  clear alarm events by appid
 * ------------------------------------------------------------------------- */

static
void
alarmtool_clear_events(const char *appid)
{
  cookie_t *cookie = alarmd_event_query(0,0,0,0,appid);

  if( !cookie || !*cookie )
  {
    printf("(no alarms)\n");
  }
  else for( size_t i = 0; cookie[i]; ++i )
  {
    printf("delete %ld -> %s\n", cookie[i],
           alarmd_event_del(cookie[i]) ? "ERR" : "OK");
  }
  free(cookie);
}

/* ------------------------------------------------------------------------- *
 * alarmtool_get_snooze  --  get default snooze time
 * ------------------------------------------------------------------------- */

static
void
alarmtool_get_snooze(void)
{
  unsigned secs = alarmd_get_default_snooze();
  char temp[128];
  printf("default snooze = %u (%s)\n",
         secs,
         alarmtool_format_seconds(temp, sizeof temp, secs));
}

/* ------------------------------------------------------------------------- *
 * alarmtool_set_snooze  --  set default snooze time
 * ------------------------------------------------------------------------- */

static void alarmtool_set_snooze(const char *s)
{
  unsigned snooze = alarmtool_parse_seconds(optarg);

  printf("set snooze %u -> %s\n", snooze,
         alarmd_set_default_snooze(snooze) ? "ERR" : "OK");
  alarmtool_get_snooze();
}

/* ------------------------------------------------------------------------- *
 * MAIN ENTRY POINT
 * ------------------------------------------------------------------------- */

int
main(int argc, char **argv)
{
  for( int opt; (opt = getopt(argc, argv, "hcr:sS:")) != -1; )
  {
    switch( opt )
    {
      // documented options -> generic usage
    case 'h': alarmtool_show_usage(*argv); exit(0);
    case 'c': alarmtool_clear_events(NULL); break;
    case 'r': alarmtool_clear_events(optarg); break;
    case 'S': alarmtool_set_snooze(optarg); break;
    case 's': alarmtool_get_snooze(); break;
    default: /* '?' */
      fprintf(stderr, "Use '-h' for usage information\n");
      exit(EXIT_FAILURE);
    }
  }

  return 0;
}

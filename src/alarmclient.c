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

#include "libalarm.h"
#include "dbusif.h"
#include "logging.h"
#include "alarm_dbus.h"
#include "ticker.h"
#include "strbuf.h"
#include "clockd_dbus.h"
#include "xutil.h"

#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

// appid used in alarm events
#define ALARMCLIENT_APPID        "ALARMCLIENT"

// alarmclient dbus interface
#define ALARMCLIENT_SERVICE      "com.nokia.alarmclient"
#define ALARMCLIENT_INTERFACE    "com.nokia.alarmclient"
#define ALARMCLIENT_PATH         "/com/nokia/alarmclient"

// button click dbus callbacks
#define ALARMCLIENT_CLICK_STOP   "click_stop"
#define ALARMCLIENT_CLICK_SNOOZE "click_snooze"
#define ALARMCLIENT_CLICK_VIEW   "click_view"

// state transition dbus callbacks
#define ALARMCLIENT_ALARM_ADD    "alarm_queued"
#define ALARMCLIENT_ALARM_DEL    "alarm_deleted"
#define ALARMCLIENT_ALARM_TRG    "alarm_triggered"

// number of elements in an array macro
#define numof(a) (sizeof(a)/sizeof*(a))

/* ========================================================================= *
 * USAGE
 * ========================================================================= */

static
void
alarmclient_print_usage(const char *progname)
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
         "  -h               --  print usage information and exit\n"
         "  -V               --  print version information and exit\n"
         "\n"
         "  Alarm queue management:\n"
         "  -l               --  list cookies\n"
         "  -i               --  list cookies + appid + trigger time\n"
         "  -L               --  list cookies + full alarm content\n"
         "  -g[cookie]       --  get event\n"
         "  -d[cookie]       --  delete event\n"
         "  -c               --  clear all events\n"
         "\n"
         "  Constructing alarms:\n"
         "  -b[action_data]  --  add button/action\n"
         "  -D[dbus_data]    --  add dbus data to action\n"
         "  -e[attr_data]    --  add alarm event attributes\n"
         "  -n[event_data]   --  add event with actions\n"
         "\n"
         "  Tracking event states:\n"
         "  -x               --  start dbus service & track events\n"
         "  -a[event_data]   --  add event that is trackable via -x\n"
         "  -A[event_data]   --  add event + show added event\n"
         "\n"
         "  Alarmd settings:\n"
         "  -s               --  get default snooze\n"
         "  -S[seconds]      --  set default snooze\n"
         "\n"
         "  Manipulating system time / timezone via clockd\n"
         "  -t               --  get system vs clockd time diff\n"
         "  -T[seconds]      --  set system vs clockd time diff\n"
         "  -C[seconds]      --  advance clockd time\n"
         "  -z               --  get current clockd timezone\n"
         "  -Z[timezone]     --  set current clockd timezone\n"
         "  -N[enabled]      --  set cellular time autosync\n"

         "\n"
         "  Simulating alarms set by ui components:\n"
         "  -Xclock[@hour[:min[/wday]]]     --  clock ui oneshot alarm\n"
         "  -Xclockr[@hour[:min[/wday]]]    --  clock ui repeating alarm\n"
         "  -Xcalendar[@hour[:min[/wday]]]  --  calendar ui alarm\n"
         "\n"
         "  Issuing requests to dsme:\n"
         "  -Xshutdown --  shutdown -> poweroff / act dead mode\n"
         "  -Xpowerup  --  powerup  -> user mode\n"
         "  -Xrestart  --  restart  -> restart in current mode\n"
         "\n"
         "  Simulating system ui responces:\n"
         "  -r <cookie>/<action> --  Simulate button press\n"
         "\n"
         "  Miscellaneous:\n"
         "  -w <seconds>     --  sleep for specified number of seconds\n"
         "  -W <hh:mm[:ss]>  --  sleep until given time of day is reached\n"
         );

  printf("\n"
         "EXAMPLES\n"
         "  alarmclient \\\n"
         "     -b label=Snooze,flags=TYPE_SNOOZE+WHEN_RESPONDED \\\n"
         "     -b label=Stop,flags=TYPE_DBUS+WHEN_RESPONDED,\\\n"
         "     dbus_service=com.foo.bar,dbus_path=/com/foo/bar,\\\n"
         "     dbus_name=stop_clicked \\\n"
         "     -Dint32:124 -Dstring:hello \\\n"
         "     -n title='Two Button Alarm',message='Hello there',\\\n"
         "     alarm_time=20\n"
         "\n"
         "    Adds basic two button alarm with Stop and Snooze actions,\n"
         "    triggered 20 seconds from now, where pressing stop will\n"
         "    send dbus message with two arguments to session bus\n"
         "\n"
         "    Note: copypasting the above example will not work without\n"
         "          removing leading whitespace\n"
         "\n"
         "  alarmclient -x -Aalarm_time=10\n"
         "\n"
         "    Starts dbus service. Adds 'calendar'-style alarm with\n"
         "    dbus callbacks and prints out event details. Then\n"
         "    follows status of the alarm itself using the action\n"
         "    callbacks. Also tracks presence of alarmd and listens\n"
         "    to alarm queue status signals.\n"
         );

  printf("\n"
         "EVENT DATA\n"
         "  Comma separated list of alarm_event_t member names\n"
         "  with suitable data, for example:\n"
         "    alarm_time=20,title=MyTitle,flags=BOOT+SHOW_ICON\n"
         "\n"
         "  Note: if event.cookie is set, alarmclient will use\n"
         "        alarmd_event_update() instead of alarmd_event_add()\n"
         "\n"
         "ACTION DATA\n"
         "  Comma separated list of alarm_action_t member names\n"
         "  with suitable data, for example:\n"
         "    label=Snooze,flags=TYPE_SNOOZE+WHEN_RESPONDED\n"
         "\n"
         "DBUS DATA\n"
         "  Type specifier + ':' + suitable data for type, examples:\n"
         "    bool:True\n"
         "    int32:42\n"
         "    double:12.34\n"
         "    string:Hello\n"
         "\n"
         "  Note: If type specifier is omitted, guess between int32,\n"
         "        double and string is made.\n"
         "\n"
         "ATTR DATA\n"
         "  Attr Name '=' Attr type ':' Suitable data for type, examples:\n"
         "    location=string:Cafeteria\n"
         "    number=int:42\n"
         "    start=time:12345\n"
         "\n"
         "  Note: If type specifier is omitted, 'string' is assumed.\n"
         );
  printf("\n"
         "NOTES\n"
         "  The -S, -T and -C options also support timespecifiers of\n"
         "  the following format: [-][#d][#h][#m][#[s]], so using\n"
         "  -C3d12h would advance clockd time by 3.5 day.s\n"
         "\n"
         "  The -T option also supports setting absolute date using:\n"
         "     yyyy-mm-dd [hh[:mm[:ss]] (2008-01-03 06:05:00)\n"
         "  Or time within current date using:\n"
         "     hh:mm[:ss] (12:00:00)\n"
         );
  printf("\n"
         "Author\n"
         "  Simo Piiroinen <simo.piiroinen@nokia.com>\n"
         );

  printf("\n"
         "COPYRIGHT\n%s", LGPL);

  printf("\n"
         "SEE ALSO\n"
         "  alarmd(8)\n");
}

/* ========================================================================= *
 * GENERIC UTILITIES
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * alarmclient_detect_clockd
 * ------------------------------------------------------------------------- */

static
void
alarmclient_detect_clockd(void)
{
  int have_clockd = 0;

  DBusConnection *con = 0;
  DBusError       err = DBUS_ERROR_INIT;

  if( !(con = dbus_bus_get(DBUS_BUS_SYSTEM, &err)) )
  {
    log_error("%s: %s: %s\n", "dbus_bus_get", err.name, err.message);
  }
  else
  {
    if( dbusif_check_name_owner(con, CLOCKD_SERVICE) == 0 )
    {
      have_clockd |= 1;
    }
    dbus_connection_unref(con);
  }

  dbus_error_free(&err);

  if( !have_clockd )
  {
    log_warning("CLOCKD not detected - "
                "Using custom timehandling instead of libtime\n");
  }

  ticker_use_libtime(have_clockd);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_emitf  --  printf + fflush
 * ------------------------------------------------------------------------- */

static
void
alarmclient_emitf(const char *fmt, ...) __attribute__((format(printf,1,2)));

static
void
alarmclient_emitf(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  vfprintf(stdout, fmt, va);
  va_end(va);
  fflush(stdout);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_string_append  --  overflow safe formatted append
 * ------------------------------------------------------------------------- */

static
void
alarmclient_string_append(char **ppos, char *end, char *fmt, ...)
__attribute__((format(printf,3,4)));

static
void
alarmclient_string_append(char **ppos, char *end, char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  vsnprintf(*ppos, end-*ppos, fmt, va);
  va_end(va);
  *ppos = strchr(*ppos,0);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_string_to_cookie  --  convert user input to cookie_t
 * ------------------------------------------------------------------------- */

static
cookie_t
alarmclient_string_to_cookie(const char *s)
{
  return strtol(s,0,0);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_string_to_boolean  --  convert user input to int 0/1
 * ------------------------------------------------------------------------- */

static
int
alarmclient_string_to_boolean(const char *s)
{
  const char * const y[] = { "True", "Yes", "On",  "Y", "T", 0 };
  const char * const n[] = { "False", "No", "Off", "N", "F", 0 };

  for( int i = 0; y[i]; ++i )
  {
    if( !strcasecmp(y[i], s) ) return 1;
  }
  for( int i = 0; n[i]; ++i )
  {
    if( !strcasecmp(n[i], s) ) return 0;
  }
  return strtol(s,0,0) != 0;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_string_escape  --  escape string for output as one line
 * ------------------------------------------------------------------------- */

static
const char *
alarmclient_string_escape(const char *txt)
{
  auto void addchr(int c);
  auto void addstr(const char *s);

  static char buf[8<<10];
  char *pos = buf;
  char *end = buf + sizeof buf - 1;

  auto void addchr(int c) { if( pos < end ) *pos++ = c; }

  auto void addstr(const char *s) { while(*s) addchr(*s++); }

  for( *buf = 0; txt && *txt; ++txt )
  {
    switch( *txt )
    {
    case '\n': addstr("{LF}"); break;
    case '\r': addstr("{CR}"); break;
    default:   addchr(*txt); break;
    }
  }
  *pos = 0;
  return buf;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_secs_break  --  seconds -> days, hours, mins and secs
 * ------------------------------------------------------------------------- */

static
int
alarmclient_secs_break(time_t secs, int *pd, int *ph, int *pm, int *ps)
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

  return (t<0) ? -1 : +1;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_secs_format -- seconds -> "#d #h #m #s"
 * ------------------------------------------------------------------------- */

static
char *
alarmclient_secs_format(char *buf, size_t size, time_t secs)
{
  static char tmp[256];

  if( buf == 0 ) buf = tmp, size = sizeof tmp;

  char *pos = buf;
  char *end = buf + size;

  int d = 0, h = 0, m = 0, s = 0;
  int n = alarmclient_secs_break(secs, &d,&h,&m,&s);

  *pos = 0;

  auto const char *sep(void);
  auto const char *sep(void) {
    const char *r="";
    //if( pos>buf ) r=" ";
    if( n ) r=(n<0)?"-":"+", n = 0;
    return r;
  }
  if( d ) alarmclient_string_append(&pos,end,"%s%dd", sep(), d);
  if( h ) alarmclient_string_append(&pos,end,"%s%dh", sep(), h);
  if( m ) alarmclient_string_append(&pos,end,"%s%dm", sep(), m);
  if( s || (!d && !h && !m) )
  {
    alarmclient_string_append(&pos,end,"%s%ds", sep(), s);
  }

  return buf;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_secs_parse  --  "1d2h3m4s" -> 93784
 * ------------------------------------------------------------------------- */

static
int
alarmclient_secs_parse(const char *str)
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
 * alarmclient_date_get_wday_name  --  0 -> "Sun", ..., 6 -> "Sat"
 * ------------------------------------------------------------------------- */

static
const char *
alarmclient_date_get_wday_name(int wday)
{
  static const char * const lut[7] =
  {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
  };

  if( 0 <= wday && wday <= 6 )
  {
    return lut[wday];
  }
  return "???";
}

/* ------------------------------------------------------------------------- *
 * alarmclient_date_format_long
 * ------------------------------------------------------------------------- */

static
char *
alarmclient_date_format_long(char *buf, size_t size, time_t *t)
{
  static char tmp[128];

  struct tm tm;
  ticker_get_local_ex(*t, &tm);

  if( buf == 0 )
  {
    buf = tmp, size = sizeof tmp;
  }

  snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d wd=%s tz=%s dst=%s",
           tm.tm_year + 1900,
           tm.tm_mon + 1,
           tm.tm_mday,
           tm.tm_hour,
           tm.tm_min,
           tm.tm_sec,
           alarmclient_date_get_wday_name(tm.tm_wday),
           tm.tm_zone,
           (tm.tm_isdst > 0) ? "Yes" : "No");

  return buf;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_date_format_short
 * ------------------------------------------------------------------------- */

static
char *
alarmclient_date_format_short(char *buf, size_t size, time_t *t)
{
  static char tmp[128];

  struct tm tm;
  ticker_get_local_ex(*t, &tm);

  if( buf == 0 )
  {
    buf = tmp, size = sizeof tmp;
  }

  snprintf(buf, size, "%s %04d-%02d-%02d %02d:%02d:%02d",
           alarmclient_date_get_wday_name(tm.tm_wday),
           tm.tm_year + 1900,
           tm.tm_mon + 1,
           tm.tm_mday,
           tm.tm_hour,
           tm.tm_min,
           tm.tm_sec);

  return buf;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_strbuf_append
 * ------------------------------------------------------------------------- */

static
int
alarmclient_strbuf_append(strbuf_t *self, char *text)
{
  int err = 0; // assume success

  union {
    uint8_t  u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    int8_t   i8;
    int16_t  i16;
    int32_t  i32;
    int64_t  i64;
    double   dbl;
    char    *str;
    uint32_t flg;
  } v;

  char *type = 0;
  char *data = 0;

  if( (data = strchr(text, ':')) == 0 )
  {
    // no type given: guess between int, double & string
    err = 0;

    v.i32 = strtol(text, &data, 0);
    if( data > text && *data == 0 )
    {
      strbuf_encode(self, tag_int32, &v.i32, tag_done);
      goto cleanup;
    }

    v.dbl = strtod(text, &data);
    if( data > text && *data == 0 )
    {
      strbuf_encode(self, tag_double, &v.dbl, tag_done);
      goto cleanup;
    }

    v.str = text;
    strbuf_encode(self, tag_string, &v.str, tag_done);
    goto cleanup;
  }

  type = text;
  *data++ = 0;

  if( !strcmp(type, "int8") )
  {
    v.i8 = strtol(data, 0, 0);
    strbuf_encode(self, tag_int8, &v.i8, tag_done);
  }
  else if( !strcmp(type, "int16") )
  {
    v.i16 = strtol(data, 0, 0);
    strbuf_encode(self, tag_int16, &v.i16, tag_done);
  }
  else if( !strcmp(type, "int32") )
  {
    v.i32 = strtol(data, 0, 0);
    strbuf_encode(self, tag_int32, &v.i32, tag_done);
  }
  else if( !strcmp(type, "int64") )
  {
    v.i64 = strtoll(data, 0, 0);
    strbuf_encode(self, tag_int64, &v.i64, tag_done);
  }
  else if( !strcmp(type, "uint8") )
  {
    v.u8 = strtoul(data, 0, 0);
    strbuf_encode(self, tag_uint8, &v.u8, tag_done);
  }
  else if( !strcmp(type, "uint16") )
  {
    v.u16 = strtoul(data, 0, 0);
    strbuf_encode(self, tag_uint16, &v.u16, tag_done);
  }
  else if( !strcmp(type, "uint32") )
  {
    v.u32 = strtoul(data, 0, 0);
    strbuf_encode(self, tag_uint32, &v.u32, tag_done);
  }
  else if( !strcmp(type, "uint64") )
  {
    v.u64 = strtoull(data, 0, 0);
    strbuf_encode(self, tag_uint64, &v.u64, tag_done);
  }
  else if( !strcmp(type, "double") )
  {
    v.dbl = strtod(data, 0);
    strbuf_encode(self, tag_double, &v.dbl, tag_done);
  }
  else if( !strcmp(type, "string") )
  {
    v.str = data;
    strbuf_encode(self, tag_string, &v.str, tag_done);
  }
  else if( !strcmp(type, "bool") )
  {
    v.flg = alarmclient_string_to_boolean(data);
    strbuf_encode(self, tag_bool, &v.flg, tag_done);
  }
  else
  {
    err = -1;
    goto cleanup;
  }

  cleanup:

  return err;
}

/* ========================================================================= *
 * BITMASKS TO/FROM STRINGS
 * ========================================================================= */

typedef struct
{
  const char *name;
  unsigned    mask;
} bitsymbol_t;

/* ------------------------------------------------------------------------- *
 * alarmclient_lookup_mask  --  convert string of bitnames into bitmask
 * ------------------------------------------------------------------------- */

static
unsigned alarmclient_lookup_mask(const bitsymbol_t *v, size_t n, const char *s)
{
  unsigned res = 0;
  while( *s )
  {
    int l = strcspn(s, "|+,");
    for( int i = 0; ; ++i )
    {
      if( i == n )
      {
        res |= strtoul(s, 0, 0);
        break;
      }
      if( !strncmp(v[i].name, s, l) )
      {
        res |= v[i].mask;
        break;
      }
    }
    s += l;
    s += (*s != 0);
  }
  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_lookup_name  --  convert bitmask into string of bitnames
 * ------------------------------------------------------------------------- */

static
const char *alarmclient_lookup_name(const bitsymbol_t *v, size_t n, unsigned m)
{
  auto void addchr(int c);
  auto void addstr(const char *s);

  static char buf[512];
  char *pos = buf;
  char *end = buf + sizeof buf - 1;

  auto void addchr(int c) { if( pos < end ) *pos++ = c; }

  auto void addstr(const char *s) { while(*s) addchr(*s++); }

  *buf = 0;
  for( int i = 0; i < n; ++i )
  {
    if( m & v[i].mask )
    {
      if( *buf ) addchr('|');
      addstr(v[i].name);
      m ^= v[i].mask;
    }
  }

  if( m != 0 )
  {
    char hex[16];
    snprintf(hex, sizeof hex, "0x%x", m);
    if( *buf ) addchr('|');
    addstr(hex);
  }
  *pos = 0;
  return buf;
}

/* ========================================================================= *
 * ALARM EVENT FLAGS TO/FROM STRINGS
 * ========================================================================= */

static
const bitsymbol_t lut_alarmeventflags[] =
{
  // alarmeventflags
#define X(n) { .name = #n, .mask = ALARM_EVENT_##n },
  X(BOOT)
  X(ACTDEAD)
  X(SHOW_ICON)
  X(RUN_DELAYED)
  X(CONNECTED)
  X(POSTPONE_DELAYED)
  X(DISABLE_DELAYED)
  X(BACK_RESCHEDULE)
  X(DISABLED)
#undef X
};

/* ------------------------------------------------------------------------- *
 * alarmclient_eventflags_from_text  --  alarm event bitnames into bitmask
 * ------------------------------------------------------------------------- */

static
unsigned
alarmclient_eventflags_from_text(const char *s)
{
  return alarmclient_lookup_mask(lut_alarmeventflags, numof(lut_alarmeventflags), s);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_eventflags_to_text  --  alarm event bitmask into bitnames
 * ------------------------------------------------------------------------- */

static
const char *
alarmclient_eventflags_to_text(unsigned m)
{
  static char buf[512];

  static const char * const states[] =
  {
#define ALARM_STATE(n) #n,
#include "states.inc"
  };
  const char *state = "UNKN";

  unsigned s = m >> ALARM_EVENT_CLIENT_BITS;

  if( s < numof(states) )
  {
    state = states[s];
    m ^= s << ALARM_EVENT_CLIENT_BITS;
  }

  const char *flags = alarmclient_lookup_name(lut_alarmeventflags,
                                              numof(lut_alarmeventflags), m);

  snprintf(buf, sizeof buf, "STATE(%s) %s", state, flags);

  return buf;
}

/* ========================================================================= *
 * ALARM ACTION FLAGS TO/FROM STRINGS
 * ========================================================================= */

static const bitsymbol_t lut_alarmactionflags[] =
{
  // alarmactionflags
#define X(n) { .name = #n, .mask = ALARM_ACTION_##n },
  X(TYPE_NOP)
  X(TYPE_SNOOZE)
  X(TYPE_DBUS)
  X(TYPE_EXEC)
  X(TYPE_DISABLE)

#if ALARMD_ACTION_BOOTFLAGS
  X(TYPE_DESKTOP)
  X(TYPE_ACTDEAD)
#endif

  X(WHEN_QUEUED)
  X(WHEN_TRIGGERED)
  X(WHEN_DISABLED)
  X(WHEN_RESPONDED)
  X(WHEN_DELETED)
  X(WHEN_DELAYED)

  X(DBUS_USE_ACTIVATION)
  X(DBUS_USE_SYSTEMBUS)
  X(DBUS_ADD_COOKIE)

  X(EXEC_ADD_COOKIE)
#undef X
};

/* ------------------------------------------------------------------------- *
 * alarmclient_actionflags_from_text  --  alarm action bitnames into bitmask
 * ------------------------------------------------------------------------- */

static
unsigned
alarmclient_actionflags_from_text(const char *s)
{
  return alarmclient_lookup_mask(lut_alarmactionflags, numof(lut_alarmactionflags), s);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_actionflags_to_text  --  alarm action bitmask into bitnames
 * ------------------------------------------------------------------------- */

static
const char *
alarmclient_actionflags_to_text(unsigned m)
{
  return alarmclient_lookup_name(lut_alarmactionflags, numof(lut_alarmactionflags), m);
}

/* ========================================================================= *
 * STRUCTURE INPUT PARSER
 * ========================================================================= */

typedef struct
{
  const char *name;
  size_t      offs;
  void      (*set)(void *, const char *);
} filler_t;

static
void
alarmclient_struct_set_time(void *aptr, const char *s)
{
  *(time_t *)aptr = strtol(s,0,0);
}
static
void
alarmclient_struct_set_year(void *aptr, const char *s)
{
  int y = strtol(s,0,0);
  *(int *)aptr = (y >= 1900) ? (y - 1900) : y;
}
static
void
alarmclient_struct_set_text(void *aptr, const char *s)
{
  //*(const char **)aptr = s;
  *(char **)aptr = strdup(s ?: "");
}
static
void
alarmclient_struct_set_int(void *aptr, const char *s)
{
  *(int *)aptr = strtol(s,0,0);
}
static
void
alarmclient_struct_set_uns(void *aptr, const char *s)
{
  *(unsigned *)aptr = strtoul(s,0,0);
}

static
void
alarmclient_struct_set_eflag(void *aptr, const char *s)
{
  *(int32_t *)aptr = alarmclient_eventflags_from_text(s);
}
static
void
alarmclient_struct_set_aflag(void *aptr, const char *s)
{
  *(int32_t *)aptr = alarmclient_actionflags_from_text(s);
}

static
void
alarmclient_struct_fill(const filler_t *lut, int cnt, void *aptr, char *filler)
{
  char *base = aptr;

  auto int match(const char *label, const char *filler);
  auto int match(const char *label, const char *filler)
  {
    if( !strcmp(label, filler) ) return 1;
    label = strrchr(label, '.');
    return label && !strcmp(label+1, filler);
  }

  int last = -1;
  char *next = 0;
  char *arg = 0;

  for( ; filler && *filler; filler = next )
  {
    if( (next = strchr(filler, ',')) != 0 )
    {
      *next++ = 0;
    }

    if( (arg = strchr(filler, '=')) )
    {
      *arg++ = 0;

      for(  last = 0; ; ++last )
      {
        if( last == cnt )
        {
          alarmclient_emitf("Unknown event member '%s'\n", filler);
          exit(EXIT_FAILURE);
        }
        if( match( lut[last].name, filler) )
        {
          alarmclient_emitf("SET '%s' '%s'\n", lut[last].name, arg);
          lut[last].set(base + lut[last].offs, arg);
          break;
        }
      }
    }
    else
    {
      if( ++last >= cnt )
      {
        alarmclient_emitf("member %d out of bounds (%d)\n", last, cnt);
        exit(EXIT_FAILURE);
      }
      alarmclient_emitf("SET '%s' '%s'\n", lut[last].name, filler);
      lut[last].set(base + lut[last].offs, filler);
    }
  }
}

/* ========================================================================= *
 * ACTIONS
 * ========================================================================= */

// INPUT PARSER: alarm_action_t
static const filler_t action_filler[] =
{
#define X(t,m) { #m, offsetof(alarm_action_t, m), alarmclient_struct_set_##t },
  X(aflag,  flags)
  X(text,   label)
  X(text,   exec_command)
  X(text,   dbus_interface)
  X(text,   dbus_service)
  X(text,   dbus_path)
  X(text,   dbus_name)
  X(text,   dbus_args)
#undef X
};

/* ------------------------------------------------------------------------- *
 * alarmclient_action_fill_fields
 * ------------------------------------------------------------------------- */

static
void
alarmclient_action_fill_fields(alarm_action_t *act, char *text)
{
  alarmclient_struct_fill(action_filler, numof(action_filler), act, text);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_action_add_dbus_arg
 * ------------------------------------------------------------------------- */

static
void
alarmclient_action_add_dbus_arg(alarm_action_t *act, char *text)
{
  strbuf_t buf;
  char *prev = act->dbus_args;

  strbuf_ctor_ex(&buf, prev ?: "");
  alarmclient_strbuf_append(&buf, text);

  free(prev);
  act->dbus_args = strbuf_steal(&buf);

  strbuf_dtor(&buf);
}

/* ========================================================================= *
 * EVENTS
 * ========================================================================= */

// INPUT PARSER: alarm_event_t
static const filler_t event_filler[] =
{
#define X1(t,m) { #m, offsetof(alarm_event_t, m), alarmclient_struct_set_##t },
#define X2(t,n,m) { #n, offsetof(alarm_event_t, m), alarmclient_struct_set_##t },

  X2(uns,    cookie,  ALARMD_PRIVATE(cookie))
  X2(time,   trigger, ALARMD_PRIVATE(trigger))

  X1(text,   title)
  X1(text,   message)
  X1(text,   sound)
  X1(text,   icon)
  X1(eflag,  flags)
  X1(text,   alarm_appid)

  X1(time,   alarm_time)
  X1(year,   alarm_tm.tm_year)
  X1(int,    alarm_tm.tm_mon)
  X1(int,    alarm_tm.tm_mday)
  X1(int,    alarm_tm.tm_hour)
  X1(int,    alarm_tm.tm_min)
  X1(int,    alarm_tm.tm_sec)
  X1(int,    alarm_tm.tm_wday)
  X1(int,    alarm_tm.tm_yday)
  X1(int,    alarm_tm.tm_isdst)
  X1(text,   alarm_tz)

  X1(time,   recur_secs)
  X1(int,    recur_count)

  X1(time,   snooze_secs)
  X1(int,    snooze_total)

  X1(int,    action_cnt)
#undef X1
#undef X2
};

/* ------------------------------------------------------------------------- *
 * alarmclient_event_fill_fields
 * ------------------------------------------------------------------------- */

static
void
alarmclient_event_fill_fields(alarm_event_t *eve, char *text)
{
  alarmclient_struct_fill(event_filler, numof(event_filler), eve, text);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_event_set_attr
 * ------------------------------------------------------------------------- */

static
void
alarmclient_event_set_attr(alarm_event_t *eve, char *text)
{
  char *key  = text;
  char *type = 0;
  char *val  = 0;

  if( (type = strchr(key, '=')) != 0 )
  {
    *type++ = 0;
    if( (val = strchr(type, ':')) != 0 )
    {
      *val++ = 0;
    }
    else
    {
      val = type, type = 0;
    }
    if( type == 0 || !strcmp(type, "string") )
    {
      alarm_event_set_attr_string(eve, key, val);
    }
    else if( !strcmp(type, "int") )
    {
      alarm_event_set_attr_int(eve, key, strtol(val, 0, 0));
    }
    else if( !strcmp(type, "time_t") || !strcmp(type, "time") )
    {
      alarm_event_set_attr_int(eve, key, strtol(val, 0, 0));
    }
    else
    {
      log_error("unsupported attribute type '%s'\n", type);
    }
  }
  else
  {
    log_error("attribute format: <key>=<type>:<value>\n");
  }
}

/* ------------------------------------------------------------------------- *
 * alarmclient_event_show  --  print event data in human readable form
 * ------------------------------------------------------------------------- */

static
void
alarmclient_event_show(alarm_event_t *event)
{
  alarmclient_emitf("alarm_event_t {\n");

#define PS2(n,m) alarmclient_emitf("\t%-17s '%s'\n", #n, alarmclient_string_escape(event->m))
#define PN2(n,m) alarmclient_emitf("\t%-17s %ld\n",  #n, (long)(event->m))
#define PX2(n,m) alarmclient_emitf("\t%-17s %s\n",   #n, alarmclient_eventflags_to_text(event->m))

#define PS(m) PS2(m,m)
#define PN(m) PN2(m,m)
#define PX(m) PX2(m,m)

  PN2(cookie, ALARMD_PRIVATE(cookie));

  {
    time_t now = ticker_get_time();
    time_t trg = alarm_event_get_trigger(event);
    time_t dif = now - trg;

    alarmclient_emitf("\t%-17s %ld -> %s (T%s)\n",
                      "trigger",
                      (long)trg,
                      alarmclient_date_format_long(0, 0, &trg),
                      alarmclient_secs_format(0, 0, dif));
  }

  PS(title);
  PS(message);
  PS(sound);
  PS(icon);
  PX(flags);

  PS(alarm_appid);

  PN(alarm_time);
  PN(alarm_tm.tm_year);
  PN(alarm_tm.tm_mon);
  PN(alarm_tm.tm_mday);
  PN(alarm_tm.tm_hour);
  PN(alarm_tm.tm_min);
  PN(alarm_tm.tm_sec);
  PN(alarm_tm.tm_wday);
  PN(alarm_tm.tm_yday);
  PN(alarm_tm.tm_isdst);
  PS(alarm_tz);

  PN(recur_secs);
  PN(recur_count);

  PN(snooze_secs);
  PN(snooze_total);

  PN(action_cnt);
  PN(recurrence_cnt);
  PN(attr_cnt);

#undef PX
#undef PN
#undef PS

#undef PX2
#undef PN2
#undef PS2

  for( size_t i = 0; i < event->action_cnt; ++i )
  {
    alarm_action_t *act = &event->action_tab[i];

    alarmclient_emitf("\n\tACTION %d:\n", (int)i);

#define PS(v) alarmclient_emitf("\t%-17s '%s'\n", #v, alarmclient_string_escape(act->v))
#define PX(v) alarmclient_emitf("\t%-17s %s\n", #v, alarmclient_actionflags_to_text(act->v))

    //alarmclient_emitf("\n");
    PX(flags);
    PS(label);
    PS(exec_command);
    PS(dbus_interface);
    PS(dbus_service);
    PS(dbus_path);
    PS(dbus_name);
    PS(dbus_args);

#undef PX
#undef PS
  }

  for( size_t i = 0; i < event->recurrence_cnt; ++i )
  {
    alarm_recur_t *rec = &event->recurrence_tab[i];

    alarmclient_emitf("\n\tRECURRENCY %d:\n", (int)i);

    alarmclient_emitf("\t%-17s", "mask_min");
    switch( rec->mask_min )
    {
    case ALARM_RECUR_MIN_DONTCARE:
      alarmclient_emitf(" ALARM_RECUR_MIN_DONTCARE\n");
      break;
    case ALARM_RECUR_MIN_ALL:
      alarmclient_emitf(" ALARM_RECUR_MIN_ALL\n");
      break;
    default:
      for( int i = 0; i < 60; ++i )
      {
        if( rec->mask_min & (1ull << i) )
        {
          alarmclient_emitf(" %d", i);
        }
      }
      alarmclient_emitf("\n");
      break;
    }

    alarmclient_emitf("\t%-17s", "mask_hour");
    switch( rec->mask_hour )
    {
    case ALARM_RECUR_HOUR_DONTCARE:
      alarmclient_emitf(" ALARM_RECUR_HOUR_DONTCARE\n");
      break;
    case ALARM_RECUR_HOUR_ALL:
      alarmclient_emitf(" ALARM_RECUR_HOUR_ALL\n");
      break;
    default:
      for( int i = 0; i < 24; ++i )
      {
        if( rec->mask_hour & (1 << i) )
        {
          alarmclient_emitf(" %d", i);
        }
      }
      alarmclient_emitf("\n");
      break;
    }

    alarmclient_emitf("\t%-17s", "mask_mday");
    switch( rec->mask_mday )
    {
    case ALARM_RECUR_MDAY_DONTCARE:
      alarmclient_emitf(" ALARM_RECUR_MDAY_DONTCARE\n");
      break;
    case ALARM_RECUR_MDAY_ALL:
      alarmclient_emitf(" ALARM_RECUR_MDAY_ALL\n");
      break;
    default:
      for( int i = 1; i <= 31; ++i )
      {
        if( rec->mask_mday & (1 << i) )
        {
          alarmclient_emitf(" %d", i);
        }
      }
      if( rec->mask_mday & ALARM_RECUR_MDAY_EOM )
      {
        alarmclient_emitf(" ALARM_RECUR_MDAY_EOM");
      }
      alarmclient_emitf("\n");
      break;
    }

    alarmclient_emitf("\t%-17s", "mask_wday");
    switch( rec->mask_wday )
    {
    case ALARM_RECUR_WDAY_DONTCARE:
      alarmclient_emitf(" ALARM_RECUR_WDAY_DONTCARE\n");
      break;
    case ALARM_RECUR_WDAY_ALL:
      alarmclient_emitf(" ALARM_RECUR_WDAY_ALL\n");
      break;
    default:
      for( int i = 0; i < 7; ++i )
      {
        if( rec->mask_wday & (1 << i) )
        {
          alarmclient_emitf(" %s", alarmclient_date_get_wday_name(i));
        }
      }
      alarmclient_emitf("\n");
      break;
    }

    alarmclient_emitf("\t%-17s", "mask_mon");
    switch( rec->mask_mon )
    {
    case ALARM_RECUR_MON_DONTCARE:
      alarmclient_emitf(" ALARM_RECUR_MON_DONTCARE\n");
      break;
    case ALARM_RECUR_MON_ALL:
      alarmclient_emitf(" ALARM_RECUR_MON_ALL\n");
      break;
    default:
      for( int i = 0; i < 12; ++i )
      {
        static const char * const mon[12] =
        {
          "Jan","Feb","Mar","Apr","May","Jun",
          "Jul","Aug","Sep","Oct","Nov","Dec"
        };
        if( rec->mask_mon & (1 << i) )
        {
          alarmclient_emitf(" %s", mon[i]);
        }
      }
      alarmclient_emitf("\n");
      break;
    }

    alarmclient_emitf("\t%-17s", "special");

    switch( rec->special )
    {
    case ALARM_RECUR_SPECIAL_NONE:
      alarmclient_emitf(" ALARM_RECUR_SPECIAL_NONE\n");
      break;
    case ALARM_RECUR_SPECIAL_BIWEEKLY:
      alarmclient_emitf(" ALARM_RECUR_SPECIAL_BIWEEKLY\n");
      break;
    case ALARM_RECUR_SPECIAL_MONTHLY:
      alarmclient_emitf(" ALARM_RECUR_SPECIAL_MONTHLY\n");
      break;
    case ALARM_RECUR_SPECIAL_YEARLY:
      alarmclient_emitf(" ALARM_RECUR_SPECIAL_YEARLY\n");
      break;
    default:
      alarmclient_emitf(" ALARM_RECUR_SPECIAL_UNKNOWN(%d)\n",
            rec->special);
      break;
    }
  }
  if( event->attr_cnt ) alarmclient_emitf("\n");

  for( size_t i = 0; i < event->attr_cnt; ++i )
  {
    alarm_attr_t *att = event->attr_tab[i];

    alarmclient_emitf("\tATTR '%s' = ", att->attr_name);
    switch( att->attr_type )
    {
    case ALARM_ATTR_NULL:
      alarmclient_emitf("NULL\n");
      break;
    case ALARM_ATTR_INT:
      alarmclient_emitf("INT    %d\n", att->attr_data.ival);
      break;
    case ALARM_ATTR_TIME:
      if( att->attr_data.tval < 365 * 24 * 60 * 60 )
      {
        if( att->attr_data.tval > -365 * 24 * 60 * 60 )
        {
          alarmclient_emitf("TIME   %s\n",
                            alarmclient_secs_format(0,0, att->attr_data.tval));
        }
        else
        {
          alarmclient_emitf("TIME   %+ld\n", (long)att->attr_data.tval);
        }
      }
      else
      {
        alarmclient_emitf("TIME   %s", ctime(&att->attr_data.tval));
      }
      break;
    case ALARM_ATTR_STRING:
      alarmclient_emitf("STRING '%s'\n", att->attr_data.sval);
      break;
    default:
      alarmclient_emitf("???\n");
      break;
    }
  }

  alarmclient_emitf("}\n");
}

/* ========================================================================= *
 * MAINLOOP (optional)
 * ========================================================================= */

static GMainLoop *alarmclient_mainloop_hnd = 0;

/* ------------------------------------------------------------------------- *
 * alarmclient_mainloop_stop
 * ------------------------------------------------------------------------- */

static
void
alarmclient_mainloop_stop(void)
{
  if( alarmclient_mainloop_hnd == 0 )
  {
    exit(EXIT_FAILURE);
  }
  g_main_loop_quit(alarmclient_mainloop_hnd);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_mainloop_run
 * ------------------------------------------------------------------------- */

static
int
alarmclient_mainloop_run(void)
{
  int error = -1;

  /* - - - - - - - - - - - - - - - - - - - *
   * enter mainloop
   * - - - - - - - - - - - - - - - - - - - */

  alarmclient_mainloop_hnd = g_main_loop_new(NULL, FALSE);

  log_info("ENTER MAINLOOP\n");
  g_main_loop_run(alarmclient_mainloop_hnd);
  log_info("LEAVE MAINLOOP\n");

  g_main_loop_unref(alarmclient_mainloop_hnd);
  alarmclient_mainloop_hnd = 0;

  error = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * cleanup & exit
   * - - - - - - - - - - - - - - - - - - - */

  return error;
}

/* ========================================================================= *
 * SIGNAL HANDLER
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * alarmclient_sighnd_terminate  --  terminate process from signal handler
 * ------------------------------------------------------------------------- */

static
void
alarmclient_sighnd_terminate(void)
{
  static int done = 0;

  switch( ++done )
  {
  case 1:
    alarmclient_mainloop_stop();
    break;

  default:
    _exit(EXIT_FAILURE);
  }
}

/* ------------------------------------------------------------------------- *
 * alarmclient_sighnd_handler  --  handle trapped signals
 * ------------------------------------------------------------------------- */

static
void
alarmclient_sighnd_handler(int sig)
{
  log_error("Got signal [%d] %s\n", sig, strsignal(sig));
  signal(sig, alarmclient_sighnd_handler);
  alarmclient_sighnd_terminate();
}

/* ------------------------------------------------------------------------- *
 * alarmclient_sighnd_init  --  setup signal trapping
 * ------------------------------------------------------------------------- */

static
void
alarmclient_sighnd_init(void)
{
  static const int sig[] = { SIGHUP, SIGINT, SIGQUIT, SIGTERM,  -1 };

  /* - - - - - - - - - - - - - - - - - - - *
   * trap some signals for clean exit
   * - - - - - - - - - - - - - - - - - - - */

  for( int i = 0; sig[i] != -1; ++i )
  {
    signal(sig[i], alarmclient_sighnd_handler);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * make writing to closed sockets
   * return -1 instead of raising a
   * SIGPIPE signal
   * - - - - - - - - - - - - - - - - - - - */

  signal(SIGPIPE, SIG_IGN);

  /* - - - - - - - - - - - - - - - - - - - *
   * allow core dumps on segfault
   * - - - - - - - - - - - - - - - - - - - */

#ifndef NDEBUG
  signal(SIGSEGV, SIG_DFL);
#endif

}

/* ========================================================================= *
 * DBUS SERVER (optional)
 * ========================================================================= */

/* NameOwnerChanged signal from dbus daemon for alarmd service name */

#define MATCH_ALARMD_OWNERCHANGED\
  "type='signal'"\
  ",sender='"DBUS_SERVICE_DBUS"'"\
  ",interface='"DBUS_INTERFACE_DBUS"'"\
  ",path='"DBUS_PATH_DBUS"'"\
  ",member='NameOwnerChanged'"\
  ",arg0='"ALARMD_SERVICE"'"

/* ALARMD_QUEUE_STATUS_IND signal from alarmd */

#define MATCH_ALARMD_STATUS_IND\
  "type='signal'"\
  ",sender='"ALARMD_SERVICE"'"\
  ",interface='"ALARMD_INTERFACE"'"\
  ",path='"ALARMD_PATH"'"\
  ",member='"ALARMD_QUEUE_STATUS_IND"'"

static DBusConnection *alarmclient_server_session_bus    = NULL;

static const char * const alarmclient_server_session_sigs[] =
{
  MATCH_ALARMD_OWNERCHANGED,
#ifdef MATCH_ALARMD_STATUS_IND
  MATCH_ALARMD_STATUS_IND,
#endif
  0
};

/* ------------------------------------------------------------------------- *
 * alarmclient_server_handle_callback  --  handle generic cookie callbacks
 * ------------------------------------------------------------------------- */

static
DBusMessage *
alarmclient_server_handle_callback(DBusMessage *msg)
{
  dbus_int32_t  cookie = 0;
  DBusMessage  *rsp    = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_INT32, &cookie,
                                       DBUS_TYPE_INVALID)) )
  {

    // send back the cookie to avoid dbus timeouts
    rsp = dbusif_reply_create(msg,
                              DBUS_TYPE_INT32, &cookie,
                              DBUS_TYPE_INVALID);

  }

  alarmclient_emitf("CALLBACK: %s(%d)\n", dbus_message_get_member(msg), (int)cookie);
  return rsp;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_server_handle_queued  --  handle queued callback
 * ------------------------------------------------------------------------- */

static
DBusMessage *
alarmclient_server_handle_queued(DBusMessage *msg)
{
  dbus_int32_t   cookie = 0;
  DBusMessage   *rsp    = 0;
  alarm_event_t *eve    = 0;

  char           tmp[256];

  *tmp = 0;

  if( !(rsp = dbusif_method_parse_args(msg,
                                       DBUS_TYPE_INT32, &cookie,
                                       DBUS_TYPE_INVALID)) )
  {
    // send back the cookie to avoid dbus timeouts
    rsp = dbusif_reply_create(msg,
                              DBUS_TYPE_INT32, &cookie,
                              DBUS_TYPE_INVALID);

  }

  // augment the output by time-to-trigger info
  if( (cookie > 0) && (eve = alarmd_event_get(cookie)) )
  {
    time_t diff = ticker_get_time() - eve->ALARMD_PRIVATE(trigger);

    snprintf(tmp, sizeof tmp, " Trigger time: T%s",
             alarmclient_secs_format(0,0,diff));
  }

  alarmclient_emitf("CALLBACK: %s(%d)%s\n", dbus_message_get_member(msg), (int)cookie, tmp);

  alarm_event_delete(eve);
  return rsp;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_server_handle_owner  --  handle dbus service ownership signal
 * ------------------------------------------------------------------------- */

static
DBusMessage *
alarmclient_server_handle_owner(DBusMessage *msg)
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

  return 0;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_server_handle_status  --  handle alarmd queue status signal
 * ------------------------------------------------------------------------- */

static
DBusMessage *
alarmclient_server_handle_status(DBusMessage *msg)
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

  alarmclient_emitf("ALARM QUEUE: active=%d, desktop=%d, actdead=%d, no-boot=%d\n",
        ac,dt,ad,nb);

  dbus_error_free(&err);

  return 0;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_server_filter  -- handle requests coming via dbus
 * ------------------------------------------------------------------------- */

static const struct
{
  int          type;
  const char  *iface;
  const char  *object;
  const char  *member;
  DBusMessage *(*func)(DBusMessage *);
  int          result;

} lut[] =
{
  {
    DBUS_MESSAGE_TYPE_METHOD_CALL,
    ALARMCLIENT_INTERFACE,
    ALARMCLIENT_PATH,
    ALARMCLIENT_ALARM_ADD,
    alarmclient_server_handle_queued,
    DBUS_HANDLER_RESULT_HANDLED
  },
  {
    DBUS_MESSAGE_TYPE_METHOD_CALL,
    ALARMCLIENT_INTERFACE,
    ALARMCLIENT_PATH,
    ALARMCLIENT_ALARM_TRG,
    alarmclient_server_handle_callback,
    DBUS_HANDLER_RESULT_HANDLED
  },
  {
    DBUS_MESSAGE_TYPE_METHOD_CALL,
    ALARMCLIENT_INTERFACE,
    ALARMCLIENT_PATH,
    ALARMCLIENT_ALARM_DEL,
    alarmclient_server_handle_callback,
    DBUS_HANDLER_RESULT_HANDLED
  },
  {
    DBUS_MESSAGE_TYPE_METHOD_CALL,
    ALARMCLIENT_INTERFACE,
    ALARMCLIENT_PATH,
    ALARMCLIENT_CLICK_STOP,
    alarmclient_server_handle_callback,
    DBUS_HANDLER_RESULT_HANDLED
  },
  {
    DBUS_MESSAGE_TYPE_METHOD_CALL,
    ALARMCLIENT_INTERFACE,
    ALARMCLIENT_PATH,
    ALARMCLIENT_CLICK_SNOOZE,
    alarmclient_server_handle_callback,
    DBUS_HANDLER_RESULT_HANDLED
  },
  {
    DBUS_MESSAGE_TYPE_METHOD_CALL,
    ALARMCLIENT_INTERFACE,
    ALARMCLIENT_PATH,
    ALARMCLIENT_CLICK_VIEW,
    alarmclient_server_handle_callback,
    DBUS_HANDLER_RESULT_HANDLED
  },
  {
    DBUS_MESSAGE_TYPE_SIGNAL,
    DBUS_INTERFACE_DBUS,
    DBUS_PATH_DBUS,
    "NameOwnerChanged",
    alarmclient_server_handle_owner,
    DBUS_HANDLER_RESULT_NOT_YET_HANDLED
  },
#ifdef MATCH_ALARMD_STATUS_IND
  {
    DBUS_MESSAGE_TYPE_SIGNAL,
    ALARMD_INTERFACE,
    ALARMD_PATH,
    ALARMD_QUEUE_STATUS_IND,
    alarmclient_server_handle_status,
    DBUS_HANDLER_RESULT_NOT_YET_HANDLED
  },
#endif
  { .type = -1, }
};

static
DBusHandlerResult
alarmclient_server_filter(DBusConnection *conn,
                       DBusMessage *msg,
                       void *user_data)
{
  DBusHandlerResult   result    = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  const char         *iface = dbus_message_get_interface(msg);
  const char         *member    = dbus_message_get_member(msg);
  const char         *object    = dbus_message_get_path(msg);
  int                 type      = dbus_message_get_type(msg);
  DBusMessage        *rsp       = 0;

  if( !iface || !member || !object )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * lookup callback to handle the message
   * and possibly generate a reply
   * - - - - - - - - - - - - - - - - - - - */

  for( int i = 0; lut[i].type != -1; ++i )
  {
    if( lut[i].type != type ) continue;
    if( strcmp(lut[i].iface, iface) ) continue;
    if( strcmp(lut[i].object, object) ) continue;
    if( strcmp(lut[i].member, member) ) continue;

    result = lut[i].result;
    rsp  = lut[i].func(msg);
    break;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * if no response message was created
   * above and the peer expects reply,
   * create a generic error message
   * - - - - - - - - - - - - - - - - - - - */

  if( type == DBUS_MESSAGE_TYPE_METHOD_CALL )
  {
    if( rsp == 0 && !dbus_message_get_no_reply(msg) )
    {
      rsp = dbus_message_new_error(msg, DBUS_ERROR_FAILED, member);
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * send response if we have something
   * to send
   * - - - - - - - - - - - - - - - - - - - */

  if( rsp != 0 )
  {
    dbus_connection_send(conn, rsp, 0);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * cleanup and return to caller
   * - - - - - - - - - - - - - - - - - - - */

  cleanup:

  if( rsp != 0 )
  {
    dbus_message_unref(rsp);
  }

  return result;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_server_init
 * ------------------------------------------------------------------------- */

static
int
alarmclient_server_init(void)
{
  int         res = -1;
  DBusError   err = DBUS_ERROR_INIT;

  /* - - - - - - - - - - - - - - - - - - - *
   * connect to session bus
   * - - - - - - - - - - - - - - - - - - - */

  if( (alarmclient_server_session_bus = dbus_bus_get(DBUS_BUS_SESSION, &err)) == 0 )
  {
    log_critical("can't connect to session bus: %s: %s\n",
                 err.name, err.message);
    goto cleanup;
  }

  dbus_connection_setup_with_g_main(alarmclient_server_session_bus, NULL);
  dbus_connection_set_exit_on_disconnect(alarmclient_server_session_bus, 0);

  /* - - - - - - - - - - - - - - - - - - - *
   * add message filter
   * - - - - - - - - - - - - - - - - - - - */

  if( !dbus_connection_add_filter(alarmclient_server_session_bus,
                                  alarmclient_server_filter, 0, 0) )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * request signals to be sent to us
   * - - - - - - - - - - - - - - - - - - - */

  if( dbusif_add_matches(alarmclient_server_session_bus,
                         alarmclient_server_session_sigs) == -1 )
  {
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * claim service name
   * - - - - - - - - - - - - - - - - - - - */

  int ret = dbus_bus_request_name(alarmclient_server_session_bus,
                                  ALARMCLIENT_SERVICE,
                                  DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                  &err);

  if( ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER )
  {
    if( dbus_error_is_set(&err) )
    {
      log_critical("can't claim service name: %s: %s\n", err.name, err.message);
    }
    else
    {
      log_critical("can't claim service name\n");
    }
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * success
   * - - - - - - - - - - - - - - - - - - - */

  res = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * cleanup & return
   * - - - - - - - - - - - - - - - - - - - */

  cleanup:

  dbus_error_free(&err);
  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_server_quit
 * ------------------------------------------------------------------------- */

static
void
alarmclient_server_quit(void)
{
  if( alarmclient_server_session_bus != 0 )
  {
    dbusif_remove_matches(alarmclient_server_session_bus,
                          alarmclient_server_session_sigs);

    dbus_connection_remove_filter(alarmclient_server_session_bus,
                                  alarmclient_server_filter, 0);

    dbus_connection_unref(alarmclient_server_session_bus);
    alarmclient_server_session_bus = 0;
  }
}

/* ========================================================================= *
 * TRACKER
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * alarmclient_tracker_init
 * ------------------------------------------------------------------------- */

static
int
alarmclient_tracker_init(void)
{
  int error = -1;

  /* - - - - - - - - - - - - - - - - - - - *
   * start dbus service
   * - - - - - - - - - - - - - - - - - - - */

  if( alarmclient_server_init() == -1 )
  {
    goto cleanup;
  }

  error = 0;

  cleanup:
  return error;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_tracker_exec
 * ------------------------------------------------------------------------- */

static
int
alarmclient_tracker_exec(void)
{
  int error = -1;

  /* - - - - - - - - - - - - - - - - - - - *
   * enter mainloop
   * - - - - - - - - - - - - - - - - - - - */

  if( alarmclient_mainloop_run() == 0 )
  {
    error = 0;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * stop dbbus service
   * - - - - - - - - - - - - - - - - - - - */

  alarmclient_server_quit();

  return error;
}

/* ========================================================================= *
 * Dynamic event construction
 * ========================================================================= */

static int alarm_count = 0;

/* ------------------------------------------------------------------------- *
 * alarmclient_setup_alarm_with_title
 * ------------------------------------------------------------------------- */

static
alarm_event_t *
alarmclient_setup_alarm_with_title(const char *title,
                                       const char *message)
{
  alarm_event_t *eve = 0;
  char          *t = 0;
  char          *m = 0;

  alarm_count += 1;
  asprintf(&t, "%d. %s", alarm_count, title);
  asprintf(&m, "%d. %s", alarm_count, message);

  eve = alarm_event_create();

  alarm_event_set_alarm_appid(eve, ALARMCLIENT_APPID);
  alarm_event_set_title(eve, t);
  alarm_event_set_message(eve, m);

  free(t);
  free(m);
  return eve;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_setup_alarm_add_to_server
 * ------------------------------------------------------------------------- */

static
cookie_t
alarmclient_setup_alarm_add_to_server(alarm_event_t *eve, int verbose)
{
  cookie_t        aid = 0;

  // send -> alarmd
  aid = alarmd_event_add(eve);
  alarmclient_emitf("cookie = %d\n", (int)aid);

  if( (aid > 0) && (eve = alarmd_event_get(aid)) )
  {
    time_t diff = ticker_get_time() - eve->ALARMD_PRIVATE(trigger);
    alarmclient_emitf("Time left: %s\n",
           alarmclient_secs_format(0, 0, diff));
    alarm_event_delete(eve);
  }

  return aid;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_setup_clock_oneshot
 * ------------------------------------------------------------------------- */

static
cookie_t
alarmclient_setup_clock_oneshot(int h, int m, int wd, int verbose, char *args)
{
  alarm_event_t  *eve = 0;
  alarm_action_t *act = 0;
  cookie_t        aid = 0;

  alarmclient_emitf("----( oneshot alarm %02d:%02d %s )----\n",
         h,m, (wd < 0) ? "anyday" : alarmclient_date_get_wday_name(wd));

  /* - defined in local time
   * -> leave alarm_tz unset
   * -> re-evaluated if tz changes
   *
   * - do not add recurrence masks
   *
   * - set up trigger time via alarm_tm field
   * -> set tm_hour and tm_min fields as required
   * -> if specific weekday is requested set also tm_wday field
   *
   * - if alarm is missed it must be disabled
   * -> set event flag ALARM_EVENT_DISABLE_DELAYED
   *
   * - if user presses "stop", event must be disabled
   * -> set action flag ALARM_ACTION_TYPE_DISABLE
   */

  // event
  eve = alarmclient_setup_alarm_with_title("clock", "oneshot");
  eve->flags |= ALARM_EVENT_DISABLE_DELAYED;
  eve->flags |= ALARM_EVENT_ACTDEAD;
  eve->flags |= ALARM_EVENT_SHOW_ICON;
  eve->flags |= ALARM_EVENT_BACK_RESCHEDULE;

  // action
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_DISABLE;
  alarm_action_set_label(act, "Stop");

  // action
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_SNOOZE;
  alarm_action_set_label(act, "Snooze");

  // trigger
  if( h >= 0 && m >= 0 )
  {
    struct tm tm;
    time_t t = time(0);
    localtime_r(&t,&tm);

    tm.tm_hour = h;
    tm.tm_min  = m;

    if( mktime(&tm) < t )
    {
      tm.tm_mday += 1;
      mktime(&tm);
    }

    eve->alarm_tm.tm_year = tm.tm_year;
    eve->alarm_tm.tm_mon  = tm.tm_mon;
    eve->alarm_tm.tm_mday = tm.tm_mday;
  }
  eve->alarm_tm.tm_hour = h;
  eve->alarm_tm.tm_min  = m;
  eve->alarm_tm.tm_wday = wd;

  // attributes
  alarm_event_set_attr_string(eve, "textdomain", "osso-clock");

  // fill in command line data
  alarmclient_event_fill_fields(eve, args);

  aid = alarmclient_setup_alarm_add_to_server(eve, verbose);
  alarm_event_delete(eve);

  return aid;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_setup_clock_recurring
 * ------------------------------------------------------------------------- */

static
cookie_t
alarmclient_setup_clock_recurring(int h, int m, int wd, int verbose, char *args)
{
  alarm_event_t  *eve = 0;
  alarm_action_t *act = 0;
  alarm_recur_t  *rec = 0;
  cookie_t        aid = 0;

  alarmclient_emitf("----( recurring alarm %02d:%02d every %s )----\n",
         h,m, (wd < 0) ? "day" : alarmclient_date_get_wday_name(wd));

  /* - defined in local time
   * -> leave alarm_tz unset
   * -> re-evaluated if tz changes
   *
   * - do not set trigger time (alarm_time / alarm_tm)
   * -> leave to default values
   *
   * - add recurrence mask
   * -> setup up requested hour/minute in the mask
   *
   * - request reschedule also if time moves backwards
   * -> set event flag ALARM_EVENT_BACK_RESCHEDULE
   */

  // event
  eve = alarmclient_setup_alarm_with_title("clock", "recurring");

  eve->flags |= ALARM_EVENT_BACK_RESCHEDULE;
  eve->flags |= ALARM_EVENT_ACTDEAD;
  eve->flags |= ALARM_EVENT_SHOW_ICON;

  // action
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  alarm_action_set_label(act, "Stop");

  // action
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_SNOOZE;
  alarm_action_set_label(act, "Snooze");

  // recurrence
  rec = alarm_event_add_recurrences(eve, 1);
  rec->mask_hour = 1u   << h;
  rec->mask_min  = 1ull << m;
  rec->mask_wday = (wd < 0) ? ALARM_RECUR_WDAY_ALL : (1u << wd);
  eve->recur_count = -1;

  // attributes
  alarm_event_set_attr_string(eve, "textdomain", "osso-clock");

  // fill in user data
  alarmclient_event_fill_fields(eve, args);

  aid = alarmclient_setup_alarm_add_to_server(eve, verbose);
  alarm_event_delete(eve);

  return aid;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_setup_calendar_alarm
 * ------------------------------------------------------------------------- */

static
cookie_t
alarmclient_setup_calendar_alarm(int h, int m, int wd, int verbose, char *args)
{
  alarm_event_t  *eve = 0;
  alarm_action_t *act = 0;
  cookie_t        aid = 0;
  time_t          now = ticker_get_time();
  time_t          trg = now;
  struct tm       tm;

  alarmclient_emitf("----( calendar alarm %02d:%02d %s )----\n",
         h,m, (wd < 0) ? "anyday" : alarmclient_date_get_wday_name(wd));

  // event
  eve = alarmclient_setup_alarm_with_title("calendar", "alarm");
  alarm_event_set_icon(eve, "/usr/share/icons/hicolor/128x128/hildon/clock_calendar_alarm.png");

  // calendar alarms use absolute time triggering
  ticker_get_local_ex(trg, &tm);
  tm.tm_hour = h;
  tm.tm_min  = m;
  tm.tm_sec  = 0;
  trg = ticker_mktime(&tm, 0);

  if( trg <= now )
  {
    tm.tm_mday += 1;
    trg = ticker_mktime(&tm, 0);
  }

  if( 0 <= wd && wd <= 6 && tm.tm_wday != wd )
  {
    int d = wd - tm.tm_wday;
    tm.tm_mday += (d<0) ? (7+d) : d;
    trg = ticker_mktime(&tm, 0);
  }
  eve->alarm_time = trg;

  // snooze action
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_SNOOZE;
  alarm_action_set_label(act, "Snooze");

  // view action
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  alarm_action_set_label(act, "View");

#if 0
  act->flags |= ALARM_ACTION_TYPE_DBUS;
  alarm_action_set_dbus_service(act, "com.nokia.calendar");
  alarm_action_set_dbus_interface(act, "com.nokia.calendar");
  alarm_action_set_dbus_path(act, "/com/nokia/calendar");
  alarm_action_set_dbus_name(act, "launch_view");
#endif

  // stop action
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  alarm_action_set_label(act, "Stop");

#if 0
  act->flags |= ALARM_ACTION_TYPE_DBUS;
  alarm_action_set_dbus_service(act, "com.nokia.calendar");
  alarm_action_set_dbus_interface(act, "com.nokia.calendar");
  alarm_action_set_dbus_path(act, "/com/nokia/calendar");
  alarm_action_set_dbus_name(act, "calendar_alarm_responded");
#endif
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_EXEC;
  act->flags |= ALARM_ACTION_EXEC_ADD_COOKIE;
  alarm_action_set_exec_command(act, "touch /tmp/stop.flag");

  // attributes

  alarm_event_set_attr_time(eve, "target_time", trg);
  alarm_event_set_attr_string(eve, "location", "Lyyra");

  // fill in user data
  alarmclient_event_fill_fields(eve, args);

  aid = alarmclient_setup_alarm_add_to_server(eve, verbose);
  alarm_event_delete(eve);

  return aid;
}

/* ========================================================================= *
 * COMMAND LINE OPTION HANDLERS
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_show_time
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_show_time(void)
{
  char tz[64];

  ticker_get_timezone(tz, sizeof tz);

  time_t now = ticker_get_time();
  time_t off = ticker_get_offset();

  alarmclient_emitf("TIMEZONE = %s\n", tz);
  alarmclient_emitf("CURRTIME = %s\n", alarmclient_date_format_long(0, 0, &now));
  alarmclient_emitf("TIMEOFFS = %+d\n", (int)off);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_set_time
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_set_time(const char *args)
{
  int Y,M,D, h,m,s;
  time_t t = ticker_get_time();
  struct tm tm;

  Y = M = D = h = m = s = -1;

  if( sscanf(args, "%d-%d-%d %d:%d:%d", &Y,&M,&D, &h,&m,&s) >= 3 )
  {
    ticker_get_local_ex(t, &tm);

    if( Y >= 1900 ) Y -= 1900;
    if( M >= 0    ) M -= 1;

    if( Y >= 0 ) tm.tm_year = Y;
    if( M >= 0 ) tm.tm_mon  = M;
    if( D >= 0 ) tm.tm_mday = D;

    if( h >= 0 ) tm.tm_hour = h;
    if( m >= 0 ) tm.tm_min  = m;
    if( s >= 0 ) tm.tm_sec  = s;

    t = ticker_mktime(&tm, 0);
    alarmclient_emitf("DATE & TIME: %s\n", alarmclient_date_format_long(0, 0, &t));
    ticker_set_time(t);
  }
  else if (sscanf(args, "%d:%d:%d", &h,&m,&s) >= 2 )
  {
    ticker_get_local_ex(t, &tm);
    if( h >= 0 ) tm.tm_hour = h;
    if( m >= 0 ) tm.tm_min  = m;
    if( s >= 0 ) tm.tm_sec  = s;

    t = ticker_mktime(&tm, 0);
    alarmclient_emitf("TIME ONLY: %s\n", alarmclient_date_format_long(0, 0, &t));
    ticker_set_time(t);
  }
  else
  {
    t = alarmclient_secs_parse(args);
    alarmclient_emitf("OFFSET: %+d\n", (int)t);

    ticker_set_offset(t);
  }
  alarmclient_handle_show_time();
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_add_time
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_add_time(const char *args)
{
  ticker_set_time(ticker_get_time() + alarmclient_secs_parse(args));
  alarmclient_handle_show_time();
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_autosync
 * ------------------------------------------------------------------------- */

static void
alarmclient_handle_autosync(const char *args)
{
  ticker_set_autosync(alarmclient_string_to_boolean(args));
}

/* ------------------------------------------------------------------------- *
 * alarmclient_dsme_request
 * ------------------------------------------------------------------------- */

#include "ipc_dsme.h"
static
void
alarmclient_dsme_request(int type)
{
  DBusConnection *con = 0;
  DBusError       err = DBUS_ERROR_INIT;

  if( !(con = dbus_bus_get(DBUS_BUS_SYSTEM, &err)) )
  {
    log_error("%s: %s: %s\n", "dbus_bus_get", err.name, err.message);
  }
  else
  {
    switch( type )
    {
    case 'p':
      log_info("POWERUP REQ -> DSME\n");
      ipc_dsme_req_powerup(con);
      break;
    case 'r':
      log_info("REBOOT REQ -> DSME\n");
      ipc_dsme_req_reboot(con);
      break;
    case 's':
      log_info("SHUTDOWN REQ -> DSME\n");
      ipc_dsme_req_shutdown(con);
      break;
    }
    dbus_connection_flush(con);
    dbus_connection_unref(con);
  }
  dbus_error_free(&err);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_special
 * ------------------------------------------------------------------------- */

static
cookie_t alarmclient_handle_special(const char *args)
{
  auto int inlist(const char *find, const char *list);

  auto int inlist(const char *find, const char *list)
  {
    for( ; *list; list = strchr(list,0) + 1 )
    {
      if( !strcmp(find, list) ) return 1;
    }
    return 0;
  }

  char *tag = strdup(args);
  char *trg = strchr(tag, '@');
  char *arg = strchr(tag, ',');

  cookie_t aid = 0;

  int h = 10;
  int m =  0;
  int w = -1;

  if( trg != 0 ) *trg++ = 0;
  if( arg != 0 ) *arg++ = 0;

  if( trg != 0 )
  {
    sscanf(trg, "%d:%d/%d", &h, &m,&w);
  }

  if( !strcmp(tag, "clock") )
  {
    alarmclient_setup_clock_oneshot(h,m, w, 0, arg);
  }
  else if( !strcmp(tag, "clockr") )
  {
    alarmclient_setup_clock_recurring(h,m,w, 0, arg);
  }
  else if( !strcmp(tag, "calendar") )
  {
    alarmclient_setup_calendar_alarm(h,m,w, 0, arg);
  }
  else if( inlist(tag, "shutdown\0powerdown\0poweroff\0halt\0") )
  {
    alarmclient_dsme_request('s');
  }
  else if( inlist(tag, "powerup\0") )
  {
    alarmclient_dsme_request('p');
  }
  else if( inlist(tag, "restart\0reboot\0") )
  {
    alarmclient_dsme_request('r');
  }
  else if( !strcmp(tag, "nolimbo") )
  {
    unsigned m = 1u << 17;
    alarmd_set_debug(m,0,m,0);
  }
  else if( !strcmp(tag, "limbo") )
  {
    unsigned m = 1u << 17;
    alarmd_set_debug(m,0,0,m);
  }
  else
  {
    log_error("unknown preset '%s'\n", tag);
  }
  free(tag);

  return aid;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_show_timezone
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_show_timezone(void)
{
  char tz[64];
  ticker_get_timezone(tz, sizeof tz);
  alarmclient_emitf("TIMEZONE = '%s'\n", tz);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_set_timezone
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_set_timezone(const char *args)
{
  ticker_set_timezone(args);
  alarmclient_handle_show_timezone();
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_get_snooze
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_get_snooze(void)
{
  alarmclient_emitf("SNOOZE == %u\n", alarmd_get_default_snooze());
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_set_snooze
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_set_snooze(const char *args)
{
  unsigned snooze = alarmclient_secs_parse(args);
  int      error  = alarmd_set_default_snooze(snooze);
  alarmclient_emitf("SNOOZE %u -> %s\n", snooze, error ? "ERR" : "OK");
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_del_event
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_del_event(char *args)
{
  cookie_t cookie = alarmclient_string_to_cookie(args);
  int      error  = alarmd_event_del(cookie);
  alarmclient_emitf("DEL %ld -> %s\n", cookie, error ? "ERR" : "OK");
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_until
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_until(char *args)
{
  struct tm tm;
  time_t t,w;
  int h=0, m=0, s=0;

  if( sscanf(args, "%d:%d:%d",&h,&m,&s) < 2 )
  {
    log_error("-W requires parameter <hh:mm[:ss]>\n");
    exit(1);
  }

  t = ticker_get_time();
  ticker_get_local_ex(t, &tm);
  tm.tm_hour = h;
  tm.tm_min  = m;
  tm.tm_sec  = s;
  w = ticker_mktime(&tm, 0);

  log_debug("Waiting until %s ...\n", alarmclient_date_format_long(0,0,&w));

  while( (t = ticker_get_time()) < w )
  {
    sleep(1);
  }

  log_debug("... resuming at %s\n", alarmclient_date_format_long(0,0,&t));
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_sleep
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_sleep(char *args)
{
  unsigned secs = alarmclient_secs_parse(args);
  log_debug("Sleeping for %u seconds ...\n", secs);
  sleep(secs);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_reply
 * ------------------------------------------------------------------------- */

#include "queue.h"
#include <ctype.h>

static
void
alarmclient_handle_reply(char *args)
{
  alarm_event_t  *eve = 0;
  alarm_action_t *act = 0;

  cookie_t *cookie_dyn = 0;
  cookie_t *cookie_tab = 0;
  cookie_t  cookie_tmp[2] = {0,0};

  int cookie = -1;
  int button = -1;

  char *cookie_s = xsplit(&args, '/');
  char *button_s = xsplit(&args, '/');

  if( isdigit(*button_s) )
  {
    button = strtol(button_s, 0, 0);
  }

  if( (cookie_tmp[0] = strtol(cookie_s,0,0)) > 0 )
  {
    cookie_tab = cookie_tmp;
  }
  else
  {
    cookie_tab = cookie_dyn = alarmd_event_query(0,0, 0,0, cookie_s);
  }

  if( cookie_tab == 0 )
  {
    log_error("-r requires argument of format: <cookie>/<button>\n");
    goto cleanup;
  }

  for( int i = 0; ; ++i )
  {
    if( (cookie = cookie_tab[i]) == 0 )
    {
      log_error("no events in clickable state found\n");
      goto cleanup;
    }

    alarm_event_delete(eve);

    if( (eve = alarmd_event_get(cookie)) == 0 )
    {
      log_error("Cookie %d: No such event\n", cookie);
      continue;
    }

    switch( eve->flags >> ALARM_EVENT_CLIENT_BITS )
    {
    case ALARM_STATE_SYSUI_REQ:
    case ALARM_STATE_SYSUI_ACK:
      break;
    default:
      log_error("Cookie %d: Not in clickable state\n", cookie);
      continue;
    }

    if( button != -1 )
    {
      break;
    }

    for( int k = 0; (act = alarm_event_get_action(eve, k)); ++k )
    {
      if( !alarm_action_is_button(act) )
      {
        continue;
      }
      if( *button_s == 0 || xissame(act->label, button_s) )
      {
        button = k;
        break;
      }
    }

    if( button != -1 )
    {
      break;
    }

    log_error("Cookie %d: No suitable button found\n", cookie);
  }

  if( (act = alarm_event_get_action(eve, button)) == 0 )
  {
    log_error("Cookie %d, action %d: no such action\n", cookie, button);
    goto cleanup;
  }

  if( !alarm_action_is_button(act) )
  {
    log_error("Cookie %d, action %d: not a button action\n", cookie, button);
    goto cleanup;
  }

  if( alarmd_ack_dialog(cookie, button) == -1 )
  {
    log_error("Cookie %d, action %d: sending reply failed\n", cookie, button);
    goto cleanup;
  }

  log_info("Cookie %d, action %d: reply sent\n", cookie, button);

  cleanup:

  free(cookie_dyn);
  alarm_event_delete(eve);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_get_event
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_get_event(char *args)
{
  cookie_t cookie = alarmclient_string_to_cookie(args);
  alarm_event_t *event = alarmd_event_get(cookie);

  alarmclient_emitf("GET %ld -> %s\n", cookie, event ? "OK" : "ERR");

  if( event != 0 )
  {
    alarmclient_event_show(event);
    alarm_event_delete(event);
  }
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_list_events
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_list_events(int verbose)
{
  cookie_t       *cookie = 0;
  alarm_event_t **event  = 0;

  cookie = alarmd_event_query(0,0,0,0,0);

  size_t n;

  for( n = 0; cookie && cookie[n]; ++n ) {}

  if( n == 0 )
  {
    alarmclient_emitf("(no alarms)\n");
  }
  else if( verbose == 0 )
  {
    for( size_t i = 0; cookie && cookie[i]; ++i )
    {
      alarmclient_emitf("[%03ld]\n", cookie[i]);
    }
  }
  else
  {
    event = calloc(n, sizeof *event);
    for( size_t i = 0; i < n; ++i )
    {
      event[i] = alarmd_event_get(cookie[i]);
    }

    if( verbose == 1 )
    {
      time_t now = ticker_get_time();

      for( size_t i = 0; i < n; ++i )
      {
        char date[128];
        char secs[128];
        char ident[256];
        char stamp[512];

        time_t diff = now - event[i]->ALARMD_PRIVATE(trigger);

        alarmclient_date_format_short(date, sizeof date, &event[i]->ALARMD_PRIVATE(trigger));
        alarmclient_secs_format(secs, sizeof secs, diff);

        snprintf(ident, sizeof ident, "%s/%s/%s",
                 alarm_event_get_alarm_appid(event[i]),
                 alarm_event_get_title(event[i]),
                 alarm_event_get_message(event[i]));

        snprintf(stamp, sizeof stamp, "%s (T%s)", date, secs);

        alarmclient_emitf("[%03ld] %-36s %s\n", cookie[i], stamp, ident);
      }
    }
    else
    {
      for( size_t i = 0; i < n; ++i )
      {
        alarmclient_emitf("[%03ld]  ", cookie[i]);
        alarmclient_event_show(event[i]);
      }
    }
    for( size_t i = 0; i < n; ++i )
    {
      alarm_event_delete(event[i]);
    }
    free(event);
  }

  free(cookie);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_clear_events
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_clear_events(void)
{
  cookie_t *cookie = alarmd_event_query(0,0,0,0,0);

  if( !cookie || !*cookie )
  {
    alarmclient_emitf("(no alarms)\n");
  }
  else for( size_t i = 0; cookie[i]; ++i )
  {
    int rc = alarmd_event_del(cookie[i]);
    alarmclient_emitf("DEL %ld -> %s\n", cookie[i], rc ? "ERR" : "OK");
  }
  free(cookie);
}
/* ========================================================================= *
 * QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ
 * ========================================================================= */

static alarm_event_t  *the_event  = 0;
static alarm_action_t *the_action = 0;

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_flush_current_event
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_flush_current_event(void)
{
  alarm_event_delete(the_event);
  the_event  = 0;
  the_action = 0;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_get_current_event
 * ------------------------------------------------------------------------- */

static
alarm_event_t *
alarmclient_handle_get_current_event(void)
{
  if( the_event == 0 )
  {
    the_event = alarm_event_create();
    alarm_event_set_alarm_appid(the_event, ALARMCLIENT_APPID);
    alarm_event_set_title(the_event,   "[TITLE]");
    alarm_event_set_message(the_event, "[MESSAGE]");
  }
  return the_event;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_get_new_action
 * ------------------------------------------------------------------------- */

static
alarm_action_t *
alarmclient_handle_get_new_action(void)
{
  alarm_event_t  *eve = alarmclient_handle_get_current_event();
  the_action = alarm_event_add_actions(eve, 1);
  return the_action;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_get_current_action
 * ------------------------------------------------------------------------- */

static
alarm_action_t *
alarmclient_handle_get_current_action(void)
{
  if( the_action == 0 )
  {
    alarmclient_handle_get_new_action();
  }
  return the_action;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_new_event
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_new_event(char *args)
{
  cookie_t       cookie = 0;
  alarm_event_t *eve = alarmclient_handle_get_current_event();

  alarmclient_event_fill_fields(eve, args);

  if( 0 < eve->alarm_time && eve->alarm_time < 365 * 24 * 60 * 60 )
  {
    eve->alarm_time += ticker_get_time();
  }

  if( eve->ALARMD_PRIVATE(cookie) > 0 )
  {
    cookie = alarmd_event_update(eve);
  }
  else
  {
    cookie = alarmd_event_add(eve);
  }
  if( cookie > 0 )
  {
    alarmclient_emitf("ADD -> %ld\n", cookie);
  }
  else
  {
    alarmclient_emitf("ADD -> %s\n", "ERR");
  }

  alarmclient_handle_flush_current_event();
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_new_action
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_new_action(char *args)
{
  alarm_action_t *act = alarmclient_handle_get_new_action();
  alarmclient_action_fill_fields(act, args);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_dbus_args
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_dbus_args(char *args)
{
  alarm_action_t *act = alarmclient_handle_get_current_action();
  if( act == 0 )
  {
    log_error("you need to add action (-b) before setting dbus args\n");
    exit(EXIT_FAILURE);
  }
  alarmclient_action_add_dbus_arg(act, args);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_attr_args
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_attr_args(char *args)
{
  alarm_event_t *eve = alarmclient_handle_get_current_event();
  alarmclient_event_set_attr(eve, args);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_handle_add_trackable_event
 * ------------------------------------------------------------------------- */

static
void
alarmclient_handle_add_trackable_event(char *args, int show)
{
  /* This function adds alarm with:
   *
   * - calendar style Stop/Snooze/View 3-button dialog
   *
   * - separate callbacks for all button presses
   *
   * - dbus callback for QUEUED: this can be used
   *   by the client application to refresh trigger
   *   time shown for recurring/snoozed alarms
   *
   * - dbus callback for TRIGGERED: this is done
   *   right before sending the alarm to system
   *   ui for showing the dialog
   *
   * - dbus callback for DELETE: this can be used
   *   by the client to get notifications when
   *   an alarm has been removed for any reason
   */

  auto void dbus_action(alarm_action_t *act, const char *name);
  auto void button_action(alarm_action_t *act, const char *name);

  auto void dbus_action(alarm_action_t *act, const char *name)
  {
    const char *service   = ALARMCLIENT_SERVICE;
    const char *interface = ALARMCLIENT_INTERFACE;
    const char *object    = ALARMCLIENT_PATH;

    act->flags |= ALARM_ACTION_TYPE_DBUS;
    act->flags |= ALARM_ACTION_DBUS_ADD_COOKIE;

    alarm_action_set_dbus_name(act, name);
    alarm_action_set_dbus_interface(act, interface);
    alarm_action_set_dbus_path(act, object);
    alarm_action_set_dbus_service(act, service);
  }

  auto void button_action(alarm_action_t *act, const char *name)
  {
    act->flags |= ALARM_ACTION_WHEN_RESPONDED;
    alarm_action_set_label(act, name);
  }

  alarm_event_t  *eve = 0;
  alarm_action_t *act = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * basic event setup
   * - - - - - - - - - - - - - - - - - - - */

  eve = alarm_event_create();

  alarm_event_set_alarm_appid(eve, ALARMCLIENT_APPID);
  alarm_event_set_title(eve, "[TITLE]");
  alarm_event_set_message(eve, "[MESSAGE]");

  /* - - - - - - - - - - - - - - - - - - - *
   * add buttons
   * - - - - - - - - - - - - - - - - - - - */

  // STOP -> dbus method call clicked_stop(cookie)
  act = alarm_event_add_actions(eve, 1);
  button_action(act, "Stop");
  dbus_action(act, ALARMCLIENT_CLICK_STOP);

  // SNOOZE -> reschedule + dbus method call clicked_snooze(cookie)
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_TYPE_SNOOZE;
  button_action(act, "Snooze");
  dbus_action(act, ALARMCLIENT_CLICK_SNOOZE);

  // STOP -> dbus method call clicked_view(cookie)
  act = alarm_event_add_actions(eve, 1);
  button_action(act, "View");
  dbus_action(act, ALARMCLIENT_CLICK_VIEW);

  // DUMMY -> event_queued(cookie) when alarm is queued (or re-queued)
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_QUEUED;
  dbus_action(act, ALARMCLIENT_ALARM_ADD);

  // DUMMY -> event_triggered(cookie) when alarm is triggered (before dialog)
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_TRIGGERED;
  dbus_action(act, ALARMCLIENT_ALARM_TRG);

  // DUMMY -> event_deleted(cookie) when alarm is removed from queue
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_DELETED;
  dbus_action(act, ALARMCLIENT_ALARM_DEL);

  /* - - - - - - - - - - - - - - - - - - - *
   * parse user settings
   * - - - - - - - - - - - - - - - - - - - */

  alarmclient_event_fill_fields(eve, args);

  /* - - - - - - - - - - - - - - - - - - - *
   * setup absolute trigger time
   * - - - - - - - - - - - - - - - - - - - */

  time_t now = ticker_get_time();

  if( 0 < eve->alarm_time && eve->alarm_time < 365 * 24 * 60 * 60 )
  {
    eve->alarm_time += now;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * use really short custom snooze time
   * - - - - - - - - - - - - - - - - - - - */

  eve->snooze_secs = 5;

  /* - - - - - - - - - - - - - - - - - - - *
   * add/update event
   * - - - - - - - - - - - - - - - - - - - */

  cookie_t cookie = 0;

  if( eve->ALARMD_PRIVATE(cookie) > 0 )
  {
    if( (cookie = alarmd_event_update(eve)) > 0 )
    {
      alarmclient_emitf("UPDATE -> %ld\n", cookie);
    }
    else
    {
      alarmclient_emitf("UPDATE -> %s\n", "ERR");
    }
  }
  else
  {
    if( (cookie = alarmd_event_add(eve)) > 0 )
    {
      alarmclient_emitf("ADD -> %ld\n", cookie);
    }
    else
    {
      alarmclient_emitf("ADD -> %s\n", "ERR");
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * show if so requested
   * - - - - - - - - - - - - - - - - - - - */

  if( show )
  {
    alarmclient_event_show(eve);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * cleanup
   * - - - - - - - - - - - - - - - - - - - */

  alarm_event_delete(eve);
}

/* ========================================================================= *
 * ALARMCLIENT ENTRY POINT
 * ========================================================================= */

int
main(int argc, char **argv)
{
  const char opt_s[] = "hliLg:d:cr:w:W:b:D:e:n:xa:A:sS:tT:C:Z:zN:_:%X:";

  static const struct option opt_l[] =
  {
    {"help",       0, 0, 'h'},
    {"usage",      0, 0, 'h'},
    {"version",    0, 0, 'V'},
    {0,            0, 0,  0 }
  };

  int opt;
  int tracking = 0;

  /* - - - - - - - - - - - - - - - - - - - *
   * logging setup
   * - - - - - - - - - - - - - - - - - - - */
  log_set_level(LOG_WARNING);
  if( xscratchbox() )
  {
    log_set_level(LOG_DEBUG);
  }
  log_open("alarmclient", LOG_TO_STDERR, 0);

  /* - - - - - - - - - - - - - - - - - - - *
   * install signal handlers
   * - - - - - - - - - - - - - - - - - - - */
  alarmclient_sighnd_init();

  /* - - - - - - - - - - - - - - - - - - - *
   * check if we have clockd available
   * - - - - - - - - - - - - - - - - - - - */
  alarmclient_detect_clockd();

  /* - - - - - - - - - - - - - - - - - - - *
   * parse command line args
   * - - - - - - - - - - - - - - - - - - - */
  while( (opt = getopt_long(argc, argv, opt_s, opt_l, 0)) != -1 )
  {
    switch( opt )
    {
      // print usage info
    case 'h': alarmclient_print_usage(*argv); exit(0);
    case 'V': printf("%s\n", VERS); exit(0);

      // simple queue actions
    case 'l': alarmclient_handle_list_events(0); break;
    case 'i': alarmclient_handle_list_events(1); break;
    case 'L': alarmclient_handle_list_events(2); break;
    case 'g': alarmclient_handle_get_event(optarg); break;
    case 'd': alarmclient_handle_del_event(optarg); break;
    case 'c': alarmclient_handle_clear_events(); break;
    case 'r': alarmclient_handle_reply(optarg); break;
    case 'w': alarmclient_handle_sleep(optarg); break;
    case 'W': alarmclient_handle_until(optarg); break;
      // dynamic alarm construction
    case 'b': alarmclient_handle_new_action(optarg); break;
    case 'D': alarmclient_handle_dbus_args(optarg); break;
    case 'e': alarmclient_handle_attr_args(optarg); break;
    case 'n': alarmclient_handle_new_event(optarg); break;

      // trackable calendar like alarms
    case 'x':
      if( !tracking )
      {
        tracking = 1;
        if( alarmclient_tracker_init() == -1 )
        {
          exit(EXIT_FAILURE);
        }
      }
      break;
    case 'a': alarmclient_handle_add_trackable_event(optarg, 0); break;
    case 'A': alarmclient_handle_add_trackable_event(optarg, 1); break;

      // default snooze period
    case 's': alarmclient_handle_get_snooze(); break;
    case 'S': alarmclient_handle_set_snooze(optarg); break;

      // clockd timezone
    case 'Z': alarmclient_handle_set_timezone(optarg); break;
    case 'z': alarmclient_handle_show_timezone(); break;

      // clockd timebase
    case 'C': alarmclient_handle_add_time(optarg); break;
    case 'T': alarmclient_handle_set_time(optarg); break;
    case 't': alarmclient_handle_show_time(); break;
    case 'N': alarmclient_handle_autosync(optarg); break;

      // debugging known usecases
    case 'X': alarmclient_handle_special(optarg); break;

      // undocumented: internal debugging only
    case '_':
      if( alarmclient_string_to_boolean(optarg) )
      {
        alarmd_set_debug(1,0,1,0);
      }
      else
      {
        alarmd_set_debug(1,0,0,1);
      }
      break;

      // undocumented: placeholder for short term debug stuff
    case '%':
      break;

    default: /* '?' */
      alarmclient_emitf("Use '-h' for usage information\n");
      exit(EXIT_FAILURE);
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * enter tracking mainloop, if requested
   * - - - - - - - - - - - - - - - - - - - */
  if( tracking )
  {
    if( alarmclient_tracker_exec() == -1 )
    {
      exit(EXIT_FAILURE);
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * exit
   * - - - - - - - - - - - - - - - - - - - */
  return EXIT_SUCCESS;
}

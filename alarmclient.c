/* ========================================================================= *
 * File: alarmclient.c
 *
 * Copyright (C) 2008 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
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
 * Misc utils
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * alarmclient_append_string  --  overflow safe formatted append
 * ------------------------------------------------------------------------- */

static
void
alarmclient_append_string(char **ppos, char *end, char *fmt, ...)
__attribute__((format(printf,3,4)));

static
void
alarmclient_append_string(char **ppos, char *end, char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  vsnprintf(*ppos, end-*ppos, fmt, va);
  va_end(va);
  *ppos = strchr(*ppos,0);
}

/* ------------------------------------------------------------------------- *
 * alarmclient_break_seconds  --  seconds -> days, hours, mins and secs
 * ------------------------------------------------------------------------- */

static int
alarmclient_break_seconds(time_t secs, int *pd, int *ph, int *pm, int *ps)
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
 * alarmclient_format_seconds -- seconds -> "#d #h #m #s"
 * ------------------------------------------------------------------------- */

static
char *
alarmclient_format_seconds(char *buf, size_t size, time_t secs)
{
  static char tmp[256];

  if( buf == 0 ) buf = tmp, size = sizeof tmp;

  char *pos = buf;
  char *end = buf + size;

  int d = 0, h = 0, m = 0, s = 0;
  int n = alarmclient_break_seconds(secs, &d,&h,&m,&s);

  *pos = 0;

  auto const char *sep(void);
  auto const char *sep(void) {
    const char *r="";
    //if( pos>buf ) r=" ";
    if( n ) r=(n<0)?"-":"+", n = 0;
    return r;
  }
  if( d ) alarmclient_append_string(&pos,end,"%s%dd", sep(), d);
  if( h ) alarmclient_append_string(&pos,end,"%s%dh", sep(), h);
  if( m ) alarmclient_append_string(&pos,end,"%s%dm", sep(), m);
  if( s || (!d && !h && !m) )
  {
    alarmclient_append_string(&pos,end,"%s%ds", sep(), s);
  }

  return buf;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_parse_seconds  --  "1d2h3m4s" -> 93784
 * ------------------------------------------------------------------------- */

static
int
alarmclient_parse_seconds(const char *str)
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
 * wday_name  --  0 -> "Sun", ..., 6 -> "Sat"
 * ------------------------------------------------------------------------- */

static const char *wday_name(int wday)
{
  static const char * const wday_name_lut[7] =
  {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
  };

  if( 0 <= wday && wday <= 6 )
  {
    return wday_name_lut[wday];
  }
  return "???";
}

/* ------------------------------------------------------------------------- *
 * alarmclient_format_date_long
 * ------------------------------------------------------------------------- */

static
char *
alarmclient_format_date_long(char *buf, size_t size, time_t *t)
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
           wday_name(tm.tm_wday),
           tm.tm_zone,
           (tm.tm_isdst > 0) ? "Yes" : "No");

  return buf;
}

/* ------------------------------------------------------------------------- *
 * alarmclient_format_date_short
 * ------------------------------------------------------------------------- */

static
char *
alarmclient_format_date_short(char *buf, size_t size, time_t *t)
{
  static char tmp[128];

  struct tm tm;
  ticker_get_local_ex(*t, &tm);

  if( buf == 0 )
  {
    buf = tmp, size = sizeof tmp;
  }

  snprintf(buf, size, "%s %04d-%02d-%02d %02d:%02d:%02d",
           wday_name(tm.tm_wday),
           tm.tm_year + 1900,
           tm.tm_mon + 1,
           tm.tm_mday,
           tm.tm_hour,
           tm.tm_min,
           tm.tm_sec);

  return buf;
}

/* ------------------------------------------------------------------------- *
 * emitf  --  printf + fflush
 * ------------------------------------------------------------------------- */

static
void
emitf(const char *fmt, ...) __attribute((format(printf,1,2)));

static
void
emitf(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  vfprintf(stdout, fmt, va);
  va_end(va);
  fflush(stdout);
}

/* ------------------------------------------------------------------------- *
 * str2cookie  --  convert user input to cookie_t
 * ------------------------------------------------------------------------- */

static inline cookie_t str2cookie(const char *s)
{
  return strtol(s,0,0);
}

/* ------------------------------------------------------------------------- *
 * str2bool  --  convert user input to int 0/1
 * ------------------------------------------------------------------------- */

static int str2bool(const char *s)
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
 * escape  --  escape string for output as one line
 * ------------------------------------------------------------------------- */

static const char *escape(const char *txt)
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
 * serialize_to  --  serialize dbus arguments provided as strings
 * ------------------------------------------------------------------------- */

static
int
serialize_to(strbuf_t *self, char *text, ...)
{
  int err = 0;
  va_list va;
  va_start(va, text);

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

  for( ; text != 0; text = va_arg(va, char *) )
  {
    char *type = 0;
    char *data = strchr(text, ':');

    if( data == 0 )
    {
      v.i32 = strtol(text, &data, 0);
      if( data > text && *data == 0 )
      {
        strbuf_encode(self, tag_int32, &v.i32, tag_done);
        continue;
      }

      v.dbl = strtod(text, &data);
      if( data > text && *data == 0 )
      {
        strbuf_encode(self, tag_double, &v.dbl, tag_done);
        continue;
      }
      v.str = text;
      strbuf_encode(self, tag_string, &v.str, tag_done);
      continue;
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
      v.flg = str2bool(data);
      strbuf_encode(self, tag_bool, &v.flg, tag_done);
    }
    else
    {
      err = -1;
      goto cleanup;
    }
  }

  cleanup:

  va_end(va);

  return err;
}

/* ------------------------------------------------------------------------- *
 * serialize  --  accumulate serialized user provided dbus arguments
 * ------------------------------------------------------------------------- */

static void
serialize(char **ptxt, char *data)
{
  strbuf_t buf;

  strbuf_ctor_ex(&buf, *ptxt ?: "");
  serialize_to(&buf, data, NULL);
  free(*ptxt);
  *ptxt = strbuf_steal(&buf);
  strbuf_dtor(&buf);
}

/* ========================================================================= *
 * DBUS SERVER
 * ========================================================================= */

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

  emitf("CALLBACK: %s(%d)\n", dbus_message_get_member(msg), (int)cookie);
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
    time_t diff = ticker_get_time() - eve->trigger;

    snprintf(tmp, sizeof tmp, " Trigger time: T%s",
             alarmclient_format_seconds(0,0,diff));
  }

  emitf("CALLBACK: %s(%d)%s\n", dbus_message_get_member(msg), (int)cookie, tmp);

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

  emitf("ALARM QUEUE: active=%d, desktop=%d, actdead=%d, no-boot=%d\n",
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

#if ENABLE_LOGGING
// QUARANTINE   const char         *typestr   = dbusif_get_msgtype_name(type);
// QUARANTINE   log_debug("\n");
// QUARANTINE   log_debug("----------------------------------------------------------------\n");
// QUARANTINE   log_debug("@ %s: %s(%s, %s, %s)\n", __FUNCTION__, typestr, object, iface, member);
#endif

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

#if ENABLE_LOGGING
// QUARANTINE   log_debug("----------------------------------------------------------------\n");
// QUARANTINE   log_debug("\n");
#endif

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
      log_critical("can't claim service name: %s: %s\n",
                   err.name, err.message);
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
 * FAKE SYSTEM UI MAINLOOP
 * ========================================================================= */

static GMainLoop *alarmclient_mainloop_hnd = 0;

/* ------------------------------------------------------------------------- *
 * alarmclient_mainloop_stop
 * ------------------------------------------------------------------------- */

static
int
alarmclient_mainloop_stop(void)
{
  if( alarmclient_mainloop_hnd == 0 )
  {
    exit(EXIT_FAILURE);
  }
  g_main_loop_quit(alarmclient_mainloop_hnd);
  return 0;
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

// QUARANTINE   cleanup:

  return error;
}

/* ========================================================================= *
 * FAKE SUSTEM UI SIGNAL HANDLER
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
    if( alarmclient_mainloop_stop() )
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
 * alarmclient_sighnd_handler  --  handle trapped signals
 * ------------------------------------------------------------------------- */

static
void
alarmclient_sighnd_handler(int sig)
{
  log_error("Got signal [%d] %s\n", sig, strsignal(sig));

  switch( sig )
  {
  case SIGINT:
  case SIGHUP:
  case SIGTERM:
  case SIGQUIT:
    signal(sig, alarmclient_sighnd_handler);
    alarmclient_sighnd_terminate();
    break;
  }
}

/* ------------------------------------------------------------------------- *
 * alarmclient_sighnd_init  --  setup signal trapping
 * ------------------------------------------------------------------------- */

static
void
alarmclient_sighnd_init(void)
{
  static const int sig[] =
  {
    SIGHUP,
    SIGINT,
    SIGQUIT,
    SIGTERM,
    -1
  };

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
   * sanity checks
   * - - - - - - - - - - - - - - - - - - - */

  if( sizeof(dbus_int32_t) != sizeof(cookie_t) )
  {
    log_critical("sizeof(dbus_int32_t) != sizeof(cookie_t)\n");
    goto cleanup;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * install signal handlers
   * - - - - - - - - - - - - - - - - - - - */

  alarmclient_sighnd_init();

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
 * Bitmasks to/from strings
 * ========================================================================= */

typedef struct
{
  const char *name;
  unsigned    mask;
} bitsymbol_t;

static unsigned lookup_mask(const bitsymbol_t *v, size_t n, const char *s)
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

static const char *lookup_name(const bitsymbol_t *v, size_t n, unsigned m)
{
  auto void addchr(int c);
  auto void addstr(const char *s);

  static char buf[1024];
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

static const bitsymbol_t lut_alarmeventflags[] =
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

static unsigned
alarmeventflags_from_text(const char *s)
{
  return lookup_mask(lut_alarmeventflags, numof(lut_alarmeventflags), s);
}

static const char *
alarmeventflags_to_text(unsigned m)
{
  return lookup_name(lut_alarmeventflags, numof(lut_alarmeventflags), m);
}

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

static unsigned
alarmactionflags_from_text(const char *s)
{
  return lookup_mask(lut_alarmactionflags, numof(lut_alarmactionflags), s);
}

static const char *
alarmactionflags_to_text(unsigned m)
{
  return lookup_name(lut_alarmactionflags, numof(lut_alarmactionflags), m);
}

/* ========================================================================= *
 * structure input data parser
 * ========================================================================= */

typedef struct
{
  const char *name;
  size_t      offs;
  void      (*set)(void *, const char *);
} filler_t;

// set member callbacks
static void
set_time(void *aptr, const char *s)
{
  *(time_t *)aptr = strtol(s,0,0);
}
static void
set_year(void *aptr, const char *s)
{
  int y = strtol(s,0,0);
  *(int *)aptr = (y >= 1900) ? (y - 1900) : y;
}
static void
set_text(void *aptr, const char *s)
{
  //*(const char **)aptr = s;
  *(char **)aptr = strdup(s ?: "");
}
static void
set_int(void *aptr, const char *s)
{
  *(int *)aptr = strtol(s,0,0);
}
static void
set_uns(void *aptr, const char *s)
{
  *(unsigned *)aptr = strtoul(s,0,0);
}

static void
set_eflag(void *aptr, const char *s)
{
  *(int32_t *)aptr = alarmeventflags_from_text(s);
}
static void
set_aflag(void *aptr, const char *s)
{
  *(int32_t *)aptr = alarmactionflags_from_text(s);
}

// lookup table for alarm_event_t
static const filler_t event_filler[] =
{
#define X(t,m) { #m, offsetof(alarm_event_t, m), set_##t },
  X(uns,    cookie)
  X(time,   trigger)

  X(text,   title)
  X(text,   message)
  X(text,   sound)
  X(text,   icon)
  X(eflag,  flags)
  X(text,   alarm_appid)

  X(time,   alarm_time)
  X(year,   alarm_tm.tm_year)
  X(int,    alarm_tm.tm_mon)
  X(int,    alarm_tm.tm_mday)
  X(int,    alarm_tm.tm_hour)
  X(int,    alarm_tm.tm_min)
  X(int,    alarm_tm.tm_sec)
  X(int,    alarm_tm.tm_wday)
  X(int,    alarm_tm.tm_yday)
  X(int,    alarm_tm.tm_isdst)
  X(text,   alarm_tz)

  X(time,   recur_secs)
  X(int,    recur_count)

  X(time,   snooze_secs)
  X(int,    snooze_total)

  X(int,    action_cnt)
#undef X
};

// lookup table for alarm_action_t
static const filler_t action_filler[] =
{
#define X(t,m) { #m, offsetof(alarm_action_t, m), set_##t },
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

static
void
fill_item(const filler_t *lut, int cnt, void *aptr, char *filler)
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
          emitf("Unknown event member '%s'\n", filler);
          exit(1);
        }
        //if( !strcmp(lut[last].name, filler) )
        if( match( lut[last].name, filler) )
        {
          emitf("SET '%s' '%s'\n", lut[last].name, arg);
          lut[last].set(base + lut[last].offs, arg);
          break;
        }
      }
    }
    else
    {
      if( ++last >= cnt )
      {
        emitf("member %d out of bounds (%d)\n", last, cnt);
        exit(1);
      }
      emitf("SET '%s' '%s'\n", lut[last].name, filler);
      lut[last].set(base + lut[last].offs, filler);
    }
  }
}

static void fill_event(alarm_event_t *eve, char *text)
{
  fill_item(event_filler, numof(event_filler), eve, text);
}

static void fill_action(alarm_action_t *act, char *text)
{
  fill_item(action_filler, numof(action_filler), act, text);
}

/* ========================================================================= *
 * print event data in human readable form
 * ========================================================================= */

static
void
print_event(alarm_event_t *event)
{
  emitf("alarm_event_t {\n");

#define PS(v) emitf("\t%-17s '%s'\n", #v, escape(event->v))
#define PN(v) emitf("\t%-17s %ld\n", #v, (long)(event->v))
#define PX(v) emitf("\t%-17s %s\n", #v, alarmeventflags_to_text(event->v))

  PN(cookie);
  //PN(trigger);

  {
    time_t diff = ticker_get_time() - event->trigger;
    emitf("\t%-17s %d -> %s (T%s)\n",
          "trigger",
          (int)event->trigger,
          alarmclient_format_date_long(0, 0, &event->trigger),
          alarmclient_format_seconds(0, 0, diff));
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

  for( size_t i = 0; i < event->action_cnt; ++i )
  {
    alarm_action_t *act = &event->action_tab[i];

    emitf("\n\tACTION %d:\n", (int)i);

#define PS(v) emitf("\t%-17s '%s'\n", #v, escape(act->v))
#define PX(v) emitf("\t%-17s %s\n", #v, alarmactionflags_to_text(act->v))

    //emitf("\n");
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

    emitf("\n\tRECURRENCY %d:\n", (int)i);

    emitf("\t%-17s", "mask_min");
    switch( rec->mask_min )
    {
    case ALARM_RECUR_MIN_DONTCARE:
      emitf(" ALARM_RECUR_MIN_DONTCARE\n");
      break;
    case ALARM_RECUR_MIN_ALL:
      emitf(" ALARM_RECUR_MIN_ALL\n");
      break;
    default:
      for( int i = 0; i < 60; ++i )
      {
        if( rec->mask_min & (1ull << i) )
        {
          emitf(" %d", i);
        }
      }
      emitf("\n");
      break;
    }

    emitf("\t%-17s", "mask_hour");
    switch( rec->mask_hour )
    {
    case ALARM_RECUR_HOUR_DONTCARE:
      emitf(" ALARM_RECUR_HOUR_DONTCARE\n");
      break;
    case ALARM_RECUR_HOUR_ALL:
      emitf(" ALARM_RECUR_HOUR_ALL\n");
      break;
    default:
      for( int i = 0; i < 24; ++i )
      {
        if( rec->mask_hour & (1 << i) )
        {
          emitf(" %d", i);
        }
      }
      emitf("\n");
      break;
    }

    emitf("\t%-17s", "mask_mday");
    switch( rec->mask_mday )
    {
    case ALARM_RECUR_MDAY_DONTCARE:
      emitf(" ALARM_RECUR_MDAY_DONTCARE\n");
      break;
    case ALARM_RECUR_MDAY_ALL:
      emitf(" ALARM_RECUR_MDAY_ALL\n");
      break;
    default:
      for( int i = 1; i <= 31; ++i )
      {
        if( rec->mask_mday & (1 << i) )
        {
          emitf(" %d", i);
        }
      }
      if( rec->mask_mday & ALARM_RECUR_MDAY_EOM )
      {
        emitf(" ALARM_RECUR_MDAY_EOM");
      }
      emitf("\n");
      break;
    }

    emitf("\t%-17s", "mask_wday");
    switch( rec->mask_wday )
    {
    case ALARM_RECUR_WDAY_DONTCARE:
      emitf(" ALARM_RECUR_WDAY_DONTCARE\n");
      break;
    case ALARM_RECUR_WDAY_ALL:
      emitf(" ALARM_RECUR_WDAY_ALL\n");
      break;
    default:
      for( int i = 0; i < 7; ++i )
      {
        if( rec->mask_wday & (1 << i) )
        {
          emitf(" %s", wday_name(i));
        }
      }
      emitf("\n");
      break;
    }

    emitf("\t%-17s", "mask_mon");
    switch( rec->mask_mon )
    {
    case ALARM_RECUR_MON_DONTCARE:
      emitf(" ALARM_RECUR_MON_DONTCARE\n");
      break;
    case ALARM_RECUR_MON_ALL:
      emitf(" ALARM_RECUR_MON_ALL\n");
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
          emitf(" %s", mon[i]);
        }
      }
      emitf("\n");
      break;
    }

    emitf("\t%-17s", "special");

    switch( rec->special )
    {
    case ALARM_RECUR_SPECIAL_NONE:
      emitf(" ALARM_RECUR_SPECIAL_NONE\n");
      break;
    case ALARM_RECUR_SPECIAL_BIWEEKLY:
      emitf(" ALARM_RECUR_SPECIAL_BIWEEKLY\n");
      break;
    case ALARM_RECUR_SPECIAL_MONTHLY:
      emitf(" ALARM_RECUR_SPECIAL_MONTHLY\n");
      break;
    case ALARM_RECUR_SPECIAL_YEARLY:
      emitf(" ALARM_RECUR_SPECIAL_YEARLY\n");
      break;
    default:
      emitf(" ALARM_RECUR_SPECIAL_UNKNOWN(%d)\n",
            rec->special);
      break;
    }
  }
  if( event->attr_cnt ) emitf("\n");

  for( size_t i = 0; i < event->attr_cnt; ++i )
  {
    alarm_attr_t *att = event->attr_tab[i];

    emitf("\tATTR '%s' = ", att->attr_name);
    switch( att->attr_type )
    {
    case ALARM_ATTR_NULL:  emitf("NULL\n"); break;
    case ALARM_ATTR_INT:   emitf("INT    %d\n", att->attr_data.ival); break;
    case ALARM_ATTR_TIME:  emitf("TIME   %s", ctime(&att->attr_data.tval)); break;
    case ALARM_ATTR_STRING:emitf("STRING '%s'\n", att->attr_data.sval);
    }
  }

  emitf("}\n");
}

static void handle_del_event(char *s)
{
  cookie_t cookie = str2cookie(s);
  emitf("DEL %ld -> %d\n", cookie, alarmd_event_del(cookie));
}

static void handle_get_event(char *s)
{
  cookie_t cookie = str2cookie(s);
  alarm_event_t *event = alarmd_event_get(cookie);

  emitf("GET %ld -> %p\n", cookie, event);

  if( event != 0 )
  {
    print_event(event);
    alarm_event_delete(event);
  }
}

static void handle_list_events(int verbose)
{
  cookie_t       *cookie = 0;
  alarm_event_t **event  = 0;

  cookie = alarmd_event_query(0,0,0,0,0);

  size_t n;

  for( n = 0; cookie && cookie[n]; ++n ) {}

  if( n == 0 )
  {
    emitf("(no alarms)\n");
  }
  else if( verbose == 0 )
  {
    for( size_t i = 0; cookie && cookie[i]; ++i )
    {
      emitf("[%03ld]\n", cookie[i]);
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
        //struct tm tm;
        char date[128];
        char secs[128];
        char ident[256];

        time_t diff = now - event[i]->trigger;

        //ticker_get_local_ex(event[i]->trigger, &tm);
        alarmclient_format_date_short(date, sizeof date, &event[i]->trigger);
        alarmclient_format_seconds(secs, sizeof secs, diff);

        snprintf(ident, sizeof ident, "%.16s/%.16s/%s",
                 alarm_event_get_alarm_appid(event[i]),
                 alarm_event_get_title(event[i]),
                 alarm_event_get_message(event[i]));

        emitf("[%03ld]  ", cookie[i]);
        emitf("%-32s  ", ident);
        emitf("%s (T%s)", date, secs);
        emitf("\n");
      }
    }
    else
    {
      for( size_t i = 0; i < n; ++i )
      {
        emitf("[%03ld]  ", cookie[i]);
        print_event(event[i]);
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

static void handle_clear_events(void)
{
  cookie_t *cookie = alarmd_event_query(0,0,0,0,0);

  if( !cookie || !*cookie )
  {
    emitf("(no alarms)\n");
  }
  else for( size_t i = 0; cookie[i]; ++i )
  {
    emitf("DEL %ld -> %d\n", cookie[i], alarmd_event_del(cookie[i]));
  }
  free(cookie);
}

static void handle_get_snooze(void)
{
  emitf("SNOOZE == %u\n", alarmd_get_default_snooze());
}

static void handle_set_snooze(const char *s)
{
  unsigned snooze = alarmclient_parse_seconds(optarg);
  emitf("SNOOZE %u -> %d\n", snooze, alarmd_set_default_snooze(snooze));
}

/* ========================================================================= *
 * Dynamic event construction
 * ========================================================================= */

static alarm_event_t  *the_event  = 0;
static alarm_action_t *the_action = 0;
static void fin_event(void)
{
  alarm_event_delete(the_event);
  the_event  = 0;
  the_action = 0;
}

static alarm_event_t *cur_event(void)
{
  if( the_event == 0 )
  {
    the_event = alarm_event_create();

    // must have
    alarm_event_set_alarm_appid(the_event, ALARMCLIENT_APPID);

    // should have
    alarm_event_set_title(the_event, "[EMPTY TITLE]");
    alarm_event_set_message(the_event, "[EMPTY MESSAGE]");

    // optional
#if 0
    alarm_event_set_sound(the_event, "[SOUND FILE]");
    alarm_event_set_icon(the_event, "[ICON FILE]");
    alarm_event_set_alarm_tz(the_event, "UTC");
#endif
  }

  return the_event;
}

static alarm_action_t *new_action(void)
{
  alarm_event_t *eve = cur_event();
  the_action = alarm_event_add_actions(eve, 1);
  return the_action;
}

static alarm_action_t *cur_action(void)
{
  return the_action;
}

static void handle_new_action(char *s)
{
  alarm_action_t *act = new_action();
  fill_action(act, s);
}

static void handle_dbus_args(char *s)
{
  alarm_action_t *act = cur_action();

  if( act == 0 )
  {
    fprintf(stderr, "you need to add action (-b) before setting dbus args\n");
    exit(1);
  }

  serialize(&act->dbus_args, s);
}

static void handle_attr_args(char *s)
{
  alarm_event_t *eve = cur_event();

  printf("@%s(%s)\n", __FUNCTION__, s);

  char *k = s;
  char *t = 0;
  char *v = 0;

  if( (t = strchr(k, '=')) != 0 )
  {
    *t++ = 0;

    printf("@%s(%s,%s)\n", __FUNCTION__, k, t);

    if( (v = strchr(t, ':')) != 0 )
    {
      *v++ = 0;
    }
    else
    {
      v = t, t = 0;
    }

    printf("@%s(%s,%s,%s)\n", __FUNCTION__, k, t, v);

    if( t == 0 || !strcmp(t, "string") )
    {
      alarm_event_set_attr_string(eve, k, v);
    }
    else if( !strcmp(t, "int") )
    {
      alarm_event_set_attr_int(eve, k, strtol(v, 0, 0));
    }
    else if( !strcmp(t, "time_t") || !strcmp(t, "time") )
    {
      alarm_event_set_attr_int(eve, k, strtol(v, 0, 0));
    }
    else
    {
      log_error("unsupported attribute type '%s'\n", t);
    }
  }
  else
  {
    log_error("attribute format: <key>=<type>:<value>\n");
  }
  print_event(eve);
}

static void handle_new_event(char *s)
{
  cookie_t       cookie = 0;
  alarm_event_t *eve = cur_event();

  fill_event(eve, s);

  if( 0 < eve->alarm_time && eve->alarm_time < 365 * 24 * 60 * 60 )
  {
    eve->alarm_time += ticker_get_time();
  }

  if( eve->cookie > 0 )
  {
    cookie = alarmd_event_update(eve);
  }
  else
  {
    cookie = alarmd_event_add(eve);
  }
  emitf("ADD -> %ld\n", cookie);

  fin_event();
}

static void handle_add_event(char *s, int show)
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

  alarm_event_t *eve = 0;
  alarm_action_t *act;

  /* - - - - - - - - - - - - - - - - - - - *
   * basic event setup
   * - - - - - - - - - - - - - - - - - - - */

  eve = cur_event();

  alarm_event_set_alarm_appid(eve, ALARMCLIENT_APPID);
  alarm_event_set_title(eve, "[EMPTY TITLE]");
  alarm_event_set_message(eve, NULL);

  eve->snooze_secs = 5;

  /* - - - - - - - - - - - - - - - - - - - *
   * add buttons
   * - - - - - - - - - - - - - - - - - - - */

  const char *service   = ALARMCLIENT_SERVICE;
  const char *interface = ALARMCLIENT_INTERFACE;
  const char *object    = ALARMCLIENT_PATH;

  // STOP -> dbus method call clicked_stop(cookie)
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_DBUS;
  act->flags |= ALARM_ACTION_DBUS_ADD_COOKIE;
  alarm_action_set_label(act, "Stop");
  alarm_action_set_dbus_name(act, ALARMCLIENT_CLICK_STOP);
  alarm_action_set_dbus_interface(act, interface);
  alarm_action_set_dbus_path(act, object);
  alarm_action_set_dbus_service(act, service);

  // SNOOZE -> reschedule + dbus method call clicked_snooze(cookie)
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_SNOOZE;
  act->flags |= ALARM_ACTION_TYPE_DBUS;
  act->flags |= ALARM_ACTION_DBUS_ADD_COOKIE;
  alarm_action_set_label(act, "Snooze");
  alarm_action_set_dbus_name(act, ALARMCLIENT_CLICK_SNOOZE);
  alarm_action_set_dbus_interface(act, interface);
  alarm_action_set_dbus_path(act, object);
  alarm_action_set_dbus_service(act, service);

  // STOP -> dbus method call clicked_view(cookie)
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_RESPONDED;
  act->flags |= ALARM_ACTION_TYPE_DBUS;
  act->flags |= ALARM_ACTION_DBUS_ADD_COOKIE;
  alarm_action_set_label(act, "View");
  alarm_action_set_dbus_name(act, ALARMCLIENT_CLICK_VIEW);
  alarm_action_set_dbus_interface(act, interface);
  alarm_action_set_dbus_path(act, object);
  alarm_action_set_dbus_service(act, service);

  // DUMMY -> event_queued(cookie) when alarm is queued (or re-queued)
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_QUEUED;
  act->flags |= ALARM_ACTION_TYPE_DBUS;
  act->flags |= ALARM_ACTION_DBUS_ADD_COOKIE;
  alarm_action_set_dbus_name(act, ALARMCLIENT_ALARM_ADD);
  alarm_action_set_dbus_interface(act, interface);
  alarm_action_set_dbus_path(act, object);
  alarm_action_set_dbus_service(act, service);

  // DUMMY -> event_triggered(cookie) when alarm is triggered (before dialog)
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_TRIGGERED;
  act->flags |= ALARM_ACTION_TYPE_DBUS;
  act->flags |= ALARM_ACTION_DBUS_ADD_COOKIE;
  alarm_action_set_dbus_name(act, ALARMCLIENT_ALARM_TRG);
  alarm_action_set_dbus_interface(act, interface);
  alarm_action_set_dbus_path(act, object);
  alarm_action_set_dbus_service(act, service);

  // DUMMY -> event_deleted(cookie) when alarm is removed from queue
  act = alarm_event_add_actions(eve, 1);
  act->flags |= ALARM_ACTION_WHEN_DELETED;
  act->flags |= ALARM_ACTION_TYPE_DBUS;
  act->flags |= ALARM_ACTION_DBUS_ADD_COOKIE;
  alarm_action_set_dbus_name(act, ALARMCLIENT_ALARM_DEL);
  alarm_action_set_dbus_interface(act, interface);
  alarm_action_set_dbus_path(act, object);
  alarm_action_set_dbus_service(act, service);

  /* - - - - - - - - - - - - - - - - - - - *
   * parse user settings
   * - - - - - - - - - - - - - - - - - - - */

  fill_event(eve, s);

  time_t now = ticker_get_time();

  if( 0 < eve->alarm_time && eve->alarm_time < 365 * 24 * 60 * 60 )
  {
    eve->alarm_time += now;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * set message if none from user
   * - - - - - - - - - - - - - - - - - - - */

  if( !*alarm_event_get_message(eve) )
  {
    char       *msg = 0;
    time_t      zen = eve->alarm_time;
    const char *tz  = alarm_event_get_alarm_tz(eve);
    struct tm   tm2 = eve->alarm_tm;
    struct tm   tm1;
    char        ts1[256];
    char        ts2[256];
    char        tz_def[128];

    *ts1 = *ts2 = *tz_def = 0;

    ticker_get_timezone(tz_def, sizeof tz_def);

    if( zen <= 0 )
    {
      zen = ticker_mktime(&tm2, tz ?: tz_def);
    }
    else
    {
      ticker_get_remote(zen, tz ?: tz_def, &tm2);
    }

    ticker_get_local_ex(now, &tm1);
    ticker_format_time(&tm1, "%a, %d %b %Y %H:%M:%S %z", ts1, sizeof ts1);

    if( zen < 0 )
    {
      asprintf(&msg, "Alarm added at:\n%s", ts1);
    }
    else
    {
      ticker_format_time_ex(&tm2, tz ?: tz_def, "%a, %d %b %Y %H:%M:%S %z", ts2, sizeof ts2);
      asprintf(&msg, "Alarm added at:\n%s\n"
               "Alarm triggers at:\n%s", ts1, ts2);
    }
    alarm_event_set_message(eve, msg);
    free(msg);
  }

// QUARANTINE   alarm_event_set_attr_int(eve, "number_value", 42);
// QUARANTINE   alarm_event_set_attr_time(eve, "time_value", time(0));
// QUARANTINE   alarm_event_set_attr_string(eve, "string_value", "Fluubio meni kalaan");

  cookie_t cookie = 0;

  if( eve->cookie > 0 )
  {
    cookie = alarmd_event_update(eve);
  }
  else
  {
    cookie = alarmd_event_add(eve);
  }

  emitf("ADD -> %ld\n", cookie);

  if( show )
  {
    print_event(eve);
  }

  //alarm_event_delete(eve);
  fin_event();
}

static
void
usage(const char *progname)
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
         "  -l               --  list cookies\n"
         "  -i               --  list cookies + appid + trigger time\n"
         "  -L               --  list cookies + full alarm content\n"
         "  -g[cookie]       --  get event\n"
         "  -d[cookie]       --  delete event\n"
         "  -c               --  clear all events\n"
         "\n"
         "  -b[action_data]  --  add button/action\n"
         "  -D[dbus_data]    --  add dbus data to action\n"
         "  -e[attr_data]    --  add alarm event attributes\n"
         "  -n[event_data]   --  add event with actions\n"
         "\n"
         "  -x               --  start dbus service & track events\n"
         "  -a[event_data]   --  add event that is trackable via -x\n"
         "  -A[event_data]   --  add event + show added event\n"
         "\n"
         "  -s               --  get default snooze\n"
         "  -S[seconds]      --  set default snooze\n"
         "\n"
         "  -t               --  get system vs clockd time diff\n"
         "  -T[seconds]      --  set system vs clockd time diff\n"
         "  -C[seconds]      --  advance clockd time\n"
         "  -z               --  get current clockd timezone\n"
         "  -Z[timezone]     --  set current clockd timezone\n"
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
}

static
int
running_in_scratchbox(void)
{
  return access("/targets/links/scratchbox.config", F_OK) == 0;
}

static void detect_clockd(void)
{
  int have_clockd = 0;

  DBusConnection *con = 0;
  DBusError       err = DBUS_ERROR_INIT;

  if( !(con = dbus_bus_get_private(DBUS_BUS_SYSTEM, &err)) )
  {
    log_error("%s: %s: %s\n", "dbus_bus_get_private", err.name, err.message);
  }
  else
  {
    if( dbusif_check_name_owner(con, CLOCKD_SERVICE) == 0 )
    {
      have_clockd |= 1;
    }

    dbus_connection_close(con);
    dbus_connection_unref(con);
  }

  dbus_error_free(&err);

  if( !have_clockd )
  {
    fprintf(stderr, "CLOCKD not detected!\n"
            "Using custom timehandling instead of libtime!\n");
  }

  ticker_use_libtime(have_clockd);
}

static void
handle_show_time(void)
{
  char tz[64];

  ticker_get_timezone(tz, sizeof tz);

  time_t now = ticker_get_time();
  time_t off = ticker_get_offset();

  printf("TIMEZONE = %s\n", tz);
  printf("CURRTIME = %s\n", alarmclient_format_date_long(0, 0, &now));
  printf("TIMEOFFS = %+d\n", (int)off);
}

static void
handle_set_time(const char *str)
{
  int Y,M,D, h,m,s;
  time_t t = ticker_get_time();
  struct tm tm;

  Y = M = D = h = m = s = -1;

  if( sscanf(str, "%d-%d-%d %d:%d:%d", &Y,&M,&D, &h,&m,&s) >= 3 )
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
    printf("DATE & TIME: %s\n", alarmclient_format_date_long(0, 0, &t));
    ticker_set_time(t);
  }
  else if (sscanf(str, "%d:%d:%d", &h,&m,&s) >= 2 )
  {
    ticker_get_local_ex(t, &tm);
    if( h >= 0 ) tm.tm_hour = h;
    if( m >= 0 ) tm.tm_min  = m;
    if( s >= 0 ) tm.tm_sec  = s;

    t = ticker_mktime(&tm, 0);
    printf("TIME ONLY: %s\n", alarmclient_format_date_long(0, 0, &t));
    ticker_set_time(t);
  }
  else
  {
    t = alarmclient_parse_seconds(str);
    printf("OFFSET: %+d\n", (int)t);

    ticker_set_offset(t);
  }
  handle_show_time();
}

static void
handle_add_time(const char *str)
{
  ticker_set_time(ticker_get_time() + alarmclient_parse_seconds(optarg));
  handle_show_time();
}

static void
handle_show_timezone(void)
{
  char tz[64];
  ticker_get_timezone(tz, sizeof tz);
  printf("TIMEZONE = '%s'\n", tz);
}

static void
handle_set_timezone(const char *str)
{
  ticker_set_timezone(str);
  handle_show_timezone();
}

static cookie_t
add_event(alarm_event_t *eve, int verbose)
{
  cookie_t        aid = 0;

  // send -> alarmd
  aid = alarmd_event_add(eve);
  printf("cookie = %d\n", (int)aid);

  if( (aid > 0) && (eve = alarmd_event_get(aid)) )
  {
    time_t diff = ticker_get_time() - eve->trigger;
    printf("Time left: %s\n",
           alarmclient_format_seconds(0, 0, diff));
    alarm_event_delete(eve);
  }

  return aid;
}
static int alarm_count = 0;

static alarm_event_t *alarm_with_title(const char *title,
                                       const char *message)
{
  char *t = 0;
  char *m = 0;
  alarm_event_t  *eve = 0;

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

static alarm_event_t *
new_clock_oneshot(int h, int m, int wd, int verbose)
{
  alarm_event_t  *eve = 0;
  alarm_action_t *act = 0;

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
  eve = alarm_with_title("clock", "oneshot");
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

  eve->alarm_tm.tm_hour = h;
  eve->alarm_tm.tm_min  = m;
  eve->alarm_tm.tm_wday = wd;

  return eve;
}

static alarm_event_t *
new_clock_recurring(int h, int m, int wd, int verbose)
{
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

  alarm_event_t  *eve = 0;
  alarm_action_t *act = 0;
  alarm_recur_t  *rec = 0;

  // event
  eve = alarm_with_title("clock", "recurring");

  eve->flags |= ALARM_EVENT_BACK_RESCHEDULE;
  eve->flags |= ALARM_EVENT_ACTDEAD;
  eve->flags |= ALARM_EVENT_SHOW_ICON;

// QUARANTINE   // FIXME: remove ALARM_EVENT_DISABLE_DELAYED after testing
// QUARANTINE   eve->flags |= ALARM_EVENT_DISABLE_DELAYED;

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

  return eve;
}

static alarm_event_t *
new_calendar_alarm(int h, int m, int wd, int verbose)
{
#if 0
  title             'bbb'
  icon              '/usr/share/icons/hicolor/128x128/hildon/clock_calendar_alarm.png'
  flags             0x20000
  alarm_appid       'Calendar'
  alarm_time        1230853500

  ACTION 0:
  flags             TYPE_SNOOZE|WHEN_RESPONDED
  label             'Snooze'

  ACTION 1:
  flags             TYPE_DBUS|WHEN_RESPONDED
  label             'View'
  dbus_interface    'com.nokia.calendar'
  dbus_service      'com.nokia.calendar'
  dbus_path         '/com/nokia/calendar'
  dbus_name         'launch_view'
  dbus_args         'L4;l0;s1\;'

  ACTION 2:
  flags             TYPE_DBUS|WHEN_RESPONDED
  label             'Stop'
  dbus_interface    'com.nokia.calendar'
  dbus_service      'com.nokia.calendar'
  dbus_path         '/com/nokia/calendar'
  dbus_name         'calendar_alarm_responded'
  dbus_args         's1\;L2;'

  ATTR 'target_time' = TIME   Thu Jan  1 23:45:00 2009
  ATTR 'location' = STRING 'bcccc'
#endif

  alarm_event_t  *eve = 0;
  alarm_action_t *act = 0;
  time_t          now = ticker_get_time();
  time_t          trg = now;
  struct tm       tm;

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

  // event
  eve = alarm_with_title("calendar", "alarm");
  alarm_event_set_icon(eve, "/usr/share/icons/hicolor/128x128/hildon/clock_calendar_alarm.png");

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

  return eve;
}

static cookie_t
setup_clock_oneshot(int h, int m, int wd, int verbose, char *args)
{
  alarm_event_t  *eve = 0;
  cookie_t        aid = 0;

  printf("----( oneshot alarm %02d:%02d %s )----\n",
         h,m, (wd < 0) ? "anyday" : wday_name(wd));

  eve = new_clock_oneshot(h,m,wd,verbose);

  fill_event(eve, args);

  aid = add_event(eve, verbose);
  alarm_event_delete(eve);

  return aid;
}

static cookie_t
setup_clock_recurring(int h, int m, int wd, int verbose, char *args)
{
  alarm_event_t  *eve = 0;
  cookie_t        aid = 0;

  printf("----( recurring alarm %02d:%02d every %s )----\n",
         h,m, (wd < 0) ? "day" : wday_name(wd));

  eve = new_clock_recurring(h, m, wd, verbose);
  fill_event(eve, args);
  aid = add_event(eve, verbose);
  alarm_event_delete(eve);

  return aid;
}

static cookie_t
setup_calendar_alarm(int h, int m, int wd, int verbose, char *args)
{
  alarm_event_t  *eve = 0;
  cookie_t        aid = 0;

  printf("----( calendar alarm %02d:%02d %s )----\n",
         h,m, (wd < 0) ? "anyday" : wday_name(wd));

  eve = new_calendar_alarm(h,m,wd,verbose);

  fill_event(eve, args);

  aid = add_event(eve, verbose);
  alarm_event_delete(eve);

  return aid;
}

static cookie_t handle_special(const char *str)
{
  char *tag = strdup(str);
  char *trg = strchr(tag, '@');
  char *arg = strchr(tag, ',');

  cookie_t aid = 0;

  int h = 10;
  int m =  0;

  if( trg != 0 ) *trg++ = 0;
  if( arg != 0 ) *arg++ = 0;

  if( trg != 0 )
  {
    sscanf(trg, "%d:%d", &h, &m);
  }

  if( !strcmp(tag, "clock") )
  {
    setup_clock_oneshot(h,m, -1, 0, arg);
  }
  else if( !strcmp(tag, "clockr") )
  {
    setup_clock_recurring(h,m,-1, 0, arg);
  }
  else if( !strcmp(tag, "calendar") )
  {
    setup_calendar_alarm(h,m,-1, 0, arg);
  }
  else
  {
    log_error("unknown preset '%s'\n", tag);
  }
  free(tag);

  return aid;
}

int
main(int argc, char **argv)
{

  int opt;
  int tracking = 0;

  auto void init_tracking(void);
  auto void exec_tracking(void);

  auto void init_tracking(void)
  {
    if( !tracking )
    {
      tracking = 1;
      if( alarmclient_tracker_init() == -1 )
      {
        exit(1);
      }
    }
  }

  auto void exec_tracking(void)
  {
    if( tracking )
    {
      if( alarmclient_tracker_exec() == -1 )
      {
        exit(1);
      }
    }
  }

  log_set_level(LOG_WARNING);
  if( running_in_scratchbox() )
  {
    log_set_level(LOG_DEBUG);
  }
  log_open("alarmclient", LOG_TO_STDERR, 0);

  detect_clockd();

  while( (opt = getopt(argc, argv, "hliLg:d:cb:D:e:n:xa:A:sS:tT:C:Z:z_:%X:")) != -1 )
  {
    switch( opt )
    {
      // documented options -> generic usage
    case 'h': usage(*argv); exit(0);

    case 'l': handle_list_events(0); break;
    case 'i': handle_list_events(1); break;
    case 'L': handle_list_events(2); break;
    case 'g': handle_get_event(optarg); break;
    case 'd': handle_del_event(optarg); break;
    case 'c': handle_clear_events(); break;

    case 'b': handle_new_action(optarg); break;
    case 'D': handle_dbus_args(optarg); break;
    case 'e': handle_attr_args(optarg); break;
    case 'n': handle_new_event(optarg); break;

    case 'x': init_tracking(); break;
    case 'a': handle_add_event(optarg, 0); break;
    case 'A': handle_add_event(optarg, 1); break;

    case 's': handle_get_snooze(); break;
    case 'S': handle_set_snooze(optarg); break;

    case 'Z': handle_set_timezone(optarg); break;
    case 'z': handle_show_timezone(); break;

    case 'C': handle_add_time(optarg); break;

    case 'T': handle_set_time(optarg); break;
    case 't': handle_show_time(); break;

      // undocumented swithces -> internal debugging only

    case 'X': handle_special(optarg); break;

    case '_':
      if( str2bool(optarg) )
      {
        alarmd_set_debug(1,0,1,0);
      }
      else
      {
        alarmd_set_debug(1,0,0,1);
      }
      break;

    case '%':
#if 0
      {
        int verbose = 0;

        handle_set_timezone("EET");
        handle_set_time("2008-01-03 06:05:00");

        setup_clock_recurring(7, 0, 2, verbose, 0);
        setup_clock_recurring(7, 0, 3, verbose, 0);
        setup_clock_recurring(7, 0, 5, verbose, 0);
        setup_clock_recurring(7, 0, 6, verbose, 0);
        setup_clock_recurring(7, 0,-1, verbose, 0);

        setup_clock_oneshot(7, 0, 2, verbose, 0);
        setup_clock_oneshot(7, 0, 3, verbose, 0);
        setup_clock_oneshot(7, 0, 5, verbose, 0);
        setup_clock_oneshot(7, 0, 6, verbose, 0);
        setup_clock_oneshot(7, 0,-1, verbose, 0);
      }
#endif

#if 01
      {
        int verbose = 0;
        handle_set_timezone("EET");
        handle_set_time("2008-01-03 11:59:40");
        setup_clock_oneshot(12, 0,-1, verbose, 0);
        //setup_clock_recurring(12, 0,-1, verbose, 0);
      }
#endif

#if 0
      {
        int verbose = 0;
        int cookie = 0;
        alarm_event_t *eve = 0;

        handle_set_timezone("EET");
        handle_set_time("11:00:00");
        cookie = setup_clock_oneshot(12, 0, -1, verbose, 0);

        sleep(10);

        handle_set_time("13:00:00");

        sleep(10);

        if( (eve = alarmd_event_get(cookie)) == 0 )
        {
          printf("FAILURE: alarm got deleted!\n");
        }
        else
        {
          if( eve->flags & ALARM_EVENT_DISABLED )
          {
            printf("SUCCESS: alarm got disabled!\n");
          }
          else
          {
            printf("FAILURE: alarm is still active!\n");
          }
          alarm_event_delete(eve);
          alarmd_event_del(cookie);
        }
      }
#endif

#if 0
      {
        int verbose = 0;
        int cookie = 0;
        time_t now = ticker_get_time();

        alarm_event_t *eve = alarm_event_create();
        alarm_event_set_alarm_appid(eve, ALARMCLIENT_APPID);

        // action
        alarm_action_t *act = alarm_event_add_actions(eve, 1);
        act->flags |= ALARM_ACTION_WHEN_TRIGGERED;
        act->flags |= ALARM_ACTION_TYPE_DISABLE;

        // trigger
        eve->alarm_time = now + 5;

        if( (cookie = alarmd_event_add(eve)) )
        {
          printf("cookie = %d\n", (int)cookie);
          alarm_event_delete(eve);
          if( (eve = alarmd_event_get(cookie)) )
          {
            int d=0,h=0,m=0,s=0,t=0;
            t = time_diff(ticker_get_time(),eve->trigger, &d,&h,&m,&s);
            printf("Time left: %dd %dh %dm %ds (%d)\n", d,h,m,s, t);
            printf("\n");
            if( verbose ) print_event(eve);
            alarm_event_delete(eve);
          }

          sleep(10);

          if( (eve = alarmd_event_get(cookie)) == 0 )
          {
            printf("FAILURE: alarm got deleted!\n");
          }
          else
          {
            if( eve->flags & ALARM_EVENT_DISABLED )
            {
              printf("SUCCESS: alarm got disabled!\n");
            }
            else
            {
              printf("FAILURE: alarm is still active!\n");
            }
            alarm_event_delete(eve);
            alarmd_event_del(cookie);
          }
        }
      }
#endif
      break;

    default: /* '?' */
      emitf("Use '-h' for usage information\n");
      exit(EXIT_FAILURE);
    }
  }

  exec_tracking();

  return 0;
}

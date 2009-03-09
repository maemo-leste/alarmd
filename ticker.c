#include "ticker.h"
#include "logging.h"

#if USE_LIBTIME
# include <clockd/libtime.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USE_CONFIG 0

/* ========================================================================= *
 * SIMULATED LIBTIME SETTING USING SYMLINKS IN /tmp
 * ========================================================================= */

#define CONF_PFIX   "/tmp/ticker."

#define CONF_TIME   CONF_PFIX"time"
#define CONF_FORMAT CONF_PFIX"format"
#define CONF_ZONE   CONF_PFIX"timezone"

static const char *
conf_get_str(const char *path, char *buff, size_t size, const char *def)
{
#if !USE_CONFIG
  *buff = 0;
#else
  memset(buff, 0, size);
  readlink(path, buff, size-1);
#endif
  if( *buff == 0 && def != 0 )
  {
    strncat(buff, def, size-1);
  }

// QUARANTINE   log_debug("conf_get_str(%s) -> %s\n", path, buff);

  return buff;
}
static int
conf_set_str(const char *path, const char *data)
{
  log_debug("conf_set_str(%s) -> %s\n", path, data);
#if !USE_CONFIG
  return 0;
#else
  remove(path);
  return symlink(data, path);
#endif
}

static int
conf_get_int(const char *path)
{
  char buff[256];
  return strtol(conf_get_str(path, buff, sizeof buff, "0"), 0, 0);
}

static int
conf_set_int(const char *path, int val)
{
  char buff[256];
  snprintf(buff, sizeof buff, "%d", val);
  return conf_set_str(path, buff);
}

/* ========================================================================= *
 * CUSTOM TZ ACCESS FUNCTIONS
 * ========================================================================= */

static const char tz_unset[] = "(unset)";

static const char *tz_get(void)
{
  return getenv("TZ") ?: tz_unset;
}

static int tz_cmp(const char *tz1, const char *tz2)
{
  return strcmp(tz1 ?: tz_unset, tz2 ?: tz_unset);
}

static void tz_set(const char *tz)
{
  if( tz != 0 && strcmp(tz, tz_unset) )
  {
    setenv("TZ", tz, 1);
  }
  else
  {
    unsetenv("TZ");
  }
  tzset();
}

static const char *tz_name(void)
{
#ifdef _GNU_SOURCE
  time_t t = 0;
  struct tm tm;
  localtime_r(&t, &tm);
  return tm.tm_zone;
#else
  tzset();
  return *tzname;
#endif
}

typedef struct timezone_t timezone_t;

struct timezone_t
{
  char *tz;
};

static timezone_t *
timezone_switch(const char *tz)
{
  timezone_t *self = calloc(1, sizeof *self);
  self->tz = strdup(tz_get());
  tz_set(tz && *tz ? tz : self->tz);
  return self;
}

static void
timezone_restore(timezone_t *self)
{
  if( self != 0 )
  {
    tz_set(self->tz);
    free(self->tz);
    free(self);
  }
}

static inline int
copy(char *d, size_t n, const char *s)
{
  int r = snprintf(d, n, "%s", s);
  return (r < n) ? r : -1;
}

static void
custom_send_notification(time_t tick)
{
#if !USE_CONFIG
  // nop
#else
  log_debug("@ %s()\n", __FUNCTION__);

  DBusError       err = DBUS_ERROR_INIT;
  DBusConnection *bus = NULL;

  if( (bus = dbus_bus_get(DBUS_BUS_SYSTEM, &err)) == 0 )
  {
    log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
  }
  else
  {
    dbus_int32_t t = tick;
    dbusif_signal_send(bus,
                       CLOCKD_PATH,
                       CLOCKD_INTERFACE,
                       CLOCKD_TIME_CHANGED,
                       DBUS_TYPE_INT32, &t,
                       DBUS_TYPE_INVALID);

    dbus_connection_flush(bus);
    dbus_connection_unref(bus);
  }
  dbus_error_free(&err);
#endif
}

/* ========================================================================= *
 * SIMULATED LIBTIME FUNCTIONALITY
 * ========================================================================= */

static
time_t
custom_get_time(void)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  return time(0) + conf_get_int(CONF_TIME);
}

static
int
custom_set_time(time_t tick)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  conf_set_int(CONF_TIME, tick - time(0));

  custom_send_notification(custom_get_time());
  return 0;
}

static
time_t
custom_mktime(struct tm *tm, const char *tz)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  timezone_t *save = timezone_switch(tz);
  time_t rc = mktime(tm);
  timezone_restore(save);
  return rc;
}

static
int
custom_get_timezone(char *s, size_t max)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);
  char buff[256];
  conf_get_str(CONF_ZONE, buff, sizeof buff, tz_get());
  tz_set(buff);
  return copy(s, max, tz_get());
}

static
int
custom_set_timezone(const char *tz)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  tz_set(tz);
  conf_set_str(CONF_ZONE, tz_get());

  int rc = tz_cmp(tz_get(), tz) ? -1 : 0;
  custom_send_notification(0);
  return rc;
}

static
int
custom_get_tzname(char *s, size_t max)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);
  return copy(s, max, tz_name());
}

static
int
custom_get_utc_ex(time_t tick, struct tm *tm)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  return gmtime_r(&tick, tm) ? 0 : -1;
}

static
int
custom_get_local_ex(time_t tick, struct tm *tm)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  return localtime_r(&tick, tm) ? 0 : -1;
}

static
int
custom_get_utc(struct tm *tm)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  return custom_get_utc_ex(custom_get_time(), tm);
}

static
int
custom_get_local(struct tm *tm)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  return custom_get_local_ex(custom_get_time(), tm);
}

static
int
custom_get_remote(time_t tick, const char *tz, struct tm *tm)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  timezone_t *save = timezone_switch(tz);
  int rc = custom_get_local_ex(tick, tm);
  timezone_restore(save);
  return rc;
}

static
int
custom_get_time_format(char *s, size_t max)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);
  char buff[256];

  return copy(s, max, conf_get_str(CONF_FORMAT, buff, sizeof buff, "%R"));
}

static
int
custom_set_time_format(const char *fmt)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);
  int rc = conf_set_str(CONF_FORMAT, fmt);
  custom_send_notification(0);
  return rc;
}

static
int
custom_format_time(const struct tm *tm, const char *fmt, char *s, size_t max)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);
  return strftime(s, max, fmt, tm);
}

static
int
custom_get_utc_offset(const char *tz)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  timezone_t *save = timezone_switch(tz);
  int rc = timezone;
  timezone_restore(save);
  return rc;
}

static
int
custom_get_dst_usage(time_t tick, const char *tz)
{
  int rc = -1;
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  struct tm tm;

  timezone_t *save = timezone_switch(tz);

  if( custom_get_local_ex(tick, &tm) == 0 )
  {
    rc = tm.tm_isdst;
  }
  timezone_restore(save);

  return rc;
}

static
double
custom_diff(time_t t1, time_t t2)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);
  return difftime(t1, t2);
}

static
int
custom_get_time_diff(time_t tick, const char *tz1, const char *tz2)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  abort(); // FIXME: unsupported custom_get_time_diff()
  return -1;
}

static
int
custom_set_autosync(int  enable)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  abort(); // FIXME: unsupported custom_activate_net_time()
  return -1;
}

static
int
custom_get_autosync(void)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  abort(); // FIXME: unsupported custom_activate_net_time()
  return -1;
}

static
int
custom_is_operator_time_accessible(void)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  abort(); // FIXME: unsupported custom_activate_net_time()
  return -1;
}

static
int
custom_is_net_time_changed(time_t *tick, char *s, size_t max)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  abort(); // FIXME: unsupported custom_activate_net_time()
  return -1;
}

static
int
custom_activate_net_time(void)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);

  abort(); // FIXME: unsupported custom_activate_net_time()
  return -1;
}

static
int
custom_get_synced(void)
{
// QUARANTINE   log_debug("@ %s()\n", __FUNCTION__);
  char buff[256];

  custom_get_timezone(buff, sizeof buff);

  return 0;
}

/* ========================================================================= *
 * DRIVER
 * ========================================================================= */

typedef struct ticker_driver_t
{
#define TICKER(type,name,args) type (*cb_##name)args;
#include "ticker.inc"
} ticker_driver_t;

static const ticker_driver_t drivers[] =
{
  {
#define TICKER(type,name,args) .cb_##name = custom_##name,
#include "ticker.inc"
  },
#if USE_LIBTIME
  {
#define TICKER(type,name,args) .cb_##name = time_##name,
#include "ticker.inc"
  },
#endif
};

static const ticker_driver_t *ticker_driver = &drivers[0];

int ticker_get_synced(void)
{
  return ticker_driver->cb_get_synced();
}

time_t ticker_get_time(void)
{
  return ticker_driver->cb_get_time();
}

int ticker_set_time(time_t tick)
{
  return ticker_driver->cb_set_time(tick);
}

time_t ticker_mktime(struct tm *tm, const char *tz)
{
  return ticker_driver->cb_mktime(tm, tz);
}

int ticker_get_timezone(char *s, size_t max)
{
  return ticker_driver->cb_get_timezone(s, max);
}

#ifdef DEAD_CODE
int ticker_get_tzname(char *s, size_t max)
{
  return ticker_driver->cb_get_tzname(s, max);
}
#endif

int ticker_set_timezone(const char *tz)
{
  return ticker_driver->cb_set_timezone(tz);
}

#ifdef DEAD_CODE
int ticker_get_utc(struct tm *tm)
{
  return ticker_driver->cb_get_utc(tm);
}
#endif

#ifdef DEAD_CODE
int ticker_get_utc_ex(time_t tick, struct tm *tm)
{
  return ticker_driver->cb_get_utc_ex(tick, tm);
}
#endif

int ticker_get_local(struct tm *tm)
{
  return ticker_driver->cb_get_local(tm);
}

int ticker_get_local_ex(time_t tick, struct tm *tm)
{
  return ticker_driver->cb_get_local_ex(tick, tm);
}

int ticker_get_remote(time_t tick, const char *tz, struct tm *tm)
{
  return ticker_driver->cb_get_remote(tick, tz, tm);
}

#ifdef DEAD_CODE
int ticker_get_time_format(char *s, size_t max)
{
  return ticker_driver->cb_get_time_format(s, max);
}
#endif

#ifdef DEAD_CODE
int ticker_set_time_format(const char *fmt)
{
  return ticker_driver->cb_set_time_format(fmt);
}
#endif

int ticker_format_time(const struct tm *tm, const char *fmt, char *s, size_t max)
{
  return ticker_driver->cb_format_time(tm, fmt, s, max);
}

#ifdef DEAD_CODE
int ticker_get_utc_offset(const char *tz)
{
  return ticker_driver->cb_get_utc_offset(tz);
}
#endif

#ifdef DEAD_CODE
int ticker_get_dst_usage(time_t tick, const char *tz)
{
  return ticker_driver->cb_get_dst_usage(tick, tz);
}
#endif

#ifdef DEAD_CODE
double ticker_diff(time_t t1, time_t t2)
{
  return ticker_driver->cb_diff(t1, t2);
}
#endif

#ifdef DEAD_CODE
int ticker_get_time_diff(time_t tick, const char *tz1, const char *tz2)
{
  return ticker_driver->cb_get_time_diff(tick, tz1, tz2);
}
#endif

#ifdef DEAD_CODE
int ticker_set_autosync(int enable)
{
  return ticker_driver->cb_set_autosync(enable);
}
#endif

#ifdef DEAD_CODE
int ticker_get_autosync(void)
{
  return ticker_driver->cb_get_autosync();
}
#endif

#ifdef DEAD_CODE
int ticker_is_net_time_changed(time_t *tick, char *s, size_t max)
{
  return ticker_driver->cb_is_net_time_changed(tick, s, max);
}
#endif

#ifdef DEAD_CODE
int ticker_activate_net_time(void)
{
  return ticker_driver->cb_activate_net_time();
}
#endif

#ifdef DEAD_CODE
int ticker_is_operator_time_accessible(void)
{
  return ticker_driver->cb_is_operator_time_accessible();
}
#endif

/* ========================================================================= *
 * NON-LIBTIME FUNCTIONS
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * ticker_use_libtime  --  select libtime / simulated functionality
 * ------------------------------------------------------------------------- */

void
ticker_use_libtime(int enable)
{
  log_debug("%s -> %s\n", __FUNCTION__, enable ? "YES" : "NO");
#if USE_LIBTIME
  ticker_driver = &drivers[(enable != 0)];
#endif
}

/* ------------------------------------------------------------------------- *
 * ticker_tm_is_same  --  check equality of all fields in two struct tm obects
 * ------------------------------------------------------------------------- */

int
ticker_tm_is_same(const struct tm *tm1, const struct tm *tm2)
{
#define CHECK_MEMBER(v) if( tm1->v != tm2->v ) return 0;
  CHECK_MEMBER(tm_sec)
  CHECK_MEMBER(tm_min)
  CHECK_MEMBER(tm_hour)
  CHECK_MEMBER(tm_mday)
  CHECK_MEMBER(tm_mon)
  CHECK_MEMBER(tm_year)
  CHECK_MEMBER(tm_wday)
  CHECK_MEMBER(tm_yday)
  CHECK_MEMBER(tm_isdst)
#undef CHECK_MEMBER

  return 1;
}

/* ------------------------------------------------------------------------- *
 * ticker_get_offset  --  get "system time" adjustment
 * ------------------------------------------------------------------------- */

time_t ticker_get_offset(void)
{
  return ticker_get_time() - time(0);
}

/* ------------------------------------------------------------------------- *
 * ticker_set_offset  --  adjust "system time"
 * ------------------------------------------------------------------------- */

int ticker_set_offset(time_t offs)
{
  return ticker_set_time(time(0) + offs);
}

/* ------------------------------------------------------------------------- *
 * ticker_get_monotonic  -- get monotonic time value
 * ------------------------------------------------------------------------- */

time_t ticker_get_monotonic(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec;
}

/* ------------------------------------------------------------------------- *
 * ticker_get_days_in_month  --  get number of days for a month in struct tm
 * ------------------------------------------------------------------------- */

static inline int is_leap_year(int y)
{
  // use macro from time.h
  return __isleap(y);

  //return (y % 4 == 0) && ((y % 100 != 0) || (y % 400 == 0));
}

int
ticker_get_days_in_month(const struct tm *src)
{
  static const int lut[12] =
  {
    31, // January
    28, // February
    31, // March
    30, // April
    31, // May
    30, // June
    31, // July
    31, // August
    30, // September
    31, // October
    30, // November
    31, // December
  };

  if( src->tm_mon != 1 )
  {
    return lut[src->tm_mon];
  }

  return 28 + is_leap_year(src->tm_year + 1900);
}

/* ------------------------------------------------------------------------- *
 * ticker_has_time  --  struct tm defines time of day
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
int
ticker_has_time(const struct tm *self)
{
#define X(v) if( self->v < 0 ) return 0;
  X(tm_sec)
  X(tm_min)
  X(tm_hour)
#undef X
  return 1;
}
#endif

/* ------------------------------------------------------------------------- *
 * ticker_has_date   --  struct tm defines date
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
int
ticker_has_date(const struct tm *self)
{
#define X(v) if( self->v < 0 ) return 0;
  X(tm_mday)
  X(tm_mon)
  X(tm_year)
#undef X
  return 1;
}
#endif

/* ------------------------------------------------------------------------- *
 * ticker_break_tm  --  time_t -> struct tm in given timezone
 * ------------------------------------------------------------------------- */

struct tm *
ticker_break_tm(time_t t, struct tm *tm, const char *tz)
{
  ticker_get_remote(t, tz, tm);
  return tm;

// QUARANTINE   setenv("TZ", tz, 1);
// QUARANTINE   tzset();
// QUARANTINE   return localtime_r(&t, tm);
}

/* ------------------------------------------------------------------------- *
 * ticker_build_tm  --  struct tm + timezone -> time_t
 * ------------------------------------------------------------------------- */

time_t
ticker_build_tm(struct tm *tm, const char *tz)
{
  return ticker_mktime(tm, tz);

// QUARANTINE   setenv("TZ", tz, 1);
// QUARANTINE   tzset();
// QUARANTINE
// QUARANTINE #if 0
// QUARANTINE   time_t t = mktime(tm);
// QUARANTINE   localtime_r(&t, &tm);
// QUARANTINE   return t;
// QUARANTINE #else
// QUARANTINE   tm->tm_isdst = -1;
// QUARANTINE   return mktime(tm);
// QUARANTINE #endif
}

/* ------------------------------------------------------------------------- *
 * ticker_show_tm  --  output struct tm to log
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
ticker_show_tm(const struct tm *tm)
{
#if ENABLE_LOGGING >= 3
  static const char * const wday_lut[7] =
  {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
  };

  static const char * const mon_lut[12] =
  {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
  };

  char year[8] = "????";
  char mday[8] = "??";
  char hour[8] = "??";
  char min[8]  = "??";
  char sec[8]  = "??";
  char yday[8] = "???";

  const char *mon  = "???";
  const char *wday = "???";
  const char *dst  = "?";

  if( tm->tm_isdst >= 0 )
  {
    dst = tm->tm_isdst ? "Y" : "N";
  }
  if( tm->tm_year >= 0 )
  {
    snprintf(year, sizeof year, "%04d", tm->tm_year + 1900);
  }
  if( 0 <= tm->tm_wday && tm->tm_wday < 7 )
  {
    wday = wday_lut[tm->tm_wday];
  }
  if( 0 <= tm->tm_mon && tm->tm_mon < 12 )
  {
    mon = mon_lut[tm->tm_mon];
  }
  if( tm->tm_mday >= 0 )
  {
    snprintf(mday, sizeof mday, "%02d", tm->tm_mday);
  }
  if( tm->tm_hour >= 0 )
  {
    snprintf(hour, sizeof hour, "%02d", tm->tm_hour);
  }
  if( tm->tm_min >= 0 )
  {
    snprintf(min, sizeof min, "%02d", tm->tm_min);
  }
  if( tm->tm_sec >= 0 )
  {
    snprintf(sec, sizeof sec, "%02d", tm->tm_sec);
  }
  if( tm->tm_yday >= 0 )
  {
    snprintf(yday, sizeof yday, "%03d", tm->tm_yday);
  }

  log_debug("%s %s-%s-%s %s:%s:%s (%s/%s)\n",
            wday,
            year,
            mon,
            mday,
            hour,
            min,
            sec,
            yday,
            dst);
#endif
}
#endif

/* ------------------------------------------------------------------------- *
 * ticker_diff_tm  --  output diff of two struct tm objects
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
ticker_diff_tm(const struct tm *dst, const struct tm *src)
{
#define X(v) if( dst->v != src->v ) printf("%48s%-8s: %d -> %d\n", "", #v, src->v, dst->v);
  X(tm_sec)
  X(tm_min)
  X(tm_hour)
  X(tm_mday)
  X(tm_mon)
  X(tm_year)
  X(tm_wday)
  X(tm_yday)
  X(tm_isdst)
#undef X
}
#endif

/* ------------------------------------------------------------------------- *
 * ticker_format_time_ex  --  do strftime in specified timezone
 * ------------------------------------------------------------------------- */

int ticker_format_time_ex(const struct tm *tm, const char *tz, const char *fmt, char *s, size_t max)
{
  timezone_t *save = timezone_switch(tz);
  int rc = strftime(s, max, fmt, tm);
  timezone_restore(save);
  return rc;
}

/* ------------------------------------------------------------------------- *
 * ticker_tm_all_fields_are_zero
 * ------------------------------------------------------------------------- */

static
int
ticker_tm_all_fields_are_zero(const struct tm *tm)
{
#define CHECK_MEMBER(v) if( tm->v != 0 ) return 0;
  //CHECK_MEMBER(tm_sec)
  CHECK_MEMBER(tm_min)
  CHECK_MEMBER(tm_hour)

  CHECK_MEMBER(tm_mday)
  CHECK_MEMBER(tm_mon)
  CHECK_MEMBER(tm_year)

  CHECK_MEMBER(tm_wday)
  //CHECK_MEMBER(tm_yday)
  //CHECK_MEMBER(tm_isdst)
#undef CHECK_MEMBER

  return 1;
}

/* ------------------------------------------------------------------------- *
 * ticker_tm_all_fields_are_negative
 * ------------------------------------------------------------------------- */

static
int
ticker_tm_all_fields_are_negative(const struct tm *tm)
{
#define CHECK_MEMBER(v) if( tm->v >= 0 ) return 0;
  //CHECK_MEMBER(tm_sec)
  CHECK_MEMBER(tm_min)
  CHECK_MEMBER(tm_hour)

  CHECK_MEMBER(tm_mday)
  CHECK_MEMBER(tm_mon)
  CHECK_MEMBER(tm_year)

  CHECK_MEMBER(tm_wday)
  //CHECK_MEMBER(tm_yday)
  //CHECK_MEMBER(tm_isdst)
#undef CHECK_MEMBER

  return 1;
}

/* ------------------------------------------------------------------------- *
 * ticker_tm_is_uninitialized
 * ------------------------------------------------------------------------- */

int
ticker_tm_is_uninitialized(const struct tm *tm)
{
  return (ticker_tm_all_fields_are_zero(tm) ||
          ticker_tm_all_fields_are_negative(tm));
}

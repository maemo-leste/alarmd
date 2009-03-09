/* ========================================================================= *
 * File: hwrtc.c
 *
 * Copyright (C) 2008 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 29-Aug-2008 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#include "hwrtc.h"

#include "logging.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <linux/rtc.h>

static const char hwrtc_path[] = "/dev/rtc0";

static
void
rtc_from_tm(struct rtc_time *rtc, const struct tm *tm)
{
  memset(rtc, -1, sizeof *rtc);

#define X(v) rtc->v = tm->v;
  X(tm_sec)
  X(tm_min)
  X(tm_hour)
  X(tm_mday)
  X(tm_mon)
  X(tm_year)
#undef X
}

#ifdef DEAD_CODE
static
void
rtc_to_tm(const struct rtc_time *rtc, struct tm *tm)
{
  memset(tm, -1, sizeof *tm);

#define X(v) tm->v = rtc->v;
  X(tm_sec)
  X(tm_min)
  X(tm_hour)
  X(tm_mday)
  X(tm_mon)
  X(tm_year)
#undef X
}
#endif

time_t
hwrtc_mktime(struct tm *tm)
{
  time_t  res = -1;
  char   *old = 0;

  if( (old = getenv("TZ")) != 0 )
  {
    old = strdup(old);
  }

  setenv("TZ","UTC",1);
  tzset();

  res = mktime(tm);

  if( old != 0 )
  {
    setenv("TZ", old, 1);
  }
  else
  {
    unsetenv("TZ");
  }
  tzset();

  free(old);

  return res;
}

void
hwrtc_quit(void)
{
  // nothing to do
}

int
hwrtc_init(void)
{
  /* check if rtc device node is available and
   * readable, continue normally even if it is
   * not */

  int error = -1;

  if( access(hwrtc_path, R_OK) != 0 )
  {
    log_warning("%s: read access -> %s\n", hwrtc_path, strerror(errno));
  }
  error = 0;

  return error;
}

#ifdef DEAD_CODE
time_t
hwrtc_get_time(struct tm *utc)
{
  int res = -1;
  int fd  = -1;

  struct rtc_time rtc;
  struct tm       tm;

  memset(&rtc, 0, sizeof rtc);
  memset(&tm,  0, sizeof tm);

  if( (fd = open(hwrtc_path, O_RDONLY)) == -1 )
  {
    log_warning("%s: open -> %s\n", hwrtc_path, strerror(errno));
    goto cleanup;
  }

  if( ioctl(fd, RTC_RD_TIME, &rtc) == -1 )
  {
    log_warning("%s: RTC_RD_TIME -> %s\n", hwrtc_path, strerror(errno));
    goto cleanup;
  }

  rtc_to_tm(&rtc, &tm);
  if( (res = hwrtc_mktime(&tm)) == -1 )
  {
    log_warning("%s: utc mktime -> %s\n", hwrtc_path, strerror(errno));
  }

  cleanup:

  if( utc != 0 ) *utc = tm;

  if( fd != -1 ) close(fd);

  return res;
}
#endif

#ifdef DEAD_CODE
time_t
hwrtc_get_alarm(struct tm *utc, int *enabled)
{
  time_t res = -1;
  int    fd  = -1;

  struct rtc_wkalrm wup;
  struct tm         tm;

  memset(&wup, 0, sizeof wup);
  memset(&tm,  0, sizeof tm);

  if( (fd = open(hwrtc_path, O_RDONLY)) == -1 )
  {
    log_warning("%s: open -> %s\n", hwrtc_path, strerror(errno));
    goto cleanup;
  }

  if( ioctl(fd, fd, RTC_WKALM_RD, &wup) == -1 )
  {
    log_warning("%s: RTC_WKALM_RD -> %s\n", hwrtc_path, strerror(errno));
    goto cleanup;
  }

  rtc_to_tm(&wup.time, &tm);

  if( (res = hwrtc_mktime(&tm)) == -1 )
  {
    log_warning("%s: utc mktime -> %s\n", hwrtc_path, strerror(errno));
  }

  cleanup:

  if( enabled != 0 )
  {
    *enabled = wup.enabled;
  }
  if( utc != 0 )
  {
    *utc = tm;
  }

  if( fd != -1 ) close(fd);

  return res;
}
#endif

// QUARANTINE static void show(const char *label, struct rtc_time *rtc)
// QUARANTINE {
// QUARANTINE   log_info("%s\t%d-%d-%d, %02d:%02d:%02d\n", label,
// QUARANTINE            rtc->tm_year, rtc->tm_mon, rtc->tm_mday,
// QUARANTINE            rtc->tm_hour, rtc->tm_min, rtc->tm_sec);
// QUARANTINE }

int
hwrtc_set_alarm(const struct tm *utc, int enabled)
{
  int err = -1;
  int fd  = -1;

  struct rtc_wkalrm wup;

  memset(&wup, 0, sizeof wup);

  log_debug("hwrtc: alarm at: %04d-%02d-%02d %02d:%02d:%02d\n",
            utc->tm_year + 1900,
            utc->tm_mon  + 1,
            utc->tm_mday,
            utc->tm_hour,
            utc->tm_min,
            utc->tm_sec);

  if( (fd = open(hwrtc_path, O_RDONLY)) == -1 )
  {
    log_error("%s: open -> %s\n", hwrtc_path, strerror(errno));
    goto cleanup;
  }

// QUARANTINE   struct rtc_time rtc;
// QUARANTINE   memset(&rtc, 0, sizeof rtc);
// QUARANTINE   if( ioctl(fd, RTC_RD_TIME, &rtc) == -1 )
// QUARANTINE   {
// QUARANTINE     log_error("RTC_RD_TIME: %s\n", strerror(errno));
// QUARANTINE   }
// QUARANTINE   else
// QUARANTINE   {
// QUARANTINE     show("Current", &rtc);
// QUARANTINE   }

  rtc_from_tm(&wup.time, utc);
  wup.enabled = (enabled != 0);

// QUARANTINE   show("Wakeup", &wup.time);
// QUARANTINE   log_info("Enabled\t%d\n", wup.enabled);
// QUARANTINE   log_info("Pending\t%d\n", wup.pending);

  if( ioctl(fd, RTC_WKALM_SET, &wup) == -1 )
  {
    log_error("%s: RTC_WKALM_SET -> %s\n", hwrtc_path, strerror(errno));
    goto cleanup;
  }

// QUARANTINE   if( ioctl(fd, RTC_WKALM_RD, &wup) == 0 )
// QUARANTINE   {
// QUARANTINE     show("Wakeup", &wup.time);
// QUARANTINE     log_info("Enabled\t%d\n", wup.enabled);
// QUARANTINE     log_info("Pending\t%d\n", wup.pending);
// QUARANTINE   }

  err = 0;

  cleanup:

  if( fd != -1 ) close(fd);

  return err;
}

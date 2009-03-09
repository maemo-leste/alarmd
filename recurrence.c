/* ========================================================================= *
 * File: recurrence.c
 *
 * Copyright (C) 2008 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 13-Jun-2008 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#include "libalarm.h"
#include "ticker.h"

#include <stdlib.h>

/* ========================================================================= *
 * alarm_recur_t  --  methods
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * alarm_recur_ctor
 * ------------------------------------------------------------------------- */

void
alarm_recur_ctor(alarm_recur_t *self)
{
  self->mask_min  = ALARM_RECUR_MIN_DONTCARE;
  self->mask_hour = ALARM_RECUR_HOUR_DONTCARE;
  self->mask_mday = ALARM_RECUR_MDAY_DONTCARE;
  self->mask_wday = ALARM_RECUR_WDAY_DONTCARE;
  self->mask_mon  = ALARM_RECUR_MON_DONTCARE;
  self->special   = ALARM_RECUR_SPECIAL_NONE;
}

/* ------------------------------------------------------------------------- *
 * alarm_recur_dtor
 * ------------------------------------------------------------------------- */

void
alarm_recur_dtor(alarm_recur_t *self)
{
}

/* ------------------------------------------------------------------------- *
 * alarm_recur_create
 * ------------------------------------------------------------------------- */

alarm_recur_t *
alarm_recur_create(void)
{
  alarm_recur_t *self = calloc(1, sizeof *self);
  alarm_recur_ctor(self);
  return self;
}

/* ------------------------------------------------------------------------- *
 * alarm_recur_delete
 * ------------------------------------------------------------------------- */

void
alarm_recur_delete(alarm_recur_t *self)
{
  if( self != 0 )
  {
    alarm_recur_dtor(self);
    free(self);
  }
}

/* ------------------------------------------------------------------------- *
 * alarm_recur_init_from_tm
 * ------------------------------------------------------------------------- */

void
alarm_recur_init_from_tm(alarm_recur_t *self, const struct tm *tm)
{
  self->mask_min  = 1llu << tm->tm_min;
  self->mask_hour = 1u   << tm->tm_hour;
  self->mask_mday = 1u   << tm->tm_mday;
  self->mask_wday = 1u   << tm->tm_wday;
  self->mask_mon  = 1u   << tm->tm_mon;
  self->special   = 0;
}

/* ------------------------------------------------------------------------- *
 * alarm_recur_delete_cb
 * ------------------------------------------------------------------------- */

void
alarm_recur_delete_cb(void *self)
{
  alarm_recur_delete(self);
}

/* ------------------------------------------------------------------------- *
 * alarm_recur_handle_masks
 * ------------------------------------------------------------------------- */

static
time_t
alarm_recur_handle_masks(const alarm_recur_t *self, struct tm *dst,
                         int align_only, const char *tz)
{
  time_t    res = -1;
  int       hit = (align_only != 0);

// QUARANTINE   struct tm src = *dst;
// QUARANTINE   log_debug("@alarm_recur_handle_masks\n");

  // SECONDS
  if( dst->tm_sec != 0 )
  {
    dst->tm_sec  = 0;
    dst->tm_min += 1;
    hit = 1;
    if( (res = ticker_build_tm(dst, tz)) == -1 )
    {
      goto cleanup;
    }
  }

// QUARANTINE   ticker_diff_tm(dst, &src); src=*dst;
// QUARANTINE   log_debug("(minutes)\n");

  // MINUTES
  if( self->mask_min )
  {
    for( dst->tm_min += !hit; ; dst->tm_min += 1, hit = 1 )
    {
      if( (res = ticker_build_tm(dst, tz)) == -1 )
      {
        goto cleanup;
      }
      if( (self->mask_min & (1llu<<dst->tm_min)) )
      {
        break;
      }
    }
  }

// QUARANTINE   ticker_diff_tm(dst, &src); src=*dst;
// QUARANTINE   log_debug("(hours)\n");

  // HOURS
  if( self->mask_hour )
  {
    for( dst->tm_hour += !hit; ; dst->tm_hour += 1, hit = 1 )
    {
      if( (res = ticker_build_tm(dst, tz)) == -1 )
      {
        goto cleanup;
      }
      if( (self->mask_hour & (1u<<dst->tm_hour)) )
      {
        break;
      }
    }
  }

// QUARANTINE   ticker_diff_tm(dst, &src); src=*dst;
// QUARANTINE   log_debug("(days)\n");

  // DAY OF MONTH and DAY OF WEEK
  if( self->mask_wday || self->mask_mday )
  {
    //uint32_t mday = self->mask_mday ?: ALARM_RECUR_MDAY_ALL;
    uint32_t wday = self->mask_wday ?: ALARM_RECUR_WDAY_ALL;

    uint32_t mday = (self->mask_mday & ALARM_RECUR_MDAY_ALL);

    if( !mday && !(self->mask_mday & ALARM_RECUR_MDAY_EOM) )
    {
      mday  = ALARM_RECUR_MDAY_ALL;
    }

// QUARANTINE     log_debug("wday=%08x\n", wday);
// QUARANTINE     log_debug("mday=%08x\n", mday);

    for( dst->tm_mday += !hit; ; dst->tm_mday += 1, hit = 1 )
    {
      if( (res = ticker_build_tm(dst, tz)) == -1 )
      {
        goto cleanup;
      }

      uint32_t temp = mday;

      if( self->mask_mday & ALARM_RECUR_MDAY_EOM )
      {
        uint32_t mask = (1u << ticker_get_days_in_month(dst)) - 1;
        if( mday == 0 || mday > mask )
        {
          // just last day or day of month > days in this month
          temp |= ~mask;
        }
      }

      if( (wday & (1<<dst->tm_wday)) && (temp & (1<<dst->tm_mday)) )
      {
        break;
      }
    }
  }

// QUARANTINE   ticker_diff_tm(dst, &src); src=*dst;
// QUARANTINE   log_debug("(months)\n");

  // MONTHS
  if( self->mask_mon )
  {
    for( dst->tm_mon += !hit; ; dst->tm_mon += 1, hit = 1 )
    {
      if( (res = ticker_build_tm(dst, tz)) == -1 )
      {
        goto cleanup;
      }
      if( (self->mask_mon & (1u<<dst->tm_mon)) )
      {
        break;
      }
    }
  }

  cleanup:

// QUARANTINE   ticker_diff_tm(dst, &src); src=*dst;
// QUARANTINE   log_debug("(done)\n");
// QUARANTINE   log_debug("return: %d, %s\n", (int)res, ctime(&res));

  return res;
}

static
time_t
alarm_recur_handle_specials(const alarm_recur_t *self, struct tm *dst,
                            const char *tz)
{
  switch( self->special )
  {
  case ALARM_RECUR_SPECIAL_BIWEEKLY:
    dst->tm_mday += 14;
    break;

  case ALARM_RECUR_SPECIAL_MONTHLY:
    dst->tm_mon += 1;
    break;

  case ALARM_RECUR_SPECIAL_YEARLY:
    dst->tm_year += 1;
    break;
  }

  return ticker_build_tm(dst, tz);
}

time_t
alarm_recur_align(const alarm_recur_t *self, struct tm *trg, const char *tz)
{
  return alarm_recur_handle_masks(self, trg, 1, tz);
}

time_t
alarm_recur_next(const alarm_recur_t *self, struct tm *trg, const char *tz)
{
  alarm_recur_handle_specials(self, trg, tz);
  return alarm_recur_handle_masks(self, trg, 0, tz);
}

#ifdef TESTMAIN

int
main(int ac, char **av)
{
  alarm_recur_t rec;

  alarm_recur_ctor(&rec);

#if 0 // 8:45 and 16:45 every Tuesday & Saturday
  rec.mask_min  = 1ull << 45;
  rec.mask_hour = (1u << 16) | (1u << 8);
  rec.mask_wday = ALARM_RECUR_WDAY_TUE | ALARM_RECUR_WDAY_SAT;
#endif

#if 0 // 12:00 30th or last day of month
  rec.mask_min  = 1ull << 0;
  rec.mask_hour = (1u << 12);
// QUARANTINE   rec.mask_mday = (1u << 27) | ALARM_RECUR_MDAY_EOM;
// QUARANTINE   rec.mask_mday = (1u << 28) | ALARM_RECUR_MDAY_EOM;
// QUARANTINE   rec.mask_mday = (1u << 29) | ALARM_RECUR_MDAY_EOM;
  rec.mask_mday = (1u << 30) | ALARM_RECUR_MDAY_EOM;
// QUARANTINE   rec.mask_mday = (1u << 31) | ALARM_RECUR_MDAY_EOM;
  //rec.mask_mday = ALARM_RECUR_MDAY_EOM;
#endif

#if 0 // Mon - Fri, 8:00 -> 16:00, on the hour
  rec.mask_min  = 1ull << 0;
  rec.mask_hour = (1u << 17) - (1u << 8);
  rec.mask_wday = ALARM_RECUR_WDAY_MONFRI;
#endif

#if 01 // Friday the 13th
  rec.mask_min  = 1 << 0;
  rec.mask_hour = 1 << 12;
  rec.mask_wday = ALARM_RECUR_WDAY_FRI;
  rec.mask_mday = 1 << 13;
#endif

  struct tm now, tmp;
  struct tm tpl =
  {
    .tm_sec     =  0,
    .tm_min     = -1,
    .tm_hour    = -1,
    .tm_mday    = -1,
    .tm_mon     = -1,
    .tm_year    = -1,
    .tm_wday    = -1,
    .tm_yday    = -1,
    .tm_isdst   = -1,
  };

  time_t t = ticker_get_time();

  if( ac > 1 ) tpl.tm_year = strtol(av[1],0,0)-1900;
  if( ac > 2 ) tpl.tm_mon  = strtol(av[2],0,0)-1;
  if( ac > 3 ) tpl.tm_mday = strtol(av[3],0,0);
  if( ac > 4 ) tpl.tm_hour = strtol(av[4],0,0);
  if( ac > 5 ) tpl.tm_min  = strtol(av[5],0,0);
  if( ac > 6 ) tpl.tm_sec  = strtol(av[6],0,0);

// QUARANTINE   log_debug("Template:  "); ticker_show_tm(&tpl);

  localtime_r(&t, &now);
// QUARANTINE   log_debug("Currently: ");ticker_show_tm(&now);

  tm_init(&tmp, &now, &tpl);

// QUARANTINE   log_debug("Applied:   ");ticker_show_tm(&tmp);

  for( int i = 0; i < 100; ++i )
  {
// QUARANTINE     printf("\n");
    t = alarm_recur_handle_masks(&rec, &tmp, 0);
// QUARANTINE     printf("\n");
// QUARANTINE     log_debug("Next:      ");ticker_show_tm(&tmp);
  }
  return 0;
}

#endif

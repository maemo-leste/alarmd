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

#include "alarmd_config.h"

#include "libalarm.h"
#include "ticker.h"
#include "logging.h"

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
                         const char *tz, int align_only)
{
  time_t    res = -1;
  int       inc = (align_only == 0);

  /* Hours, minutes and seconds behave logically and
   * can thus be handled separately */

  // SECONDS
  if( dst->tm_sec != 0 )
  {
    inc = 0, dst->tm_min += 1, dst->tm_sec = 0;
    if( (res = ticker_build_tm_guess_dst(dst, tz)) == -1 )
    {
      goto cleanup;
    }
    //log_debug("SECS: %s\n", ticker_date_format_long(0,0,res));
  }

  // MINUTES
  if( self->mask_min & ALARM_RECUR_MIN_ALL )
  {
    for( dst->tm_min += inc, inc = 0; ; dst->tm_min += 1 )
    {
      if( (res = ticker_build_tm_guess_dst(dst, tz)) == -1 )
      {
        goto cleanup;
      }
      if( (self->mask_min & (1llu<<dst->tm_min)) )
      {
        break;
      }
    }
    //log_debug("MINS: %s\n", ticker_date_format_long(0,0,res));
  }

  // HOURS
  if( self->mask_hour & ALARM_RECUR_HOUR_ALL )
  {
    for( dst->tm_hour += inc, inc = 0; ; dst->tm_hour += 1 )
    {
      if( (res = ticker_build_tm_guess_dst(dst, tz)) == -1 )
      {
        goto cleanup;
      }
      if( (self->mask_hour & (1u<<dst->tm_hour)) )
      {
        break;
      }
    }
    //log_debug("HOUR: %s\n", ticker_date_format_long(0,0,res));
  }

  /* Month, day of month and day of week do not progress
   * linearly and thus must be handled all at once */

  // DAY OF MONTH and DAY OF WEEK and MONTH

  uint32_t M_wday = self->mask_wday & ALARM_RECUR_WDAY_ALL;
  uint32_t M_mday = self->mask_mday & ALARM_RECUR_MDAY_ALL;
  uint32_t M_eom  = self->mask_mday & ALARM_RECUR_MDAY_EOM;
  uint32_t M_mon  = self->mask_mon  & ALARM_RECUR_MON_ALL;

  if( M_wday || M_mday || M_eom || M_mon )
  {
    if( M_wday == 0 )
    {
      M_wday = ALARM_RECUR_WDAY_ALL;
    }
    if( M_mday == 0 && M_eom == 0 )
    {
      M_mday = ALARM_RECUR_MDAY_ALL;
    }
    if( M_mon == 0 )
    {
      M_mon = ALARM_RECUR_MON_ALL;
    }

    for( dst->tm_mday += inc, inc = 0;; )
    {
      if( (res = ticker_build_tm_guess_dst(dst, tz)) == -1 )
      {
        goto cleanup;
      }

      if( !(M_mon & (1u << dst->tm_mon)) )
      {
        inc = 0, dst->tm_mon += 1, dst->tm_mday = 1;
        continue;
      }

      uint32_t T_mday = M_mday;

      if( M_eom )
      {
        T_mday |= (1u << ticker_get_days_in_month(dst));
      }

      if( (M_wday & (1<<dst->tm_wday)) && (T_mday & (1<<dst->tm_mday)) )
      {
        break;
      }
      inc = 0, dst->tm_mday += 1;
    }
    //log_debug("DATE: %s\n", ticker_date_format_long(0,0,res));
  }

  cleanup:

  return res;
}

/* ------------------------------------------------------------------------- *
 * alarm_recur_handle_specials  --  handle special recurrency periods
 * ------------------------------------------------------------------------- */

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

  return ticker_build_tm_guess_dst(dst, tz);
}

/* ------------------------------------------------------------------------- *
 * alarm_recur_align  --  align to next allowed recurrence time
 * ------------------------------------------------------------------------- */

time_t
alarm_recur_align(const alarm_recur_t *self, struct tm *trg, const char *tz)
{
  return alarm_recur_handle_masks(self, trg, tz, 1);
}

/* ------------------------------------------------------------------------- *
 * alarm_recur_next  --  get next recurrence time in future
 * ------------------------------------------------------------------------- */

time_t
alarm_recur_next(const alarm_recur_t *self, struct tm *trg, const char *tz)
{
  alarm_recur_handle_specials(self, trg, tz);
  return alarm_recur_handle_masks(self, trg, tz, 0);
}

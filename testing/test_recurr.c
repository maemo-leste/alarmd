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

#include "../src/libalarm.h"
#include "../src/ticker.h"
#include "../src/logging.h"

#include <stdlib.h>

int
main(int ac, char **av)
{
  alarm_recur_t rec;

  int h = 12;
  int m = 13;

  log_set_level(LOG_DEBUG);

  alarm_recur_ctor(&rec);
  rec.mask_min  = 1ull << m;
  rec.mask_hour = 1u   << h;

  rec.mask_wday |= ALARM_RECUR_WDAY_FRI;
  rec.mask_mday |= 1 << 13;
  rec.mask_mon  |= 0x55555555;

  log_error("FOO\n");
  log_warning("FOO\n");
  log_info("FOO\n");
  log_notice("FOO\n");
  log_debug("FOO\n");

  struct tm now, tmp;
  time_t t = ticker_get_time();

  localtime_r(&t, &now);

  tmp = now;

  for( int i = 0; i < 400; ++i )
  {
    log_debug("----====( %d )====----\n", i);
    t = alarm_recur_next(&rec, &tmp, "EET");
    //log_debug("Next:      ");ticker_show_tm(&tmp);
    log_debug("Next:      %s", asctime(&tmp));

    if( t == -1 )
    {
      log_error("FAILED\n");
      exit(1);
    }

    if( tmp.tm_hour != h )
    {
      log_error("tm_hour != %d\n", h);
      exit(1);
    }
    if( tmp.tm_min != m )
    {
      log_error("tm_min != %d\n", m);
      exit(1);
    }
  }
  return 0;
}

// -*- mode: c -*-

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

#ifdef TICKER
TICKER(int,      get_synced,     (void))
TICKER(time_t,   get_time,       (void))
TICKER(int,      set_time,       (time_t tick))
TICKER(time_t,   mktime,         (struct tm *tm, const char *tz))
TICKER(int,      get_timezone,   (char *s, size_t max))
TICKER(int,      get_tzname,     (char *s, size_t max))
TICKER(int,      set_timezone,   (const char *tz))
TICKER(int,      get_utc,        (struct tm *tm))
TICKER(int,      get_utc_ex,     (time_t tick, struct tm *tm))
TICKER(int,      get_local,      (struct tm *tm))
TICKER(int,      get_local_ex,   (time_t tick, struct tm *tm))
TICKER(int,      get_remote,     (time_t tick, const char *tz, struct tm *tm))
TICKER(int,      get_time_format,(char *s, size_t max))
TICKER(int,      set_time_format,(const char *fmt))
TICKER(int,      format_time,    (const struct tm *tm, const char *fmt, char *s, size_t max))
TICKER(int,      get_utc_offset, (const char *tz))
TICKER(int,      get_dst_usage,  (time_t tick, const char *tz))
TICKER(double,   diff,           (time_t t1, time_t t2))
TICKER(int,      get_time_diff,  (time_t tick, const char *tz1, const char *tz2))
TICKER(int,      set_autosync,   (int enable))
TICKER(int,      get_autosync,   (void))
TICKER(int,      is_net_time_changed,         (time_t *tick, char *s, size_t max))
TICKER(int,      activate_net_time,           (void))
TICKER(int,      is_operator_time_accessible, (void))
#undef TICKER
#endif

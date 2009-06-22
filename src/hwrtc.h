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

#ifndef HWRTC_H_
#define HWRTC_H_

#include <time.h>

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

time_t hwrtc_mktime   (struct tm *tm);
void   hwrtc_quit     (void);
int    hwrtc_init     (void);
time_t hwrtc_get_time (struct tm *utc);
time_t hwrtc_get_alarm(struct tm *utc, int *enabled);
int    hwrtc_set_alarm(const struct tm *utc, int enabled);

#ifdef __cplusplus
};
#endif

#endif /* HWRTC_H_ */

/* ========================================================================= *
 * File: hwrtc.h
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

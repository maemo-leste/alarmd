/* ========================================================================= *
 * File: codec.h
 *
 * Copyright (C) 2008 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 15-May-2008 Simo Piiroinen
 * - modified for profiled usage
 *
 * 01-May-2008 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#ifndef CODEC_H_
# define CODEC_H_

# include <time.h>
# include <dbus/dbus.h>

# include "libalarm.h"

# ifdef __cplusplus
extern "C" {
# elif 0
} /* fool JED indentation ... */
# endif

/* -- encode -- */

void encode_bool    (DBusMessageIter *iter, int *err, const int *pval);
void encode_unsigned(DBusMessageIter *iter, int *err, const unsigned *pval);
void encode_int     (DBusMessageIter *iter, int *err, const int *pval);
void encode_string  (DBusMessageIter *iter, int *err, const void *pval);
void encode_size    (DBusMessageIter *iter, int *err, const size_t *pval);
void encode_cookie  (DBusMessageIter *iter, int *err, const cookie_t *pval);
void encode_time    (DBusMessageIter *iter, int *err, const time_t *pval);
void encode_tm      (DBusMessageIter *iter, int *err, const struct tm *tm);
void encode_action  (DBusMessageIter *iter, int *err, const alarm_action_t *act, const char **def_args);
void encode_event   (DBusMessageIter *iter, int *err, const alarm_event_t *eve, const char **def_args);
void encode_uint32  (DBusMessageIter *iter, int *err, const uint32_t *pval);
void encode_uint64  (DBusMessageIter *iter, int *err, const uint64_t *pval);
void encode_recur   (DBusMessageIter *iter, int *err, const alarm_recur_t *rec);
void encode_attr    (DBusMessageIter *iter, int *err, const alarm_attr_t *att);

/* -- decode -- */

void decode_bool    (DBusMessageIter *iter, int *err, int *pval);
void decode_unsigned(DBusMessageIter *iter, int *err, unsigned *pval);
void decode_int     (DBusMessageIter *iter, int *err, int *pval);
void decode_string  (DBusMessageIter *iter, int *err, const char **pval);
void decode_dstring (DBusMessageIter *iter, int *err, char **pval);
void decode_size    (DBusMessageIter *iter, int *err, size_t *pval);
void decode_cookie  (DBusMessageIter *iter, int *err, cookie_t *pval);
void decode_time    (DBusMessageIter *iter, int *err, time_t *pval);
void decode_tm      (DBusMessageIter *iter, int *err, struct tm *tm);
void decode_action  (DBusMessageIter *iter, int *err, alarm_action_t *act);
void decode_event   (DBusMessageIter *iter, int *err, alarm_event_t *eve);
void decode_uint32  (DBusMessageIter *iter, int *err, uint32_t *pval);
void decode_uint64  (DBusMessageIter *iter, int *err, uint64_t *pval);
void decode_recur   (DBusMessageIter *iter, int *err, alarm_recur_t *rec);
void decode_attr    (DBusMessageIter *iter, int *err, alarm_attr_t *rec);

# ifdef __cplusplus
};
# endif

#endif /* CODEC_H_ */

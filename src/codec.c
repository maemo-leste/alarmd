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

#include "codec.h"

#include "logging.h"

#include <stdlib.h>
#include <string.h>

/* ========================================================================= *
 * Utilites for encoding/decoding dbus messages using message
 * iterators. Useful only because libdbus does not expose some
 * internal functions via the public API.
 * ========================================================================= */

#define SET_ERR\
  do { *err = 1; log_error_F("failed\n"); } while(0)

void
encode_uint32(DBusMessageIter *iter, int *err, const uint32_t *pval)
{
  if( *err == 0 )
  {
    dbus_uint32_t val = *pval;
    if( !dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &val) )
    {
      SET_ERR;
    }
  }
}

void
decode_uint32(DBusMessageIter *iter, int *err, uint32_t *pval)
{
  dbus_uint32_t val = 0;
  if( *err == 0 )
  {
    if( dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_UINT32 )
    {
      dbus_message_iter_get_basic(iter, &val);
      dbus_message_iter_next(iter);
    }
    else
    {
      SET_ERR;
    }
  }
  *pval = val;
}

void
encode_uint64(DBusMessageIter *iter, int *err, const uint64_t *pval)
{
  if( *err == 0 )
  {
    dbus_uint64_t val = *pval;
    if( !dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT64, &val) )
    {
      SET_ERR;
    }
  }
}

void
decode_uint64(DBusMessageIter *iter, int *err, uint64_t *pval)
{
  dbus_uint64_t val = 0;
  if( *err == 0 )
  {
    if( dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_UINT64 )
    {
      dbus_message_iter_get_basic(iter, &val);
      dbus_message_iter_next(iter);
    }
    else
    {
      SET_ERR;
    }
  }
  *pval = val;
}

#ifdef DEAD_CODE
void
encode_bool(DBusMessageIter *iter, int *err, const int *pval)
{
  if( *err == 0 )
  {
    dbus_bool_t  val = (*pval != 0);
    if( !dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &val) )
    {
      SET_ERR;
    }
  }
}
#endif

#ifdef DEAD_CODE
void
decode_bool(DBusMessageIter *iter, int *err, int *pval)
{
  dbus_bool_t  val = 0;

  if( *err == 0 )
  {
    if( dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_BOOLEAN )
    {
      dbus_message_iter_get_basic(iter, &val);
      dbus_message_iter_next(iter);
    }
    else
    {
      SET_ERR;
    }
  }

  *pval = (val != 0);
}
#endif

void
encode_unsigned(DBusMessageIter *iter, int *err, const unsigned *pval)
{
  if( *err == 0 )
  {
    dbus_uint32_t val = *pval;
    if( !dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &val) )
    {
      SET_ERR;
    }
  }
}

void
decode_unsigned(DBusMessageIter *iter, int *err, unsigned *pval)
{
  dbus_uint32_t val = 0;
  if( *err == 0 )
  {
    if( dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_UINT32 )
    {
      dbus_message_iter_get_basic(iter, &val);
      dbus_message_iter_next(iter);
    }
    else
    {
      SET_ERR;
    }
  }
  *pval = val;
}

void
encode_int(DBusMessageIter *iter, int *err, const int *pval)
{
  if( *err == 0 )
  {
    dbus_int32_t val = *pval;
    if( !dbus_message_iter_append_basic(iter, DBUS_TYPE_INT32, &val) )
    {
      SET_ERR;
    }
  }
}

void
decode_int(DBusMessageIter *iter, int *err, int *pval)
{
  dbus_int32_t val = 0;
  if( *err == 0 )
  {
    if( dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_INT32 )
    {
      dbus_message_iter_get_basic(iter, &val);
      dbus_message_iter_next(iter);
    }
    else
    {
      SET_ERR;
    }
  }
  *pval = val;
}

void
encode_string(DBusMessageIter *iter, int *err, const void *pval)
{
  if( *err == 0 )
  {
    const char **ptr = (const char **)pval;
    const char  *val = *ptr ?: "";
    if( !dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &val) )
    {
      SET_ERR;
    }
  }
}

void
decode_string(DBusMessageIter *iter, int *err, const char **pval)
{
  const char *tmp = 0;

  if( err != 0 )
  {
    if( dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRING )
    {
      dbus_message_iter_get_basic(iter, &tmp);
      dbus_message_iter_next(iter);
    }
    else
    {
      SET_ERR;
    }
  }
  *pval = tmp;
}

void
decode_dstring(DBusMessageIter *iter, int *err, char **pval)
{
  const char *tmp = 0;
  decode_string(iter, err, &tmp);
  free(*pval);
  *pval = strdup(tmp ?: "");
}

void
encode_size(DBusMessageIter *iter, int *err, const size_t *pval)
{
  unsigned tmp = *pval;
  encode_unsigned(iter, err, &tmp);
}

void
decode_size(DBusMessageIter *iter, int *err, size_t *pval)
{
  unsigned tmp = 0;
  decode_unsigned(iter, err, &tmp);
  *pval = tmp;
}

void
encode_cookie(DBusMessageIter *iter, int *err, const cookie_t *pval)
{
  unsigned tmp = *pval;
  encode_unsigned(iter, err, &tmp);
}

void
decode_cookie(DBusMessageIter *iter, int *err, cookie_t *pval)
{
  unsigned tmp = 0;
  decode_unsigned(iter, err, &tmp);
  *pval = tmp;
}

void
encode_time(DBusMessageIter *iter, int *err, const time_t *pval)
{
  int tmp = *pval;
  encode_int(iter, err, &tmp);
}

void
decode_time(DBusMessageIter *iter, int *err, time_t *pval)
{
  int tmp = 0;
  decode_int(iter, err, &tmp);
  *pval = tmp;
}

void
encode_tm(DBusMessageIter *iter, int *err, const struct tm *tm)
{
  encode_int(iter, err, &tm->tm_sec);
  encode_int(iter, err, &tm->tm_min);
  encode_int(iter, err, &tm->tm_hour);
  encode_int(iter, err, &tm->tm_mday);
  encode_int(iter, err, &tm->tm_mon);
  encode_int(iter, err, &tm->tm_year);
  encode_int(iter, err, &tm->tm_wday);
  encode_int(iter, err, &tm->tm_yday);
  encode_int(iter, err, &tm->tm_isdst);
}

void
decode_tm(DBusMessageIter *iter, int *err, struct tm *tm)
{
  decode_int(iter, err, &tm->tm_sec);
  decode_int(iter, err, &tm->tm_min);
  decode_int(iter, err, &tm->tm_hour);
  decode_int(iter, err, &tm->tm_mday);
  decode_int(iter, err, &tm->tm_mon);
  decode_int(iter, err, &tm->tm_year);
  decode_int(iter, err, &tm->tm_wday);
  decode_int(iter, err, &tm->tm_yday);
  decode_int(iter, err, &tm->tm_isdst);
}

void
encode_action(DBusMessageIter *iter, int *err, const alarm_action_t *act,
              const char **def_args)
{
  const char *use_args = act->dbus_args;

  if( (act->flags & ALARM_ACTION_TYPE_DBUS) && (use_args == 0) )
  {
    use_args = *def_args, *def_args = 0;
  }

  encode_unsigned (iter, err, &act->flags);
  encode_string   (iter, err, &act->label);
  encode_string   (iter, err, &act->exec_command);
  encode_string   (iter, err, &act->dbus_interface);
  encode_string   (iter, err, &act->dbus_service);
  encode_string   (iter, err, &act->dbus_path);
  encode_string   (iter, err, &act->dbus_name);
  encode_string   (iter, err, &use_args);
  //encode_string   (iter, err, &act->dbus_args);
}

void
decode_action(DBusMessageIter *iter, int *err, alarm_action_t *act)
{
  decode_unsigned (iter, err, &act->flags);
  decode_dstring  (iter, err, &act->label);
  decode_dstring  (iter, err, &act->exec_command);
  decode_dstring  (iter, err, &act->dbus_interface);
  decode_dstring  (iter, err, &act->dbus_service);
  decode_dstring  (iter, err, &act->dbus_path);
  decode_dstring  (iter, err, &act->dbus_name);
  decode_dstring  (iter, err, &act->dbus_args);
}

void
encode_recur(DBusMessageIter *iter, int *err, const alarm_recur_t *rec)
{
  encode_uint64 (iter, err, &rec->mask_min);
  encode_uint32 (iter, err, &rec->mask_hour);
  encode_uint32 (iter, err, &rec->mask_mday);
  encode_uint32 (iter, err, &rec->mask_wday);
  encode_uint32 (iter, err, &rec->mask_mon);
  encode_uint32 (iter, err, &rec->special);
}

void
decode_recur(DBusMessageIter *iter, int *err, alarm_recur_t *rec)
{
  decode_uint64 (iter, err, &rec->mask_min);
  decode_uint32 (iter, err, &rec->mask_hour);
  decode_uint32 (iter, err, &rec->mask_mday);
  decode_uint32 (iter, err, &rec->mask_wday);
  decode_uint32 (iter, err, &rec->mask_mon);
  decode_uint32 (iter, err, &rec->special);
}

void
encode_attr(DBusMessageIter *iter, int *err, const alarm_attr_t *att)
{
  encode_string(iter, err, &att->attr_name);
  encode_int   (iter, err, &att->attr_type);

  switch( att->attr_type )
  {
  case ALARM_ATTR_NULL:  break;
  case ALARM_ATTR_INT:   encode_int   (iter, err, &att->attr_data.ival); break;
  case ALARM_ATTR_TIME:  encode_time  (iter, err, &att->attr_data.tval); break;
  case ALARM_ATTR_STRING:encode_string(iter, err, &att->attr_data.sval); break;
  }
}

void
decode_attr(DBusMessageIter *iter, int *err, alarm_attr_t *att)
{
  decode_dstring(iter, err, &att->attr_name);
  decode_int    (iter, err, &att->attr_type);

  switch( att->attr_type )
  {
  case ALARM_ATTR_NULL:  break;
  case ALARM_ATTR_INT:   decode_int    (iter, err, &att->attr_data.ival);break;
  case ALARM_ATTR_TIME:  decode_time   (iter, err, &att->attr_data.tval);break;
  case ALARM_ATTR_STRING:decode_dstring(iter, err, &att->attr_data.sval);break;
  }
}

void
encode_event(DBusMessageIter *iter, int *err, const alarm_event_t *eve,
             const char **def_args)
{
  encode_cookie   (iter, err, &eve->ALARMD_PRIVATE(cookie));
  encode_time     (iter, err, &eve->ALARMD_PRIVATE(trigger));
  encode_string   (iter, err, &eve->title);
  encode_string   (iter, err, &eve->message);
  encode_string   (iter, err, &eve->sound);
  encode_string   (iter, err, &eve->icon);
  encode_unsigned (iter, err, &eve->flags);
  encode_string   (iter, err, &eve->alarm_appid);
  encode_time     (iter, err, &eve->alarm_time);
  encode_tm       (iter, err, &eve->alarm_tm);
  encode_string   (iter, err, &eve->alarm_tz);
  encode_time     (iter, err, &eve->recur_secs);
  encode_int      (iter, err, &eve->recur_count);
  encode_time     (iter, err, &eve->snooze_secs);
  encode_time     (iter, err, &eve->snooze_total);

  encode_size     (iter, err, &eve->action_cnt);
  encode_int      (iter, err, &eve->response);

  for( size_t i = 0; i < eve->action_cnt; ++i )
  {
    encode_action(iter, err, &eve->action_tab[i], def_args);
  }

  encode_size     (iter, err, &eve->recurrence_cnt);
  for( size_t i = 0; i < eve->recurrence_cnt; ++i )
  {
    encode_recur(iter, err, &eve->recurrence_tab[i]);
  }

  encode_size     (iter, err, &eve->attr_cnt);
  for( size_t i = 0; i < eve->attr_cnt; ++i )
  {
    encode_attr(iter, err, eve->attr_tab[i]);
  }
}

static
int
decode_eom_p(DBusMessageIter *iter, int *err)
{
  if( *err == 0 )
  {
    if( dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_INVALID )
    {
      return 1;
    }
  }
  return 0;
}

void
decode_event(DBusMessageIter *iter, int *err, alarm_event_t *eve)
{
  size_t action_cnt     = 0;
  size_t recurrence_cnt = 0;

  alarm_event_del_actions(eve);
  alarm_event_del_recurrences(eve);

  decode_cookie   (iter, err, &eve->ALARMD_PRIVATE(cookie));
  decode_time     (iter, err, &eve->ALARMD_PRIVATE(trigger));
  decode_dstring  (iter, err, &eve->title);
  decode_dstring  (iter, err, &eve->message);
  decode_dstring  (iter, err, &eve->sound);
  decode_dstring  (iter, err, &eve->icon);
  decode_unsigned (iter, err, &eve->flags);
  decode_dstring  (iter, err, &eve->alarm_appid);
  decode_time     (iter, err, &eve->alarm_time);
  decode_tm       (iter, err, &eve->alarm_tm);
  decode_dstring  (iter, err, &eve->alarm_tz);
  decode_time     (iter, err, &eve->recur_secs);
  decode_int      (iter, err, &eve->recur_count);
  decode_time     (iter, err, &eve->snooze_secs);
  decode_time     (iter, err, &eve->snooze_total);

  decode_size     (iter, err, &action_cnt);
  decode_int      (iter, err, &eve->response);

  /* - - - - - - - - - - - - - - - - - - - *
   * action table
   * - - - - - - - - - - - - - - - - - - - */

  alarm_action_t *act = alarm_event_add_actions(eve, action_cnt);
  for( size_t i = 0; i < action_cnt; ++i )
  {
    decode_action(iter, err, &act[i]);
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * recurrence table
   * - - - - - - - - - - - - - - - - - - - */

  decode_size     (iter, err, &recurrence_cnt);
  alarm_recur_t *rec = alarm_event_add_recurrences(eve, recurrence_cnt);
  for( size_t i = 0; i < recurrence_cnt; ++i )
  {
    decode_recur(iter, err, &rec[i]);
  }
  /* - - - - - - - - - - - - - - - - - - - *
   * attribute table sent by libalarm >= 1.0.4
   * - - - - - - - - - - - - - - - - - - - */

  if( !decode_eom_p(iter, err) )
  {
    size_t count = 0;
    decode_size     (iter, err, &count);
    for( size_t i = 0; i < count; ++i )
    {
      alarm_attr_t *att = alarm_event_add_attr(eve, "\x7f");
      decode_attr(iter, err, att);
    }
  }
}

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
#include "libalarm-async.h"

#include "logging.h"
#include "dbusif.h"
#include "serialize.h"
#include "alarm_dbus.h"

#include <stdlib.h>

/* ------------------------------------------------------------------------- *
 * client_make_method_message  --  construct dbus method call message
 * ------------------------------------------------------------------------- */

static
DBusMessage *
client_make_method_message(const char *method, int dbus_type, ...)
{
  DBusMessage *msg = 0;
  va_list      va;

  va_start(va, dbus_type);

  if( !(msg = dbus_message_new_method_call(ALARMD_SERVICE,
                                           ALARMD_PATH,
                                           ALARMD_INTERFACE,
                                           method)) )
  {
    log_error("%s\n", "dbus_message_new_method_call");
    goto cleanup;
  }

  if( !dbus_message_append_args_valist(msg, dbus_type, va) )
  {
    log_error("%s\n", "dbus_message_append_args_valist");
    dbus_message_unref(msg), msg = 0;
    goto cleanup;
  }

  cleanup:

  va_end(va);

  return msg;
}

/* ------------------------------------------------------------------------- *
 * client_parse_reply
 * ------------------------------------------------------------------------- */

static
dbus_bool_t
client_parse_reply(DBusMessage *rsp, DBusError *err, int arg, ...)
{
  dbus_bool_t res = 0;

  if( dbus_error_is_set(err) )
  {
    // error is already set -> nop
  }
  else if( dbus_message_get_type(rsp) == DBUS_MESSAGE_TYPE_METHOD_RETURN )
  {
    // try to parse the message
    va_list va;
    va_start(va, arg);
    res = dbus_message_get_args_valist (rsp, err, arg, va);
    va_end(va);
  }
  else if( dbus_set_error_from_message(err, rsp) )
  {
    // it was an error message and error is now set
  }
  else
  {
    dbus_set_error(err, DBUS_ERROR_NO_REPLY, "expected %s, got %s\n",
                   dbus_message_type_to_string(DBUS_MESSAGE_TYPE_METHOD_RETURN),
                   dbus_message_type_to_string(dbus_message_get_type(rsp)));

  }
  return res;
}

/* ------------------------------------------------------------------------- *
 * client_exec_method_call  --  send method call & wait for reply
 * ------------------------------------------------------------------------- */

static
int
client_exec_method_call(DBusMessage *msg, DBusMessage **prsp)
{
  int             res = -1;
  DBusConnection *con = 0;
  DBusMessage    *rsp = 0;
  DBusError       err = DBUS_ERROR_INIT;

#if ALARMD_ON_SYSTEM_BUS
  DBusBusType    type = DBUS_BUS_SYSTEM;
#else
  DBusBusType    type = DBUS_BUS_SESSION;
#endif

#if ALARMD_USE_PRIVATE_BUS
  if( !(con = dbus_bus_get_private(type, &err)) )
  {
    log_error("%s: %s: %s\n", "dbus_bus_get_private", err.name, err.message);
    goto cleanup;
  }
#else
  if( !(con = dbus_bus_get(type, &err)) )
  {
    log_error("%s: %s: %s\n", "dbus_bus_get", err.name, err.message);
    goto cleanup;
  }
#endif

#if 0
  if( prsp == 0 )
  {
    dbus_message_set_no_reply(msg, 1);
    if( !dbus_connection_send(con, msg, 0) )
    {
      log_error("%s\n", "dbus_connection_send");
      goto cleanup;
    }
    dbus_connection_flush(con);
  }
  else
  {
    if( !(rsp = dbus_connection_send_with_reply_and_block(con, msg, -1, &err)) )
    {
      log_error("%s: %s: %s\n", "dbus_connection_send_with_reply_and_block",
                err.name, err.message);
      goto cleanup;
    }
  }
#else
  int tmo = prsp ? -1 : 100;
  if( !(rsp = dbus_connection_send_with_reply_and_block(con, msg, tmo, &err)) )
  {
    log_error("%s: %s: %s\n", "dbus_connection_send_with_reply_and_block",
              err.name, err.message);
    if( prsp ) goto cleanup;
  }
#endif

  res = 0;

  cleanup:

#if ALARMD_USE_PRIVATE_BUS
  if( con != 0 ) dbus_connection_close(con), dbus_connection_unref(con);
#else
  if( con != 0 ) dbus_connection_unref(con);
#endif

  dbus_error_free(&err);

  if( prsp != 0 )
  {
    *prsp = rsp, rsp = 0;
  }

  if( rsp != 0 )
  {
    dbus_message_unref(rsp);
  }
  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmd_event_update
 * ------------------------------------------------------------------------- */

DBusMessage *
alarmd_event_update_encode_req(const alarm_event_t *event)
{
  DBusMessage    *msg = 0;

  if( alarm_event_is_sane(event) == -1 )
  {
    goto cleanup;
  }

  if( (msg = client_make_method_message(ALARMD_EVENT_UPDATE, DBUS_TYPE_INVALID)) )
  {
    if( !dbusif_encode_event(msg, event, 0) )
    {
      dbus_message_unref(msg), msg = 0;
    }
  }

  cleanup:

  return msg;
}

cookie_t
alarmd_event_update_decode_rsp(DBusMessage *rsp)
{
  cookie_t     tag = 0;
  DBusError    err = DBUS_ERROR_INIT;
  dbus_int32_t val = 0;

  if( client_parse_reply(rsp, &err,
                         DBUS_TYPE_INT32, &val,
                         DBUS_TYPE_INVALID) )
  {
    tag = (cookie_t)val;
  }

  if( dbus_error_is_set(&err) )
  {
    log_error_F("%s: %s\n", err.name, err.message);
  }
  dbus_error_free(&err);

  return tag;
}

cookie_t
alarmd_event_update(const alarm_event_t *event)
{
  cookie_t        tag = 0;
  DBusMessage    *msg = 0;
  DBusMessage    *rsp = 0;

  if( (msg = alarmd_event_update_encode_req(event)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      tag = alarmd_event_update_decode_rsp(rsp);
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  return tag;
}

/* ------------------------------------------------------------------------- *
 * alarmd_event_add_valist
 * ------------------------------------------------------------------------- */

DBusMessage *
alarmd_event_add_valist_encode_req(const alarm_event_t *event,
                                   int type, va_list va)
{
  DBusMessage    *msg = 0;
  char           *arg = 0;

  if( alarm_event_is_sane(event) == -1 )
  {
    goto cleanup;
  }

  if( type != DBUS_TYPE_INVALID )
  {
    if( (arg = serialize_pack_dbus_args(type, va)) == 0 )
    {
      goto cleanup;
    }
  }

  if( (msg = client_make_method_message(ALARMD_EVENT_ADD, DBUS_TYPE_INVALID)) )
  {
    if( !dbusif_encode_event(msg, event, arg) )
    {
      dbus_message_unref(msg), msg = 0;
    }
  }

  cleanup:

  free(arg);

  return msg;
}

cookie_t
alarmd_event_add_valist_decode_rsp(DBusMessage *rsp)
{
  cookie_t     tag = 0;
  DBusError    err = DBUS_ERROR_INIT;
  dbus_int32_t val = 0;

  if( client_parse_reply(rsp, &err,
                         DBUS_TYPE_INT32, &val,
                         DBUS_TYPE_INVALID) )
  {
    tag = (cookie_t)val;
  }

  if( dbus_error_is_set(&err) )
  {
    log_error_F("%s: %s\n", err.name, err.message);
  }

  dbus_error_free(&err);

  return tag;
}

cookie_t
alarmd_event_add_valist(const alarm_event_t *event, int type, va_list va)
{
  cookie_t        tag = 0;
  DBusMessage    *msg = 0;
  DBusMessage    *rsp = 0;

  if( (msg = alarmd_event_add_valist_encode_req(event, type, va)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      tag = alarmd_event_add_valist_decode_rsp(rsp);
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  return tag;
}

/* ------------------------------------------------------------------------- *
 * alarmd_event_add_extra
 * ------------------------------------------------------------------------- */

DBusMessage *
alarmd_event_add_with_dbus_params_encode_req(const alarm_event_t *event,
                                             int type, ...)
{
  DBusMessage *msg = 0;
  va_list va;
  va_start(va, type);
  msg = alarmd_event_add_valist_encode_req(event, type, va);
  va_end(va);
  return msg;
}

cookie_t
alarmd_event_add_with_dbus_params_decode_rsp(DBusMessage *rsp)
{
  return alarmd_event_add_valist_decode_rsp(rsp);
}

cookie_t
alarmd_event_add_with_dbus_params(const alarm_event_t *event, int type, ...)
{
  va_list va;
  va_start(va, type);
  cookie_t res = alarmd_event_add_valist(event, type, va);
  va_end(va);
  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmd_event_add
 * ------------------------------------------------------------------------- */

DBusMessage *
alarmd_event_add_encode_req(const alarm_event_t *event)
{
  return alarmd_event_add_with_dbus_params_encode_req(event, DBUS_TYPE_INVALID);
}
cookie_t
alarmd_event_add_decode_rsp(DBusMessage *rsp)
{
  return alarmd_event_add_with_dbus_params_decode_rsp(rsp);
}

cookie_t
alarmd_event_add(const alarm_event_t *event)
{
  return alarmd_event_add_with_dbus_params(event, DBUS_TYPE_INVALID);
}

/* ------------------------------------------------------------------------- *
 * alarmd_event_get
 * ------------------------------------------------------------------------- */

DBusMessage *
alarmd_event_get_encode_req(cookie_t cookie)
{
  dbus_int32_t  tag  = cookie;

  return client_make_method_message(ALARMD_EVENT_GET,
                                    DBUS_TYPE_INT32, &tag,
                                    DBUS_TYPE_INVALID);
}

alarm_event_t *
alarmd_event_get_decode_rsp(DBusMessage *rsp)
{
  alarm_event_t *eve = 0;

  if( dbus_message_get_type(rsp) == DBUS_MESSAGE_TYPE_ERROR )
  {
    // get error from error reply
    DBusError   err  = DBUS_ERROR_INIT;
    const char *name = dbus_message_get_error_name(rsp) ?: "noname";
    const char *mesg = "nomesg";
    dbus_message_get_args(rsp, &err,
                          DBUS_TYPE_STRING, &mesg,
                          DBUS_TYPE_INVALID);
    log_error_F("%s: %s\n", name, mesg);
    dbus_error_free(&err);
  }
  else
  {
    eve = dbusif_decode_event(rsp);
  }
  return eve;
}

alarm_event_t *
alarmd_event_get(cookie_t cookie)
{
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;
  alarm_event_t *eve = 0;

  if( (msg = alarmd_event_get_encode_req(cookie)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      eve = alarmd_event_get_decode_rsp(rsp);
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  return eve;
}

/* ------------------------------------------------------------------------- *
 * alarmd_event_del
 * ------------------------------------------------------------------------- */

DBusMessage *
alarmd_event_del_encode_req(cookie_t cookie)
{
  dbus_int32_t  tag  = cookie;

  return client_make_method_message(ALARMD_EVENT_DEL,
                                    DBUS_TYPE_INT32, &tag,
                                    DBUS_TYPE_INVALID);
}

int
alarmd_event_del_decode_rsp(DBusMessage *rsp)
{
  int         res = -1;
  DBusError   err = DBUS_ERROR_INIT;
  dbus_bool_t ack = 0;

  if( client_parse_reply(rsp, &err,
                         DBUS_TYPE_BOOLEAN, &ack,
                         DBUS_TYPE_INVALID) )
  {
    if( ack ) res = 0;
  }
  if( dbus_error_is_set(&err) )
  {
    log_error_F("%s: %s\n", err.name, err.message);
  }

  dbus_error_free(&err);

  return res;
}

int
alarmd_event_del(cookie_t cookie)
{
  int            res = -1;
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;

  if( (msg = alarmd_event_del_encode_req(cookie)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      res = alarmd_event_del_decode_rsp(rsp);
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmd_event_query
 * ------------------------------------------------------------------------- */

DBusMessage *
alarmd_event_query_encode_req(const time_t first, const time_t last,
                              int32_t flag_mask, int32_t flags,
                              const char *appid)
{
  dbus_int32_t  lo  = first;
  dbus_int32_t  hi  = last;
  dbus_int32_t  msk = flag_mask;
  dbus_int32_t  flg = flags;

  if( appid == 0 )
  {
    appid = "";
  }

  return client_make_method_message(ALARMD_EVENT_QUERY,
                                    DBUS_TYPE_INT32,  &lo,
                                    DBUS_TYPE_INT32,  &hi,
                                    DBUS_TYPE_INT32,  &msk,
                                    DBUS_TYPE_INT32,  &flg,
                                    DBUS_TYPE_STRING, &appid,
                                    DBUS_TYPE_INVALID);
}

cookie_t *
alarmd_event_query_decode_rsp(DBusMessage *rsp)
{
  cookie_t     *res = 0;
  dbus_int32_t *vec = 0;
  int           cnt = 0;
  DBusError     err = DBUS_ERROR_INIT;

  if( client_parse_reply(rsp, &err,
                         DBUS_TYPE_ARRAY, DBUS_TYPE_INT32, &vec, &cnt,
                         DBUS_TYPE_INVALID) )
  {
    res = calloc(cnt+1, sizeof *res);
    for( int i = 0; i < cnt; ++i )
    {
      res[i] = vec[i];
    }
  }

  if( dbus_error_is_set(&err) )
  {
    log_error_F("%s: %s\n", err.name, err.message);
  }

  dbus_error_free(&err);

  return res;
}

cookie_t *
alarmd_event_query(const time_t first, const time_t last,
                   int32_t flag_mask, int32_t flags, const char *appid)
{
  cookie_t      *res = 0;
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;

  if( (msg = alarmd_event_query_encode_req(first, last,
                                           flag_mask, flags, appid)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      res = alarmd_event_query_decode_rsp(rsp);
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmd_get_default_snooze
 * ------------------------------------------------------------------------- */

DBusMessage *
alarmd_get_default_snooze_encode_req(void)
{
  return client_make_method_message(ALARMD_SNOOZE_GET,
                                    DBUS_TYPE_INVALID);
}

int
alarmd_get_default_snooze_decode_rsp(DBusMessage *rsp)
{
  int           res = 0;
  DBusError     err = DBUS_ERROR_INIT;
  dbus_uint32_t dta = 0;

  if( client_parse_reply(rsp, &err,
                         DBUS_TYPE_UINT32, &dta,
                         DBUS_TYPE_INVALID) )
  {
    res = (int)dta;
  }
  if( dbus_error_is_set(&err) )
  {
    log_error_F("%s: %s\n", err.name, err.message);
  }

  dbus_error_free(&err);

  return res;
}

unsigned int
alarmd_get_default_snooze(void)
{
  unsigned int   res = 0;
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;

  if( (msg = alarmd_get_default_snooze_encode_req()) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      res = alarmd_get_default_snooze_decode_rsp(rsp);
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmd_set_default_snooze
 * ------------------------------------------------------------------------- */

DBusMessage *
alarmd_set_default_snooze_encode_req(unsigned int snooze)
{
  dbus_uint32_t tag = snooze;

  return client_make_method_message(ALARMD_SNOOZE_SET,
                                    DBUS_TYPE_UINT32, &tag,
                                    DBUS_TYPE_INVALID);
}

int
alarmd_set_default_snooze_decode_rsp(DBusMessage *rsp)
{
  int         res = -1;
  DBusError   err = DBUS_ERROR_INIT;
  dbus_bool_t ack = 0;

  if( client_parse_reply(rsp, &err,
                         DBUS_TYPE_BOOLEAN, &ack,
                         DBUS_TYPE_INVALID) )
  {
    if( ack ) res = 0;
  }

  if( dbus_error_is_set(&err) )
  {
    log_error_F("%s: %s\n", err.name, err.message);
  }

  dbus_error_free(&err);

  return res;
}

int
alarmd_set_default_snooze(unsigned int snooze)
{
  int            res = -1;
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;

  if( (msg = alarmd_set_default_snooze_encode_req(snooze)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      res = alarmd_set_default_snooze_decode_rsp(rsp);
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmd_ack_dialog
 * ------------------------------------------------------------------------- */

DBusMessage *
alarmd_ack_dialog_encode_req(cookie_t cookie, int button)
{
  dbus_int32_t c = cookie;
  dbus_int32_t b = button;

  return client_make_method_message(ALARMD_DIALOG_RSP,
                                    DBUS_TYPE_INT32, &c,
                                    DBUS_TYPE_INT32, &b,
                                    DBUS_TYPE_INVALID);
}

int
alarmd_ack_dialog_decode_rsp(DBusMessage *rsp)
{
  (void)rsp;
  return 0;
}

int
alarmd_ack_dialog(cookie_t cookie, int button)
{
  int            res = -1;
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;

  if( (msg = alarmd_ack_dialog_encode_req(cookie, button)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      res = alarmd_ack_dialog_decode_rsp(rsp);
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmd_ack_queue
 * ------------------------------------------------------------------------- */

DBusMessage *
alarmd_ack_queue_encode_req(cookie_t *cookies, int count)
{
  return client_make_method_message(ALARMD_DIALOG_ACK,
                                    DBUS_TYPE_ARRAY,
                                    DBUS_TYPE_INT32, &cookies, count,
                                    DBUS_TYPE_INVALID);
}

int
alarmd_ack_queue_decode_rsp(DBusMessage *rsp)
{
  (void)rsp;
  return 0;
}

int
alarmd_ack_queue(cookie_t *cookies, int count)
{
  int            res = -1;
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;

  if( (msg = alarmd_ack_queue_encode_req(cookies, count)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      res = alarmd_ack_queue_decode_rsp(rsp);
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmd_set_debug
 * ------------------------------------------------------------------------- */

DBusMessage *
alarmd_set_debug_encode_req(unsigned mask_set, unsigned mask_clr,
                            unsigned flag_set, unsigned flag_clr)
{
  dbus_uint32_t ms = mask_set;
  dbus_uint32_t mc = mask_clr;
  dbus_uint32_t fs = flag_set;
  dbus_uint32_t fc = flag_clr;

  return client_make_method_message("alarmd_set_debug",
                                    DBUS_TYPE_UINT32, &ms,
                                    DBUS_TYPE_UINT32, &mc,
                                    DBUS_TYPE_UINT32, &fs,
                                    DBUS_TYPE_UINT32, &fc,
                                    DBUS_TYPE_INVALID);
}

int alarmd_set_debug_decode_rsp(DBusMessage *rsp)
{
  int           res  = -1;
  dbus_uint32_t ack = 0;
  DBusError     err = DBUS_ERROR_INIT;

  if( client_parse_reply(rsp, &err,
                         DBUS_TYPE_UINT32, &ack,
                         DBUS_TYPE_INVALID) )
  {
    res = (int)ack;
  }

  if( dbus_error_is_set(&err) )
  {
    log_error_F("%s: %s\n", err.name, err.message);
  }

  dbus_error_free(&err);

  return res;
}

int
alarmd_set_debug(unsigned mask_set, unsigned mask_clr,
                 unsigned flag_set, unsigned flag_clr)
{
  int            res = -1;
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;

  if( (msg = alarmd_set_debug_encode_req(mask_set, mask_clr,
                                         flag_set, flag_clr)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      res = alarmd_set_debug_decode_rsp(rsp);
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  return res;
}

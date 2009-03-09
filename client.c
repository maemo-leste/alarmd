/* ========================================================================= *
 * File: client.c
 *
 * Copyright (C) 2008 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
 * ========================================================================= */

#include "alarmd_config.h"
#include "libalarm.h"

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
      log_error("%s: %s: %s\n", ,
                "dbus_connection_send_with_reply_and_block",
                err.name, err.message);
      goto cleanup;
    }
  }
#else
  int tmo = prsp ? -1 : 100;
  if( !(rsp = dbus_connection_send_with_reply_and_block(con, msg, tmo, &err)) )
  {
    log_error("%s: %s: %s\n",
              "dbus_connection_send_with_reply_and_block",
              err.name, err.message);
    if( prsp ) goto cleanup;
  }
#endif

  res = 0;

  cleanup:

// QUARANTINE   if( rsp ) dbusif_emit_message(rsp);

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

cookie_t
alarmd_event_update(const alarm_event_t *event)
{
  cookie_t        tag = 0;
  DBusMessage    *msg = 0;
  DBusMessage    *rsp = 0;
  DBusError       err = DBUS_ERROR_INIT;

  if( alarm_event_is_sane(event) == -1 )
  {
    goto cleanup;
  }

  if( (msg = client_make_method_message(ALARMD_EVENT_UPDATE, DBUS_TYPE_INVALID)) )
  {
    if( dbusif_encode_event(msg, event, 0) )
    {
      if( client_exec_method_call(msg, &rsp) != -1 )
      {
        dbus_int32_t val = 0;
        if( dbus_message_get_args(rsp, &err,
                                  DBUS_TYPE_INT32, &val,
                                  DBUS_TYPE_INVALID) )
        {
          tag = val;
        }
      }
    }
  }

  cleanup:

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  dbus_error_free(&err);

  return tag;
}

/* ------------------------------------------------------------------------- *
 * alarmd_event_add_valist
 * ------------------------------------------------------------------------- */

cookie_t
alarmd_event_add_valist(const alarm_event_t *event, int type, va_list va)
{
  cookie_t        tag = 0;
  DBusMessage    *msg = 0;
  DBusMessage    *rsp = 0;
  char           *arg = 0;
  DBusError       err = DBUS_ERROR_INIT;

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
    if( dbusif_encode_event(msg, event, arg) )
    {
      if( client_exec_method_call(msg, &rsp) != -1 )
      {
        dbus_int32_t val = 0;
        if( dbus_message_get_args(rsp, &err,
                                  DBUS_TYPE_INT32, &val,
                                  DBUS_TYPE_INVALID) )
        {
          tag = val;
        }
      }
    }
  }

  cleanup:

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  free(arg);
  dbus_error_free(&err);

  return tag;
}

/* ------------------------------------------------------------------------- *
 * alarmd_event_add_extra
 * ------------------------------------------------------------------------- */

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

cookie_t
alarmd_event_add(const alarm_event_t *event)
{
  return alarmd_event_add_with_dbus_params(event, DBUS_TYPE_INVALID);
}

/* ------------------------------------------------------------------------- *
 * alarmd_event_get
 * ------------------------------------------------------------------------- */

alarm_event_t *
alarmd_event_get(cookie_t cookie)
{
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;
  alarm_event_t *eve = 0;
  dbus_int32_t  tag  = cookie;

  if( (msg = client_make_method_message(ALARMD_EVENT_GET,
                                        DBUS_TYPE_INT32, &tag,
                                        DBUS_TYPE_INVALID)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      eve = dbusif_decode_event(rsp);
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  return eve;
}

/* ------------------------------------------------------------------------- *
 * alarmd_event_del
 * ------------------------------------------------------------------------- */

int
alarmd_event_del(cookie_t cookie)
{
  int            res = -1;
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;
  dbus_int32_t  tag  = cookie;
  DBusError     err  = DBUS_ERROR_INIT;

  if( (msg = client_make_method_message(ALARMD_EVENT_DEL,
                                        DBUS_TYPE_INT32, &tag,
                                        DBUS_TYPE_INVALID)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      dbus_bool_t ok = 0;

      if( dbus_message_get_args(rsp, &err,
                                DBUS_TYPE_BOOLEAN, &ok,
                                DBUS_TYPE_INVALID) )
      {
        if( ok ) res = 0;
      }
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  if( dbus_error_is_set(&err) )
  {
    log_error("%s -> %s: %s\n", __FUNCTION__, err.name, err.message);
  }

  dbus_error_free(&err);

  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmd_event_query
 * ------------------------------------------------------------------------- */

cookie_t *
alarmd_event_query(const time_t first, const time_t last,
                   int32_t flag_mask, int32_t flags, const char *appid)
{
  cookie_t      *res = 0;
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;
  DBusError     err  = DBUS_ERROR_INIT;

  dbus_int32_t  lo  = first;
  dbus_int32_t  hi  = last;
  dbus_int32_t  msk = flag_mask;
  dbus_int32_t  flg = flags;

  if( appid == 0 )
  {
    appid = "";
  }

  if( (msg = client_make_method_message(ALARMD_EVENT_QUERY,
                                        DBUS_TYPE_INT32,  &lo,
                                        DBUS_TYPE_INT32,  &hi,
                                        DBUS_TYPE_INT32,  &msk,
                                        DBUS_TYPE_INT32,  &flg,
                                        DBUS_TYPE_STRING, &appid,
                                        DBUS_TYPE_INVALID)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      dbus_int32_t  *v = 0;
      int            n = 0;

      if( dbus_message_get_args(rsp, &err,
                                DBUS_TYPE_ARRAY, DBUS_TYPE_INT32, &v, &n,
                                DBUS_TYPE_INVALID) )
      {
        res = calloc(n+1, sizeof *res);
        for( int i = 0; i < n; ++i )
        {
          res[i] = v[i];
        }
      }
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  if( dbus_error_is_set(&err) )
  {
    log_error("%s -> %s: %s\n", __FUNCTION__, err.name, err.message);
  }

  dbus_error_free(&err);

  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmd_get_default_snooze
 * ------------------------------------------------------------------------- */

unsigned int
alarmd_get_default_snooze(void)
{
  unsigned int   res = 0;
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;
  DBusError      err = DBUS_ERROR_INIT;

  if( (msg = client_make_method_message(ALARMD_SNOOZE_GET,
                                        DBUS_TYPE_INVALID)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      dbus_uint32_t dta = 0;

      if( dbus_message_get_args(rsp, &err,
                                DBUS_TYPE_UINT32, &dta,
                                DBUS_TYPE_INVALID) )
      {
        res = dta;
      }
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  if( dbus_error_is_set(&err) )
  {
    log_error("%s -> %s: %s\n", __FUNCTION__, err.name, err.message);
  }

  dbus_error_free(&err);

  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmd_set_default_snooze
 * ------------------------------------------------------------------------- */

int
alarmd_set_default_snooze(unsigned int snooze)
{
  int            res = -1;
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;
  dbus_uint32_t  tag = snooze;
  DBusError     err  = DBUS_ERROR_INIT;

  if( (msg = client_make_method_message(ALARMD_SNOOZE_SET,
                                        DBUS_TYPE_UINT32, &tag,
                                        DBUS_TYPE_INVALID)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      dbus_bool_t ok = 0;

      if( dbus_message_get_args(rsp, &err,
                                DBUS_TYPE_BOOLEAN, &ok,
                                DBUS_TYPE_INVALID) )
      {
        if( ok ) res = 0;
      }
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  if( dbus_error_is_set(&err) )
  {
    log_error("%s -> %s: %s\n", __FUNCTION__, err.name, err.message);
  }

  dbus_error_free(&err);

  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmd_ack_dialog
 * ------------------------------------------------------------------------- */

int
alarmd_ack_dialog(cookie_t cookie, int button)
{
  dbus_int32_t c = cookie;
  dbus_int32_t b = button;

  int            res = -1;
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;
  DBusError     err  = DBUS_ERROR_INIT;

  if( (msg = client_make_method_message(ALARMD_DIALOG_RSP,
                                        DBUS_TYPE_INT32, &c,
                                        DBUS_TYPE_INT32, &b,
                                        DBUS_TYPE_INVALID)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      res = 0;
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  if( dbus_error_is_set(&err) )
  {
    log_error("%s -> %s: %s\n", __FUNCTION__, err.name, err.message);
  }

  dbus_error_free(&err);

  return res;
}

/* ------------------------------------------------------------------------- *
 * alarmd_ack_queue
 * ------------------------------------------------------------------------- */

int
alarmd_ack_queue(cookie_t *cookies, int count)
{
  int            res = -1;
  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;
  if( (msg = client_make_method_message(ALARMD_DIALOG_ACK,
                                        DBUS_TYPE_ARRAY,
                                        DBUS_TYPE_INT32, &cookies, count,
                                        DBUS_TYPE_INVALID)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      res = 0;
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  return res;
}

int
alarmd_set_debug(unsigned mask_set, unsigned mask_clr,
                 unsigned flag_set, unsigned flag_clr)
{
  uint32_t ms = mask_set;
  uint32_t mc = mask_clr;
  uint32_t fs = flag_set;
  uint32_t fc = flag_clr;
  uint32_t r = 0;

  DBusMessage   *msg = 0;
  DBusMessage   *rsp = 0;
  DBusError     err  = DBUS_ERROR_INIT;

  if( (msg = client_make_method_message("alarmd_set_debug",
                                        DBUS_TYPE_UINT32, &ms,
                                        DBUS_TYPE_UINT32, &mc,
                                        DBUS_TYPE_UINT32, &fs,
                                        DBUS_TYPE_UINT32, &fc,
                                        DBUS_TYPE_INVALID)) )
  {
    if( client_exec_method_call(msg, &rsp) != -1 )
    {
      dbus_message_get_args(rsp, &err,
                            DBUS_TYPE_UINT32, &r,
                            DBUS_TYPE_INVALID);
    }
  }

  if( rsp != 0 ) dbus_message_unref(rsp);
  if( msg != 0 ) dbus_message_unref(msg);

  if( dbus_error_is_set(&err) )
  {
    log_error("%s -> %s: %s\n", __FUNCTION__, err.name, err.message);
  }

  dbus_error_free(&err);

  return r;
}

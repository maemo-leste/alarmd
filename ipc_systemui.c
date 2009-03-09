/* ========================================================================= *
 * File: ipc_systemui.c
 *
 * Copyright (C) 2008 Nokia. All rights reserved.
 *
 * Author: Simo Piiroinen <simo.piiroinen@nokia.com>
 *
 * -------------------------------------------------------------------------
 *
 * History:
 *
 * 18-Sep-2008 Simo Piiroinen
 * - initial version
 * ========================================================================= */

#include "ipc_systemui.h"

#include "logging.h"
#include "dbusif.h"

#include "systemui_dbus.h"

#ifdef DEAD_CODE
# include <stdlib.h>
#endif

static void (*systemui_ack_callback)(dbus_int32_t *vec, int cnt) = 0;

static const char *(*systemui_service_callback)(void) = 0;

/* ------------------------------------------------------------------------- *
 * systemui_set_ack_callback
 * ------------------------------------------------------------------------- */

void
systemui_set_ack_callback(void (*fn)(dbus_int32_t *, int))
{
  systemui_ack_callback = fn;
}

/* ------------------------------------------------------------------------- *
 * systemui_set_service_callback
 * ------------------------------------------------------------------------- */

void
systemui_set_service_callback(const char *(*fn)(void))
{
  systemui_service_callback = fn;
}

/* ------------------------------------------------------------------------- *
 * systemui_service_name
 * ------------------------------------------------------------------------- */

static const char *
systemui_service_name(void)
{
  if( systemui_service_callback )
  {
    return systemui_service_callback();
  }
  return SYSTEMUI_SERVICE;
}

#if 0 // NEW SYSTEMUI API - disabled for now
/* ------------------------------------------------------------------------- *
 * systemui_ack_cb
 * ------------------------------------------------------------------------- */

static void
systemui_ack_cb(DBusPendingCall *pending, void *user_data)
{
  DBusMessage *rsp = dbus_pending_call_steal_reply(pending);

  if( rsp != 0 )
  {
    DBusError     err = DBUS_ERROR_INIT;
    dbus_int32_t *vec = 0;
    int           cnt = 0;

    switch( dbus_message_get_type(rsp) )
    {
    case DBUS_MESSAGE_TYPE_METHOD_RETURN:
      if( dbus_message_get_args(rsp, &err,
                                DBUS_TYPE_ARRAY,
                                DBUS_TYPE_INT32, &vec, &cnt,
                                DBUS_TYPE_INVALID) )
      {
        if( systemui_ack_callback != 0 )
        {
          systemui_ack_callback(vec, cnt);
        }
      }
      if( dbus_error_is_set(&err) )
      {
        log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
      }
      break;

    case DBUS_MESSAGE_TYPE_ERROR:
      // TODO: handle system ui not up
      break;
    }

    dbus_error_free(&err);
    dbus_message_unref(rsp);
  }
}

/* ------------------------------------------------------------------------- *
 * systemui_add_dialog
 * ------------------------------------------------------------------------- */

int
systemui_add_dialog(DBusConnection *conn, const cookie_t *cookie, int count)
{
  return dbusif_method_call_async(conn,
                                  systemui_ack_cb, 0, 0,
                                  systemui_service_name(),
                                  SYSTEMUI_REQUEST_PATH,
                                  SYSTEMUI_REQUEST_IF,
                                  SYSTEMUI_ALARM_ADD,
                                  DBUS_TYPE_ARRAY,
                                  DBUS_TYPE_INT32, &cookie, count,
                                  DBUS_TYPE_INVALID);
}

/* ------------------------------------------------------------------------- *
 * systemui_cancel_dialog
 * ------------------------------------------------------------------------- */

int
systemui_cancel_dialog(DBusConnection *conn, const cookie_t *cookie, int count)
{
  return dbusif_method_call_async(conn,
                                  systemui_ack_cb, 0, 0,
                                  systemui_service_name(),
                                  SYSTEMUI_REQUEST_PATH,
                                  SYSTEMUI_REQUEST_IF,
                                  SYSTEMUI_ALARM_DEL,
                                  DBUS_TYPE_ARRAY,
                                  DBUS_TYPE_INT32, &cookie, count,
                                  DBUS_TYPE_INVALID);
}

/* ------------------------------------------------------------------------- *
 * systemui_query_dialog
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
int
systemui_query_dialog(DBusConnection *conn)
{
  return dbusif_method_call_async(conn,
                                  systemui_ack_cb, 0, 0,
                                  systemui_service_name(),
                                  SYSTEMUI_REQUEST_PATH,
                                  SYSTEMUI_REQUEST_IF,
                                  SYSTEMUI_ALARM_QUERY,
                                  DBUS_TYPE_INVALID);
}
#endif
#else // OLD SYSTEMUI API - back in use for now

/* ------------------------------------------------------------------------- *
 * systemui_ack_open  --  async response callback
 * ------------------------------------------------------------------------- */

static void
systemui_ack_open(DBusPendingCall *pending, void *user_data)
{
  log_debug("@%s()\n", __FUNCTION__);

  DBusMessage *rsp = dbus_pending_call_steal_reply(pending);
  if( rsp != 0 )
  {
    DBusError     err = DBUS_ERROR_INIT;
    dbus_int32_t  ack = 0;

    switch( dbus_message_get_type(rsp) )
    {
    case DBUS_MESSAGE_TYPE_METHOD_RETURN:
      if( dbus_message_get_args(rsp, &err,
                                DBUS_TYPE_INT32, &ack,
                                DBUS_TYPE_INVALID) )
      {
        log_debug("@%s() -> ACK=%d\n", __FUNCTION__, (int)ack);
// QUARANTINE         if( systemui_ack_callback != 0 )
// QUARANTINE         {
// QUARANTINE           dbus_int32_t cookie = (cookie_t)user_data;
// QUARANTINE           systemui_ack_callback(&cookie, 1);
// QUARANTINE         }
      }
      if( dbus_error_is_set(&err) )
      {
        log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
      }
      break;

    case DBUS_MESSAGE_TYPE_ERROR:
      // TODO: handle system ui not up
      break;
    }

    dbus_error_free(&err);
    dbus_message_unref(rsp);
  }
}

/* ------------------------------------------------------------------------- *
 * systemui_ack_close --  async response callback
 * ------------------------------------------------------------------------- */

static void
systemui_ack_close(DBusPendingCall *pending, void *user_data)
{
  log_debug("@%s()\n", __FUNCTION__);

  DBusMessage *rsp = dbus_pending_call_steal_reply(pending);
  if( rsp != 0 )
  {
    DBusError     err = DBUS_ERROR_INIT;
    dbus_int32_t  ack = 0;

    switch( dbus_message_get_type(rsp) )
    {
    case DBUS_MESSAGE_TYPE_METHOD_RETURN:
      if( dbus_message_get_args(rsp, &err,
                                DBUS_TYPE_INT32, &ack,
                                DBUS_TYPE_INVALID) )
      {

        log_debug("@%s() -> ack=%d\n", __FUNCTION__, (int)ack);
      }
      if( dbus_error_is_set(&err) )
      {
        log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
      }
      break;

    case DBUS_MESSAGE_TYPE_ERROR:
      // TODO: handle system ui not up
      break;
    }

    dbus_error_free(&err);
    dbus_message_unref(rsp);
  }
}

/* ------------------------------------------------------------------------- *
 * systemui_open_dialog  --  old style open dialog reques
 * ------------------------------------------------------------------------- */

static
int
systemui_open_dialog(DBusConnection *conn, cookie_t cookie)
{
  dbus_int32_t c = cookie;

  return dbusif_method_call_async(conn,
                                  systemui_ack_open, (void*)cookie,0,
                                  systemui_service_name(),
                                  SYSTEMUI_REQUEST_PATH,
                                  SYSTEMUI_REQUEST_IF,
                                  SYSTEMUI_ALARM_OPEN_REQ,
                                  DBUS_TYPE_INT32, &c,
                                  DBUS_TYPE_INVALID);
}

/* ------------------------------------------------------------------------- *
 * systemui_close_dialog  --  old style close dialog request
 * ------------------------------------------------------------------------- */

static
int
systemui_close_dialog(DBusConnection *conn, cookie_t cookie)
{
  dbus_int32_t c = cookie;

  return dbusif_method_call_async(conn,
                                  systemui_ack_close, 0, 0,
                                  systemui_service_name(),
                                  SYSTEMUI_REQUEST_PATH,
                                  SYSTEMUI_REQUEST_IF,
                                  SYSTEMUI_ALARM_CLOSE_REQ,
                                  DBUS_TYPE_INT32, &c,
                                  DBUS_TYPE_INVALID);
}

/* ------------------------------------------------------------------------- *
 * systemui_add_dialog  --  wrapper -> old style request
 * ------------------------------------------------------------------------- */

int
systemui_add_dialog(DBusConnection *conn, const cookie_t *cookie, int count)
{
  for( int i = 0; i < count; ++i )
  {
    systemui_open_dialog(conn, cookie[i]);
  }
  return 0;
}

/* ------------------------------------------------------------------------- *
 * systemui_cancel_dialog  --  wrapper -> old style request
 * ------------------------------------------------------------------------- */

int
systemui_cancel_dialog(DBusConnection *conn, const cookie_t *cookie, int count)
{
  for( int i = 0; i < count; ++i )
  {
    systemui_close_dialog(conn, cookie[i]);
  }
  return 0;
}

/* ------------------------------------------------------------------------- *
 * systemui_query_dialog  --  wrapper -> old style request
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
int
systemui_query_dialog(DBusConnection *conn)
{
  abort();
  return 0;
}
#endif

#endif

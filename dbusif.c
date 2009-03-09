#include "dbusif.h"

#include "logging.h"
#include "codec.h"

#include <stdlib.h>
#include <string.h>

#ifdef DEAD_CODE
# include <stdio.h>
#endif

#define PENDING_CALL_PARANOIA 0

/* ------------------------------------------------------------------------- *
 * dbusif_get_dtatype_name
 * ------------------------------------------------------------------------- */

const char *
dbusif_get_dtatype_name(int typecode)
{
  switch (typecode)
  {
  case DBUS_TYPE_INVALID:          return "invalid";
  case DBUS_TYPE_BOOLEAN:          return "boolean";
  case DBUS_TYPE_BYTE:             return "byte";
  case DBUS_TYPE_INT16:            return "int16";
  case DBUS_TYPE_UINT16:           return "uint16";
  case DBUS_TYPE_INT32:            return "int32";
  case DBUS_TYPE_UINT32:           return "uint32";
  case DBUS_TYPE_INT64:            return "int64";
  case DBUS_TYPE_UINT64:           return "uint64";
  case DBUS_TYPE_DOUBLE:           return "double";
  case DBUS_TYPE_STRING:           return "string";
  case DBUS_TYPE_OBJECT_PATH:      return "object_path";
  case DBUS_TYPE_SIGNATURE:        return "signature";
  case DBUS_TYPE_STRUCT:           return "struct";
  case DBUS_TYPE_DICT_ENTRY:       return "dict_entry";
  case DBUS_TYPE_ARRAY:            return "array";
  case DBUS_TYPE_VARIANT:          return "variant";
  case DBUS_STRUCT_BEGIN_CHAR:     return "begin_struct";
  case DBUS_STRUCT_END_CHAR:       return "end_struct";
  case DBUS_DICT_ENTRY_BEGIN_CHAR: return "begin_dict_entry";
  case DBUS_DICT_ENTRY_END_CHAR:   return "end_dict_entry";
  }
  return "unknown";
}

/* ------------------------------------------------------------------------- *
 * dbusif_get_msgtype_name
 * ------------------------------------------------------------------------- */

const char *
dbusif_get_msgtype_name(int typecode)
{
  const char *type = "UNKNOWN";

  switch( typecode )
  {
  case DBUS_MESSAGE_TYPE_INVALID:       type = "INVALID"; break;
  case DBUS_MESSAGE_TYPE_METHOD_CALL:   type = "METHOD"; break;
  case DBUS_MESSAGE_TYPE_METHOD_RETURN: type = "RETURN"; break;
  case DBUS_MESSAGE_TYPE_ERROR:         type = "ERROR"; break;
  case DBUS_MESSAGE_TYPE_SIGNAL:        type = "SIGNAL"; break;
  }
  return type;
}

/* ------------------------------------------------------------------------- *
 * dbusif_emitf
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
static
void
dbusif_emitf(const char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
  fflush(stdout);
}
#endif

/* ------------------------------------------------------------------------- *
 * dbusif_emit_message_data
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
static
dbus_bool_t
dbusif_emit_message_data(DBusMessageIter *src_iter, int tab)
{
  dbus_bool_t ok = TRUE;

  char   *text = 0;
  size_t  size = 0;
  FILE   *file = open_memstream(&text, &size);

  auto void tabout(void);

  auto void tabout(void)
  {
    for( int i = 0; i<tab; ++i ) fprintf(file, "    ");
  }

  ++tab;

  unsigned char  b;
  unsigned short w;
  unsigned long  l;
  unsigned long long q;
  double         d;
  char          *s;

  for( ; ; dbus_message_iter_next(src_iter) )
  {
    int etype;
    int mtype = dbus_message_iter_get_arg_type(src_iter);
    if( mtype == DBUS_TYPE_INVALID )
    {
      break;
    }

    tabout();
    fprintf(file, "%s = ", dbusif_get_dtatype_name(mtype));

    switch(mtype)
    {
    case DBUS_TYPE_BYTE:
      dbus_message_iter_get_basic(src_iter, &b);
      fprintf(file, "%u (byte)\n", b);
      break;

    case DBUS_TYPE_INT16:
    case DBUS_TYPE_UINT16:
      dbus_message_iter_get_basic(src_iter, &w);
      fprintf(file, "%u (byte)\n", w);
      break;

    case DBUS_TYPE_INT32:
    case DBUS_TYPE_UINT32:
    case DBUS_TYPE_BOOLEAN:
      dbus_message_iter_get_basic(src_iter, &l);
      fprintf(file, "%lu (long)\n", l);
      break;

    case DBUS_TYPE_INT64:
    case DBUS_TYPE_UINT64:
      dbus_message_iter_get_basic(src_iter, &q);
      fprintf(file, "%llu (quad)\n", q);
      break;

    case DBUS_TYPE_DOUBLE:
      dbus_message_iter_get_basic(src_iter, &d);
      fprintf(file, "%g (double)\n", d);
      break;
    case DBUS_TYPE_STRING:
    case DBUS_TYPE_OBJECT_PATH:
    case DBUS_TYPE_SIGNATURE:
      dbus_message_iter_get_basic(src_iter, &s);
      fprintf(file, "'%s' (string)\n", s);
      break;

    case DBUS_TYPE_ARRAY:
      etype = dbus_message_iter_get_element_type(src_iter);
      fprintf(file, "of %s\n", dbusif_get_dtatype_name(etype));
      break;

    case DBUS_TYPE_STRUCT:
      fprintf(file, "of misc\n");
      {
        DBusMessageIter iter;
        dbus_message_iter_recurse(src_iter, &iter);
        dbusif_emit_message_data(&iter, tab);
      }
      break;

    default:
      fprintf(file, "UNKNOWN\n");
      goto cleanup;
    }
  }

  cleanup:

  if( file != 0 )
  {
    char temp[256];
    rewind(file);

    while( fgets(temp, sizeof temp, file) )
    {
      dbusif_emitf("%s", temp);
    }

    fclose(file);
  }

  if( text != 0 )
  {
    free(text);
  }

  return ok;
}
#endif

/* ------------------------------------------------------------------------- *
 * dbusif_emit_message
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
void
dbusif_emit_message(DBusMessage *msg)
{
  const char *interface = dbus_message_get_interface(msg);
  const char *member    = dbus_message_get_member(msg);
  const char *object    = dbus_message_get_path(msg);
  const char *type      = "UNKNOWN";

  switch( dbus_message_get_type(msg) )
  {
  case DBUS_MESSAGE_TYPE_INVALID:       type = "INVALID"; break;
  case DBUS_MESSAGE_TYPE_METHOD_CALL:   type = "M-CALL"; break;
  case DBUS_MESSAGE_TYPE_METHOD_RETURN: type = "M-RET"; break;
  case DBUS_MESSAGE_TYPE_ERROR:         type = "ERROR"; break;
  case DBUS_MESSAGE_TYPE_SIGNAL:        type = "SIGNAL"; break;
  }

  dbusif_emitf("%s(%s, %s, %s)\n", type, interface, member, object);

  DBusMessageIter iter;
  dbus_message_iter_init(msg, &iter);
  dbusif_emit_message_data(&iter, 0);
}
#endif

/* ------------------------------------------------------------------------- *
 * dbusif_encode_event
 * ------------------------------------------------------------------------- */

dbus_bool_t
dbusif_encode_event(DBusMessage *msg, const alarm_event_t *eve, const char *args)
{
  int err = 0;

  DBusMessageIter iter;

  dbus_message_iter_init_append(msg, &iter);

  encode_event(&iter, &err, eve, &args);

  if( err == 0 && args != 0 )
  {
    log_error("%s: no action to use args for\n", __FUNCTION__);
    err = -1;
  }

  return (err == 0);
}

/* ------------------------------------------------------------------------- *
 * dbusif_decode_event
 * ------------------------------------------------------------------------- */

alarm_event_t *
dbusif_decode_event(DBusMessage *msg)
{
  alarm_event_t  *eve = alarm_event_create();
  int             err = 0;
  DBusMessageIter iter;

  dbus_message_iter_init(msg, &iter);
  decode_event(&iter, &err, eve);

  if( err != 0 )
  {
    alarm_event_delete(eve), eve = 0;
  }

  return eve;
}

/* ========================================================================= *
 * GENERIC DBUS HELPERS
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * dbusif_check_name_owner
 * ------------------------------------------------------------------------- */

int
dbusif_check_name_owner(DBusConnection *conn, const char *name)
{
  int          res = -1;
  DBusError    err = DBUS_ERROR_INIT;

  if( dbus_bus_name_has_owner(conn, name, &err) )
  {
    res = 0;
  }
  else
  {
    log_debug("%s: %s: no owner\n", __FUNCTION__, name);
    if( dbus_error_is_set(&err) )
    {
      log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
    }
  }
  dbus_error_free(&err);

  return res;
}

/* ------------------------------------------------------------------------- *
 * dbusif_add_matches
 * ------------------------------------------------------------------------- */

int
dbusif_add_matches(DBusConnection *conn, const char * const *rule)
{
  int       res = 0;
  DBusError err = DBUS_ERROR_INIT;

  for( int i = 0; rule[i]; ++i )
  {
    dbus_bus_add_match(conn, rule[i], &err);
    if( dbus_error_is_set(&err) )
    {
      log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
      res = -1;
      break;
    }
  }
  dbus_error_free(&err);
  return res;
}

/* ------------------------------------------------------------------------- *
 * dbusif_remove_matches
 * ------------------------------------------------------------------------- */

int
dbusif_remove_matches(DBusConnection *conn, const char * const *rule)
{
  int       res = 0;
  DBusError err = DBUS_ERROR_INIT;

  for( int i = 0; rule[i]; ++i )
  {
    dbus_bus_remove_match(conn, rule[i], &err);
    if( dbus_error_is_set(&err) )
    {
      log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
      res = -1;
      break;
    }
  }
  dbus_error_free(&err);
  return res;
}

/* ------------------------------------------------------------------------- *
 * dbusif_send_and_receive
 * ------------------------------------------------------------------------- */

int
dbusif_send_and_receive(DBusConnection *con, DBusMessage *msg, DBusMessage **prsp)
{
  int       res = -1;
  DBusError err = DBUS_ERROR_INIT;

  if( prsp == 0 )
  {
    dbus_message_set_no_reply(msg, TRUE);
    if( !dbus_connection_send(con, msg, NULL) )
    {
      goto cleanup;
    }
  }
  else
  {
    *prsp = dbus_connection_send_with_reply_and_block(con, msg, -1, &err);

    if( *prsp == 0 )
    {
      log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
      goto cleanup;
    }
  }

  res = 0;
  cleanup:

  dbus_error_free(&err);
  return res;
}

/* ------------------------------------------------------------------------- *
 * dbusif_send_async
 * ------------------------------------------------------------------------- */

#if PENDING_CALL_PARANOIA
static unsigned dbusif_pending_unique = 0;
static unsigned dbusif_pending_init = 0;
static unsigned dbusif_pending_call = 0;
static unsigned dbusif_pending_free = 0;

typedef struct {
  unsigned unique;
  void (*callback)(DBusPendingCall *, void *);
  void (*user_free)(void*);
  void  *user_data;

} dbusif_pending_gate_t;

static
dbusif_pending_gate_t *
dbusif_pending_gate_init(void (*callback)(DBusPendingCall *, void *),
                         void *user_data, void (*user_free)(void *))
{
  dbusif_pending_init += 1;

  dbusif_pending_gate_t *gate = calloc(1, sizeof *gate);

  gate->unique = dbusif_pending_unique++;
  gate->callback = callback;
  gate->user_data = user_data;
  gate->user_free = user_free;

  log_info("PENDING %s: ID=%u (%u - %u - %u)\n", __FUNCTION__, gate->unique,
           dbusif_pending_init, dbusif_pending_call, dbusif_pending_free);
  return gate;
}
static
void
dbusif_pending_gate_call(DBusPendingCall *pend, void *aptr)
{
  dbusif_pending_call += 1;

  dbusif_pending_gate_t *gate = aptr;
  log_info("PENDING %s: ID=%u (%u - %u - %u)\n", __FUNCTION__, gate->unique,
           dbusif_pending_init, dbusif_pending_call, dbusif_pending_free);
  gate->callback(pend, gate->user_data);
}
static
void
dbusif_pending_gate_free(void *aptr)
{
  dbusif_pending_free += 1;

  dbusif_pending_gate_t *gate = aptr;
  log_info("PENDING %s: ID=%u (%u - %u - %u)\n", __FUNCTION__, gate->unique,
           dbusif_pending_init, dbusif_pending_call, dbusif_pending_free);

  log_info("USER FREE: %p, DATA: %p\n", gate->user_free, gate->user_data);

  if( gate->user_free != 0 )
  {
    gate->user_free(gate->user_data);
  }

  free(gate);
}
#endif

int
dbusif_send_async(DBusConnection *con, DBusMessage *msg,
                  void (*cb)(DBusPendingCall *, void *),
                  void *user_data, void (*user_free)(void*))
{
  char            *fun = 0;
  int              res = -1;
  DBusPendingCall *pen = 0;
#if PENDING_CALL_PARANOIA
  dbusif_pending_gate_t *gate = 0;
#endif

  if( !dbus_connection_send_with_reply(con, msg, &pen, -1) )
  {
    log_error("%s: %s: %s\n", __FUNCTION__,
              "dbus_connection_send_with_reply", "failed");
    goto cleanup;
  }

  if( pen == 0 )
  {
    log_error("%s: %s: %s\n", __FUNCTION__,
              "dbus_connection_send_with_reply", "pending == NULL");
    goto cleanup;
  }

#if PENDING_CALL_PARANOIA
  if( user_data != 0 )
  {
    gate = dbusif_pending_gate_init(cb, user_data, user_free);
  }
  else
  {
    fun = strdup(dbus_message_get_member(msg));
    gate = dbusif_pending_gate_init(cb, fun, free);
    fun = 0;
  }

  if( gate && !dbus_pending_call_set_notify(pen,
                                    dbusif_pending_gate_call,
                                    gate,
                                    dbusif_pending_gate_free) )
  {
    log_error("%s: %s: %s\n", __FUNCTION__,
              "dbus_pending_call_set_notify", "failed");
    goto cleanup;
  }
  gate = 0;

#else
  if( user_data != 0 )
  {
    if( !dbus_pending_call_set_notify(pen, cb, user_data, user_free) )
    {
      log_error("%s: %s: %s\n", __FUNCTION__,
                "dbus_pending_call_set_notify", "failed");
      goto cleanup;
    }
  }
  else
  {
    fun = strdup(dbus_message_get_member(msg));

    if( !dbus_pending_call_set_notify(pen, cb, fun, free) )
    {
      log_error("%s: %s: %s\n", __FUNCTION__,
                "dbus_pending_call_set_notify", "failed");
      goto cleanup;
    }
    fun = 0;
  }
#endif

  res = 0;

  cleanup:

#if PENDING_CALL_PARANOIA
  if( gate != 0 )
  {
    dbusif_pending_gate_free(gate);
  }
#endif

  if( pen != 0 )
  {
    dbus_pending_call_unref(pen);
  }
  if( fun != 0 )
  {
    free(fun);
  }

  return res;
}

/* ========================================================================= *
 * METHOD RETURN MESSAGES
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * dbusif_reply_create  --  create method call response message
 * ------------------------------------------------------------------------- */

DBusMessage *
dbusif_reply_create(DBusMessage *msg, int type, ...)
{
  DBusMessage *rsp = 0;

  va_list va;

  va_start(va, type);

  if( (rsp = dbus_message_new_method_return(msg)) != 0 )
  {
    if( !dbus_message_append_args_valist(rsp, type, va) )
    {
      dbus_message_unref(rsp), rsp = 0;
    }
  }

  va_end(va);

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * dbusif_reply_parse_args
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
int
dbusif_reply_parse_args(DBusMessage *msg, int type, ...)
{
  int          res = -1;
  DBusError    err = DBUS_ERROR_INIT;
  int          ok  = 0;
  va_list      va;

  va_start(va, type);
  ok = dbus_message_get_args_valist (msg, &err, type, va);
  va_end(va);

  if( ok )
  {
    res = 0;
  }
  else
  {
    if( dbus_error_is_set(&err) )
    {
      log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
    }
    else
    {
      log_error("%s: %s: %s\n", __FUNCTION__, "Failed", "Unknown reason");
    }
  }

  dbus_error_free(&err);
  return res;
}
#endif

/* ========================================================================= *
 * DBUS METHOD CALL MESSAGES
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * dbusif_method_create_va
 * ------------------------------------------------------------------------- */

DBusMessage *
dbusif_method_create_va(const char *service, const char *object,
                        const char *interface,  const char *method,
                        int dbus_type, va_list va)
{
  DBusMessage *msg = 0;

  if( !(msg = dbus_message_new_method_call(service, object,
                                           interface, method)) )
  {
    log_error("%s: %s: %s\n", __FUNCTION__,
              "dbus_message_new_method_call", "failed");
    goto cleanup;
  }

  if( !dbus_message_append_args_valist(msg, dbus_type, va) )
  {
    log_error("%s: %s: %s\n", __FUNCTION__,
              "dbus_message_append_args_valist", "failed");
    dbus_message_unref(msg), msg = 0;
    goto cleanup;
  }

  cleanup:

  return msg;
}

/* ------------------------------------------------------------------------- *
 * dbusif_method_create
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
DBusMessage *
dbusif_method_create(const char *service, const char *object,
                     const char *interface, const char *method,
                     int dbus_type, ...)
{
  DBusMessage *msg = 0;
  va_list      va;
  va_start(va, dbus_type);
  msg = dbusif_method_create_va(service, object, interface, method, dbus_type, va);
  va_end(va);
  return msg;
}
#endif

/* ------------------------------------------------------------------------- *
 * dbusif_method_parse_args
 * ------------------------------------------------------------------------- */

DBusMessage *
dbusif_method_parse_args(DBusMessage *msg, int type, ...)
{
  DBusMessage *res = 0;
  DBusError    err = DBUS_ERROR_INIT;
  int          ok  = 0;
  va_list      va;

  va_start(va, type);
  ok = dbus_message_get_args_valist (msg, &err, type, va);
  va_end(va);

  if( ok == 0 )
  {
    res = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS,
                                 dbus_message_get_member(msg));
  }

  if( dbus_error_is_set(&err) )
  {
    log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
  }

  dbus_error_free(&err);
  return res;
}

/* ------------------------------------------------------------------------- *
 * dbusif_method_call
 * ------------------------------------------------------------------------- */

int
dbusif_method_call(DBusConnection *con,   DBusMessage **prsp,
                   const char *service,   const char *object,
                   const char *interface, const char *method,
                   int dbus_type, ...)
{
  int          res = -1;
  DBusMessage *msg = 0;
  va_list      va;

  va_start(va, dbus_type);
  msg = dbusif_method_create_va(service, object, interface, method, dbus_type, va);
  va_end(va);

  if( msg != 0 )
  {
    res = dbusif_send_and_receive(con, msg, prsp);
  }

  if( msg != 0 )
  {
    dbus_message_unref(msg), msg = 0;
  }

  return res;
}

/* ------------------------------------------------------------------------- *
 * dbusif_method_call_async
 * ------------------------------------------------------------------------- */

int
dbusif_method_call_async(DBusConnection *con,
                         void (*cb)(DBusPendingCall *, void *),
                         void *user_data, void (*user_free)(void *),
                         const char *service,   const char *object,
                         const char *interface, const char *method,
                         int dbus_type, ...)
{
  int          res = -1;
  DBusMessage *msg = 0;
  va_list      va;

  va_start(va, dbus_type);
  msg = dbusif_method_create_va(service, object, interface, method, dbus_type, va);
  va_end(va);

  if( msg != 0 )
  {
    res = dbusif_send_async(con, msg, cb, user_data, user_free);
  }

  if( msg != 0 )
  {
    dbus_message_unref(msg), msg = 0;
  }

  return res;
}

/* ========================================================================= *
 * DBUS SIGNAL MESSAGES
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * dbusif_signal_create_va
 * ------------------------------------------------------------------------- */

DBusMessage *
dbusif_signal_create_va(const char *object,
                        const char *interface,
                        const char *method,
                        int dbus_type, va_list va)
{
  DBusMessage *msg = 0;

  if( !(msg = dbus_message_new_signal(object, interface, method)) )
  {
    log_error("%s: %s: %s\n", __FUNCTION__,
              "dbus_message_new_signal", "failed");
    goto cleanup;
  }

  if( !dbus_message_append_args_valist(msg, dbus_type, va) )
  {
    log_error("%s: %s: %s\n", __FUNCTION__,
              "dbus_message_append_args_valist", "failed");
    dbus_message_unref(msg), msg = 0;
    goto cleanup;
  }

  cleanup:

  return msg;
}

/* ------------------------------------------------------------------------- *
 * dbusif_signal_create
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
DBusMessage *
dbusif_signal_create(const char *object,
                     const char *interface,
                     const char *method,
                     int dbus_type, ...)
{
  DBusMessage *msg = 0;
  va_list      va;
  va_start(va, dbus_type);
  msg = dbusif_signal_create_va(object, interface, method, dbus_type, va);
  va_end(va);
  return msg;
}
#endif

/* ------------------------------------------------------------------------- *
 * dbusif_signal_parse_args
 * ------------------------------------------------------------------------- */

#ifdef DEAD_CODE
int
dbusif_signal_parse_args(DBusMessage *msg, int type, ...)
{
  int          res = -1;
  DBusError    err = DBUS_ERROR_INIT;
  int          ok  = 0;
  va_list      va;

  va_start(va, type);
  ok = dbus_message_get_args_valist (msg, &err, type, va);
  va_end(va);

  if( ok )
  {
    res = 0;
  }
  else
  {
    if( dbus_error_is_set(&err) )
    {
      log_error("%s: %s: %s\n", __FUNCTION__, err.name, err.message);
    }
    else
    {
      log_error("%s: %s: %s\n", __FUNCTION__, "Failed", "Unknown reason");
    }
  }

  dbus_error_free(&err);
  return res;
}
#endif

/* ------------------------------------------------------------------------- *
 * dbusif_signal_send
 * ------------------------------------------------------------------------- */

int
dbusif_signal_send(DBusConnection *con,   const char *object,
                   const char *interface, const char *method,
                   int dbus_type, ...)
{
  int          res = -1;
  DBusMessage *msg = 0;
  va_list      va;

  va_start(va, dbus_type);
  msg = dbusif_signal_create_va(object, interface, method, dbus_type, va);
  va_end(va);

  if( msg == 0 )
  {
    log_error("%s: %s: %s\n", __FUNCTION__, method, "Signal could not be created");
  }
  else if( !dbus_connection_send(con, msg, NULL) )
  {
    log_error("%s: %s: %s\n", __FUNCTION__, method, "Signal could not be sent");
  }
  else
  {
    log_debug("%s: %s: %s\n", __FUNCTION__, method, "Signal sent");
    res = 0;
  }

  if( msg != 0 )
  {
    dbus_message_unref(msg), msg = 0;
  }

  return res;
}

/* ------------------------------------------------------------------------- *
 * dbusif_handle_message_by_member
 * ------------------------------------------------------------------------- */

DBusMessage *
dbusif_handle_message_by_member(const dbusif_method_lut *lut, DBusMessage *msg)
{
  const char  *member = dbus_message_get_member(msg);
  int          type   = dbus_message_get_type(msg);
  DBusMessage *rsp    = 0;

  for( int i = 0; ; ++i )
  {
    if( lut[i].member == 0 )
    {
      log_error("%s: %s: %s\n", __FUNCTION__, member, "unknown member");

      if( type == DBUS_MESSAGE_TYPE_METHOD_CALL )
      {
        rsp = dbus_message_new_error(msg, DBUS_ERROR_UNKNOWN_METHOD, member);
      }
      break;
    }
    if( !strcmp(lut[i].member, member) )
    {
      rsp = lut[i].callback(msg);
      break;
    }
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * if no response message was created
   * above and the peer expects reply,
   * create a generic error message
   * - - - - - - - - - - - - - - - - - - - */

  if( rsp == 0 && type == DBUS_MESSAGE_TYPE_METHOD_CALL )
  {
    if( !dbus_message_get_no_reply(msg) )
    {
      rsp = dbus_message_new_error(msg, DBUS_ERROR_FAILED, member);
    }
  }

  return rsp;
}

/* ------------------------------------------------------------------------- *
 * dbusif_handle_message_by_interface
 * ------------------------------------------------------------------------- */

int
dbusif_handle_message_by_interface(const dbusif_interface_lut *filt,
                                   DBusConnection *conn,
                                   DBusMessage *msg)
{
  DBusHandlerResult   result    = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  const char         *interface = dbus_message_get_interface(msg);
  const char         *member    = dbus_message_get_member(msg);
  const char         *object    = dbus_message_get_path(msg);
  int                 type      = dbus_message_get_type(msg);
  DBusMessage        *rsp       = 0;

#if ENABLE_LOGGING >= 3
  const char         *typestr   = dbusif_get_msgtype_name(type);
// QUARANTINE   log_debug("\n");
// QUARANTINE   log_debug("----------------------------------------------------------------\n");
  log_debug("@ %s: %s(%s, %s, %s)\n", __FUNCTION__, typestr, object, interface, member);
#endif

  if( !interface || !member || !object )
  {
    goto cleanup;
  }

  for( int i = 0; filt[i].interface; ++i )
  {
    if( filt[i].type != type )
    {
      continue;
    }
    if( strcmp(filt[i].interface, interface) )
    {
      continue;
    }
    if( strcmp(filt[i].object, object) )
    {
      continue;
    }

    rsp = dbusif_handle_message_by_member(filt[i].callbacks, msg);
    result = filt[i].result;
    break;
  }

  /* - - - - - - - - - - - - - - - - - - - *
   * send response if we have something
   * to send
   * - - - - - - - - - - - - - - - - - - - */

  if( rsp != 0 )
  {
    dbus_connection_send(conn, rsp, 0);
    //dbus_connection_flush(conn);
  }

  cleanup:

  if( rsp != 0 )
  {
    dbus_message_unref(rsp);
  }

// QUARANTINE   log_debug("----------------------------------------------------------------\n");
// QUARANTINE   log_debug("\n");

  return result;
}

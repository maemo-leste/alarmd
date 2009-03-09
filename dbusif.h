#ifndef DBUSIF_H_
#define DBUSIF_H_

#include <dbus/dbus.h>

#include "libalarm.h"

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

/* ------------------------------------------------------------------------- *
 * Generic Functions
 * ------------------------------------------------------------------------- */

const char    *dbusif_get_msgtype_name (int typecode);
const char    *dbusif_get_dtatype_name(int typecode);

void           dbusif_emit_message     (DBusMessage *msg);
dbus_bool_t    dbusif_encode_event     (DBusMessage *msg, const alarm_event_t *eve, const char *args);
alarm_event_t *dbusif_decode_event     (DBusMessage *msg);
int            dbusif_check_name_owner (DBusConnection *conn, const char *name);
int            dbusif_add_matches      (DBusConnection *conn, const char *const *rule);
int            dbusif_remove_matches   (DBusConnection *conn, const char *const *rule);
int            dbusif_send_and_receive (DBusConnection *con, DBusMessage *msg, DBusMessage **prsp);
DBusMessage   *dbusif_reply_create     (DBusMessage *msg, int type, ...);
int            dbusif_reply_parse_args (DBusMessage *msg, int type, ...);
DBusMessage   *dbusif_method_create_va (const char *service, const char *object, const char *interface, const char *method, int dbus_type, va_list va);
DBusMessage   *dbusif_method_create    (const char *service, const char *object, const char *interface, const char *method, int dbus_type, ...);
DBusMessage   *dbusif_method_parse_args(DBusMessage *msg, int type, ...);
int            dbusif_method_call      (DBusConnection *con, DBusMessage **prsp, const char *service, const char *object, const char *interface, const char *method, int dbus_type, ...);
DBusMessage   *dbusif_signal_create_va (const char *object, const char *interface, const char *method, int dbus_type, va_list va);
DBusMessage   *dbusif_signal_create    (const char *object, const char *interface, const char *method, int dbus_type, ...);
int            dbusif_signal_parse_args(DBusMessage *msg, int type, ...);
int            dbusif_signal_send      (DBusConnection *con, const char *object, const char *interface, const char *method, int dbus_type, ...);

int
dbusif_method_call_async(DBusConnection *con,
                         void (*cb)(DBusPendingCall *, void *),
                         void *user_data, void (*user_free)(void *),
                         const char *service,   const char *object,
                         const char *interface, const char *method,
                         int dbus_type, ...);

int
dbusif_send_async(DBusConnection *con, DBusMessage *msg,
                  void (*cb)(DBusPendingCall *, void *),
                  void *user_data, void (*user_free)(void*));

/* ------------------------------------------------------------------------- *
 * Message Handling Lookup Tables
 * ------------------------------------------------------------------------- */

typedef struct dbusif_method_lut    dbusif_method_lut;
typedef struct dbusif_interface_lut dbusif_interface_lut;

struct dbusif_method_lut
{
  const char    *member;
  DBusMessage *(*callback)(DBusMessage *);
};

struct dbusif_interface_lut
{
  const char              *interface;
  const char              *object;
  int                      type;
  const dbusif_method_lut *callbacks;
  int                      result;
};

DBusMessage *dbusif_handle_message_by_member(const dbusif_method_lut *lut, DBusMessage *msg);
int dbusif_handle_message_by_interface(const dbusif_interface_lut *filt, DBusConnection *conn, DBusMessage *msg);

#ifdef __cplusplus
};
#endif

#endif /* DBUSIF_H_ */

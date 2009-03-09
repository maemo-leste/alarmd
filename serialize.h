#ifndef SERIALIZE_H_
#define SERIALIZE_H_

#include <stdarg.h>
#include <dbus/dbus.h>

#ifdef __cplusplus
extern "C" {
#elif 0
} /* fool JED indentation ... */
#endif

int   serialize_unpack_to_mesg(const char *args, DBusMessage *msg);
char *serialize_pack_dbus_args(int type, va_list va);

#ifdef __cplusplus
};
#endif

#endif /* SERIALIZE_H_ */

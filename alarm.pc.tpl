prefix=@PREFIX@
exec_prefix=@BINDIR@
libdir=@LIBDIR@
includedir=@INCDIR@

Name: alarm
Description: alarmd client library
Version: @VERS@
Requires: dbus-1
Requires.private: 
Cflags: -I@INCDIR@
Libs: -L@LIBDIR@ -lalarm
Libs.private: -ltime -lrt

# -*- mode: sh -*-

CPPFLAGS += -DENABLE_LOGGING=3

TARGETS += fakeui-gtk
TARGETS += scrumdemo

PKG_NAMES    := gtk+-2.0 dbus-glib-1
PKG_CFLAGS   := $(shell pkg-config --cflags $(PKG_NAMES))
PKG_LDLIBS   := $(shell pkg-config --libs   $(PKG_NAMES))

PKG_CPPFLAGS := $(PKG_CPPFLAGS) $(filter -D%,$(PKG_CFLAGS))
PKG_CPPFLAGS := $(PKG_CPPFLAGS) $(filter -I%,$(PKG_CFLAGS))

PKG_CFLAGS   := $(filter-out -D%, $(PKG_CFLAGS))
PKG_CFLAGS   := $(filter-out -I%, $(PKG_CFLAGS))

.PHONY: build clean distclean mostlyclean

build:: $(TARGETS)

clean:: mostlyclean
	$(RM) $(TARGETS)

distclean:: clean

mostlyclean::
	$(RM) *.o *~

ifeq ($(wildcard /usr/include/clockd/libtime.h),)
CPPFLAGS += -DHAVE_LIBTIME=0
else
CPPFLAGS += -DHAVE_LIBTIME=1
LDLIBS   += -ltime
endif

CPPFLAGS += -I..
CPPFLAGS += -D_GNU_SOURCE
CPPFLAGS += $(PKG_CPPFLAGS)

CFLAGS   += -Wall -Wmissing-prototypes
CFLAGS   += -std=c99
CFLAGS   += -Werror
CFLAGS   += -Os
CFLAGS   += -g
CFLAGS   += $(PKG_CFLAGS)

LDFLAGS  += -g

LDLIBS   += -Wl,--as-needed
LDLIBS   += $(PKG_LDLIBS)

fakeui-gtk : LDLIBS += ../libalarm.a -lrt
fakeui-gtk : fakeui-gtk.o

scrumdemo : LDLIBS += ../libalarm.a -lrt
scrumdemo : scrumdemo.o

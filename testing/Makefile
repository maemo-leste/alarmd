# -*- mode: sh -*-

TARGETS += fakesb
TARGETS += fakedsme
TARGETS += skeleton
TARGETS += scrumdemo
TARGETS += test_recurr
TARGETS += asynctest

# ----------------------------------------------------------------------------
# Default flags
# ----------------------------------------------------------------------------

CFLAGS   += -Wall
CFLAGS   += -std=c99

CPPFLAGS += -D_GNU_SOURCE
CPPFLAGS += -DENABLE_LOGGING=3

LDLIBS   += -Wl,--as-needed
LDLIBS   += ../libalarm.a -ltime

# ----------------------------------------------------------------------------
# Flags from pkg-config
# ----------------------------------------------------------------------------

PKG_NAMES := \
 gtk+-2.0 \
 dbus-glib-1 \
 dsme_dbus_if \
 statusbar-alarm

PKG_CONFIG   ?= pkg-config
PKG_CFLAGS   := $(shell $(PKG_CONFIG) --cflags $(PKG_NAMES))
PKG_LDLIBS   := $(shell $(PKG_CONFIG) --libs   $(PKG_NAMES))
PKG_CPPFLAGS := $(filter -D%,$(PKG_CFLAGS)) $(filter -I%,$(PKG_CFLAGS))
PKG_CFLAGS   := $(filter-out -I%, $(filter-out -D%, $(PKG_CFLAGS)))

CFLAGS   += $(PKG_CFLAGS)
CPPFLAGS += $(PKG_CPPFLAGS)
LDLIBS   += $(PKG_LDLIBS)

# ----------------------------------------------------------------------------
# Top level targets
# ----------------------------------------------------------------------------

.PHONY: build clean distclean mostlyclean

build:: $(TARGETS)

clean:: mostlyclean
	$(RM) $(TARGETS)

distclean:: clean

mostlyclean::
	$(RM) *.o *~

# ----------------------------------------------------------------------------
# Build deps
# ----------------------------------------------------------------------------

fakedsme.o    : fakedsme.c
fakesb.o      : fakesb.c
scrumdemo.o   : scrumdemo.c
skeleton.o    : skeleton.c
test_recurr.o : test_recurr.c
asynctest.o   : asynctest.c

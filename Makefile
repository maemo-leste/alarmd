# Makefile for alarmd
# Copyright (C) 2006-2007 Nokia Corporation.
# Written by David Weinehall
#
# alarmd and libalarm are distributed in the hope that they will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this software; if not, write to the Free
# Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301 USA

VERSION := $(shell head -n1 debian/changelog | sed -e 's/^.*(\([^-]*\)\(-.*\)*).*$$/\1/')
LIBVER := 0:0:0

LIBTOOL := libtool --tag=BINCC

INSTALL := install -o root -g root --mode=755
INSTALL_DIR := install -d
INSTALL_DATA := install -o root -g root --mode=644

DOXYGEN := doxygen

PCDIR := $(DESTDIR)/usr/lib/pkgconfig
VARDIR := $(DESTDIR)/var/lib/alarmd
BINDIR := $(DESTDIR)/usr/bin
SBINDIR := $(DESTDIR)/sbin
LIBDIR := $(DESTDIR)/usr/lib
PLUGINDIR := $(DESTDIR)/usr/lib/alarmd
INCLUDEDIR := $(DESTDIR)/usr/include/alarmd
BACKUPDIR := $(DESTDIR)/etc/osso-backup/applications
CUDDIR := $(DESTDIR)/etc/osso-cud-scripts

TOPDIR := $(shell /bin/pwd)
DOCDIR := $(TOPDIR)/doc

PACKAGE=alarmd
TARGETS=alarmd libalarm.la libgtimeout.la libretu.la alarmtool apitest

WARNINGS += -Werror
WARNINGS += -W -Wall -Wpointer-arith -Wundef -Wcast-align
WARNINGS += -Wbad-function-cast -Wwrite-strings -Wsign-compare
WARNINGS += -Waggregate-return -Wmissing-noreturn -Wnested-externs
WARNINGS += -Wchar-subscripts -Wformat-security -Wmissing-prototypes
WARNINGS += -Wformat=2
#WARNINGS += -Wshadow

#CFLAGS = -g -O0
#LDFLAGS = -g
#CFLAGS += -DDEBUG_FUNC
CFLAGS += -DOSSOLOG_COMPILE
CFLAGS += $(WARNINGS)

alarmd_CFLAGS += `pkg-config glib-2.0 gobject-2.0 gmodule-2.0 dbus-1 libxml-2.0 libosso conic mce osso-systemui-dbus --cflags` -DG_MODULE \
		-DPLUGINDIR=\"$(PLUGINDIR)\" -DPACKAGE=\"$(PACKAGE)\" -DVERSION=\"$(VERSION)\" -DDATADIR=\"$(VARDIR)\" \
		-DDBUS_API_SUBJECT_TO_CHANGE
alarmd_LDFLAGS += `pkg-config glib-2.0 gobject-2.0 gmodule-2.0 dbus-1 libxml-2.0 libosso conic mce osso-systemui-dbus --libs` -export-dynamic

alarmd_SOURCES := alarmd.c event.c action.c object.c xmlobjectfactory.c \
		queue.c initialize.c timer-loader.c debug.c rpc-systemui.c \
		rpc-dbus.c actiondbus.c dbusobjectfactory.c rpc-osso.c \
		actionexec.c actiondialog.c rpc-statusbar.c \
		eventrecurring.c rpc-ic.c rpc-mce.c
alarmd_HEADERS := alarmd.h include/alarm_event.h event.h action.h \
		object.h queue.h xml-common.h rpc-osso.h \
		xmlobjectfactory.h initialize.h \
		timer-loader.h debug.h rpc-systemui.h \
		rpc-dbus.h actiondbus.h dbusobjectfactory.h \
		actionexec.h actiondialog.h rpc-statusbar.h \
		eventrecurring.h rpc-ic.h include/timer-interface.h \
		rpc-mce.h
alarmd_BACKUP := alarmd.conf
alarmd_CUD := alarmd.sh
alarmd_BACKUPDIR := $(BACKUPDIR)
alarmd_CUDDIR := $(CUDDIR)

alarmd_DIR := $(BINDIR)

libgtimeout_la_CFLAGS += `pkg-config glib-2.0 --cflags`
libgtimeout_la_LDFLAGS += `pkg-config glib-2.0 --libs` -module -avoid-version -rpath $(PLUGINDIR)

libgtimeout_la_SOURCES := timer-gtimeout.c
libgtimeout_la_HEADERS := include/timer-interface.h debug.h
libgtimeout_la_DIR := $(PLUGINDIR)

libretu_la_CFLAGS += `pkg-config glib-2.0 --cflags`
libretu_la_LDFLAGS += `pkg-config glib-2.0 --libs` -module -avoid-version -rpath $(PLUGINDIR)

libretu_la_SOURCES := timer-retu.c rpc-retutime.c
libretu_la_HEADERS := include/timer-interface.h debug.h rpc-retutime.h
libretu_la_DIR := $(PLUGINDIR)

libalarm_la_CFLAGS += `pkg-config dbus-1 --cflags` -DDBUS_API_SUBJECT_TO_CHANGE
libalarm_la_LDFLAGS += `pkg-config dbus-1 --libs` -rpath $(LIBDIR) -version-info $(LIBVER)

libalarm_la_SOURCES := alarm_event.c
libalarm_la_HEADERS := include/alarm_event.h include/alarm_dbus.h
libalarm_la_PC := libalarm.pc
libalarm_la_PCDIR := $(PCDIR)

libalarm_la_DIR := $(LIBDIR)
libalarm_la_INCLUDES := include/alarm_event.h include/alarm_dbus.h \
			include/timer-interface.h
libalarm_la_INCLUDEDIR := $(INCLUDEDIR)

alarmtool_LDFLAGS += libalarm.la
alarmtool_DIR := $(BINDIR)
alarmtool_SOURCES := alarmtool.c

apitest_CFLAGS += -I`pwd`/include
apitest_DIR := /usr/share/alarmd/tests
apitest_LDFLAGS += libalarm.la
apitest_SOURCES := tests/apitest.c

.PHONY: all
all: $(patsubst %, %.build, ${TARGETS})

$(PART): $(patsubst %.c, %.lo, ${$(subst .,_,${PART})_SOURCES}) ${$(subst .,_,${PART})_PC} ${$(subst .,_,${PART})_BACKUP} ${$(subst .,_,${PART})_CUD}
	@echo -n "  Linking..."
	@$(LIBTOOL) --quiet --mode=link $(CC) $(patsubst %.c, %.lo, ${$(subst .,_,${PART})_SOURCES}) -o $@ ${$(subst .,_,${PART})_LDFLAGS}
	@echo "done."

%.build:
	@echo "Target $(patsubst %.build,%,$@)..."
	@$(MAKE) LIBDIR="$(LIBDIR)" PLUGINDIR="$(PLUGINDIR)" -s PART="$(patsubst %.build,%,$@)" $(patsubst %.build,%,$@)

%.lo: %.c $($(subst .,_,${PART})_HEADERS)
	@echo -n "  Building $@..."
	@$(LIBTOOL) --quiet --mode=compile $(CC) $(CFLAGS) $($(subst .,_,${PART})_CFLAGS) -c $< -o $@
	@echo "done."
	

.PHONY: doc
doc:
	$(MAKE) -C doc PACKAGE=$(PACKAGE)	

.PHONY: clean
clean: $(patsubst %, %.clean, ${TARGETS})
	@rm -f core

%.clean:
	@libtool --quiet --mode=clean rm -f $(patsubst %.c, %.lo, ${$(subst .,_,$(patsubst %.clean,%,$@))_SOURCES})

.PHONY: distclean
distclean: clean
	@libtool --quiet --mode=clean rm -f ${TARGETS}
	@rm -f *~

.PHONY: install
install: $(patsubst %, %.install, ${TARGETS})

%.install: %.build
	@$(INSTALL_DIR) ${$(subst .,_,$(patsubst %.install,%,$@))_DIR}
	@libtool --quiet --mode=install $(INSTALL) $(patsubst %.install,%,$@) ${$(subst .,_,$(patsubst %.install,%,$@))_DIR}
	@if [ x != "x${$(subst .,_,$(patsubst %.install,%,$@))_PC}" ]; then $(INSTALL_DIR) ${$(subst .,_,$(patsubst %.install,%,$@))_PCDIR}; $(INSTALL_DATA) ${$(subst .,_,$(patsubst %.install,%,$@))_PC} ${$(subst .,_,$(patsubst %.install,%,$@))_PCDIR}; fi
	@if [ x != "x${$(subst .,_,$(patsubst %.install,%,$@))_BACKUP}" ]; then $(INSTALL_DIR) ${$(subst .,_,$(patsubst %.install,%,$@))_BACKUPDIR}; $(INSTALL_DATA) ${$(subst .,_,$(patsubst %.install,%,$@))_BACKUP} ${$(subst .,_,$(patsubst %.install,%,$@))_BACKUPDIR}; fi
	@if [ x != "x${$(subst .,_,$(patsubst %.install,%,$@))_CUD}" ]; then $(INSTALL_DIR) ${$(subst .,_,$(patsubst %.install,%,$@))_CUDDIR}; $(INSTALL) ${$(subst .,_,$(patsubst %.install,%,$@))_CUD} ${$(subst .,_,$(patsubst %.install,%,$@))_CUDDIR}; fi
	@if [ x != "x${$(subst .,_,$(patsubst %.install,%,$@))_INCLUDES}" ]; then $(INSTALL_DIR) ${$(subst .,_,$(patsubst %.install,%,$@))_INCLUDEDIR}; $(INSTALL_DATA) ${$(subst .,_,$(patsubst %.install,%,$@))_INCLUDES} ${$(subst .,_,$(patsubst %.install,%,$@))_INCLUDEDIR}; fi

%: %.in Makefile debian/changelog
	@cat $< | sed -e 's!@LIBDIR@!$(LIBDIR)!g; s!@INCLUDEDIR@!$(INCLUDEDIR)!g; s/@VERSION@/$(VERSION)/g; s!@VARDIR@!$(VARDIR)!g;' > $@
%.sh: %.sh.in Makefile debian/changelog
	@cat $< | sed -e 's!@LIBDIR@!$(LIBDIR)!g; s!@INCLUDEDIR@!$(INCLUDEDIR)!g; s/@VERSION@/$(VERSION)/g; s!@VARDIR@!$(VARDIR)!g;' > $@

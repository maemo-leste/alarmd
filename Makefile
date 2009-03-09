# -*- mode: sh -*-

VERS := $(shell head -1 debian/changelog  | sed -e 's:^.*(::' -e 's:).*::')
NAME ?= alarmd
ROOT ?= /tmp/test-$(NAME)

PREFIX    ?= /usr
BINDIR    ?= $(PREFIX)/bin
SBINDIR   ?= $(PREFIX)/sbin
DLLDIR    ?= $(PREFIX)/lib
LIBDIR    ?= $(PREFIX)/lib
INCDIR    ?= $(PREFIX)/include/$(NAME)

DOCDIR    ?= $(PREFIX)/share/doc/$(NAME)
MANDIR    ?= $(PREFIX)/share/man

CACHEDIR  ?= /var/cache/alarmd
DEVDOCDIR ?= $(PREFIX)/share/doc/libalarm-doc
PKGCFGDIR ?= $(PREFIX)/lib/pkgconfig

EVENT_DIR    ?= /etc/event.d
XSESSION_DIR ?= /etc/X11/Xsession.d
BACKUP_DIR   ?= /etc/osso-backup

CUD_DIR      ?= /etc/osso-cud-scripts
RFS_DIR      ?= /etc/osso-rfs-scripts

DOXYWORK = doxyout

SO ?= .so.2
# Note: if so-version is changed, you need also to edit
# - debian/control: fix libalarmN package names
# - debian/rules:   fix libalarmN package names and install dirs

TEMPLATE_COPY = sed\
 -e 's:@NAME@:${NAME}:g'\
 -e 's:@VERS@:${VERS}:g'\
 -e 's:@ROOT@:${ROOT}:g'\
 -e 's:@PREFIX@:${PREFIX}:g'\
 -e 's:@BINDIR@:${BINDIR}:g'\
 -e 's:@LIBDIR@:${LIBDIR}:g'\
 -e 's:@DLLDIR@:${DLLDIR}:g'\
 -e 's:@INCDIR@:${INCDIR}:g'\
 -e 's:@DOCDIR@:${DOCDIR}:g'\
 -e 's:@MANDIR@:${MANDIR}:g'\
 -e 's:@SBINDIR@:${SBINDIR}:g'\
 -e 's:@CACHEDIR@:${CACHEDIR}:g'\
 -e 's:@DEVDOCDIR@:${DEVDOCDIR}:g'\
 -e 's:@PKGCFGDIR@:${PKGCFGDIR}:g'\
 -e 's:@BACKUP_DIR@:${BACKUP_DIR}:g'\
 < $< > $@

# ----------------------------------------------------------------------------
# PKG CONFIG does not separate CPPFLAGS from CFLAGS -> we need them properly
#            separated so that various optional preprosessing stuff works
# ----------------------------------------------------------------------------

PKG_NAMES := \
 glib-2.0 \
 dbus-glib-1 \
 dbus-1 \
 conic \
 osso-systemui-dbus \
 mce \
 statusbar-alarm

PKG_CONFIG   ?= pkg-config
PKG_CFLAGS   := $(shell $(PKG_CONFIG) --cflags $(PKG_NAMES))
PKG_LDLIBS   := $(shell $(PKG_CONFIG) --libs   $(PKG_NAMES))
PKG_CPPFLAGS := $(filter -D%,$(PKG_CFLAGS)) $(filter -I%,$(PKG_CFLAGS))
PKG_CFLAGS   := $(filter-out -I%, $(filter-out -D%, $(PKG_CFLAGS)))
# ----------------------------------------------------------------------------
# Global Flags
# ----------------------------------------------------------------------------

CPPFLAGS += -DENABLE_LOGGING=3

CPPFLAGS += -DVERS='"${VERS}"'
CPPFLAGS += -D_GNU_SOURCE

CFLAGS   += -Wall -Wmissing-prototypes
CFLAGS   += -std=c99
CFLAGS   += -Os
CFLAGS   += -g
CFLAGS   += -Werror

LDFLAGS  += -g

LDLIBS   += -Wl,--as-needed

CPPFLAGS += $(PKG_CPPFLAGS)
CFLAGS   += $(PKG_CFLAGS)
LDLIBS   += $(PKG_LDLIBS)

## QUARANTINE CPPFLAGS += -DDEAD_CODE
## QUARANTINE CFLAGS   += -Wno-unused-function

#CFLAGS  += -fgnu89-inline

USE_LIBTIME ?= y#

ifeq ($(wildcard /usr/include/clockd/libtime.h),)
CPPFLAGS += -DHAVE_LIBTIME=0
CPPFLAGS += -DUSE_LIBTIME=0
else
CPPFLAGS += -DHAVE_LIBTIME=1
ifeq ($(USE_LIBTIME),y)
CPPFLAGS += -DUSE_LIBTIME=1
LDLIBS   += -ltime
endif
endif

# ----------------------------------------------------------------------------
# Top Level Targets
# ----------------------------------------------------------------------------

TARGETS += libalarm.a
TARGETS += libalarm$(SO)
TARGETS += alarmd
TARGETS += alarmclient
TARGETS += alarmtool

FLOW_GRAPHS = $(foreach e,.png .pdf .eps,\
		  $(addsuffix $e,$(addprefix $1,.fun .mod .api .top)))

EXTRA += $(call FLOW_GRAPHS, alarmclient)
EXTRA += $(call FLOW_GRAPHS, alarmd)

.PHONY: build clean distclean mostlyclean install

build:: $(TARGETS)

extra:: $(EXTRA)

all:: build extra

clean:: mostlyclean
	$(RM) $(TARGETS) $(EXTRA)

distclean:: clean

mostlyclean::
	$(RM) *.o *~

install:: $(addprefix install-,alarmd alarmclient libalarm libalarm-dev libalarm-doc)

.PHONY: dox
dox:	; doxygen
#dox:	; doxygen 2>&1 1>out | ./dox.py

distclean::
	$(RM) -r $(DOXYWORK)

# ----------------------------------------------------------------------------
# Pattern rules
# ----------------------------------------------------------------------------

%.pc    : %.pc.tpl Makefile ; $(TEMPLATE_COPY)
%.sh    : %.sh.tpl Makefile ; $(TEMPLATE_COPY)
%.h     : %.h.tpl  Makefile ; $(TEMPLATE_COPY)
%       : %.tpl    Makefile ; $(TEMPLATE_COPY)

%.dot   : %.dot.manual   ; cp $< $@
%.cflow : %.cflow.manual ; cp $< $@

%       : %.o ; $(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%$(SO): LDFLAGS += -shared -Wl,-soname,$@

%$(SO):
	$(CC) -o $@  $^ $(LDFLAGS) $(LDLIBS)

%.a:
	$(AR) ru $@ $^

%.pic.o : CFLAGS += -fPIC
%.pic.o : CFLAGS += -fvisibility=hidden
%.pic.o : %.c
	@echo "Compile: dynamic: $<"
	@$(CC) -o $@ -c $< $(CPPFLAGS) $(CFLAGS)

%.o     : %.c
	@echo "Compile: static: $<"
	@$(CC) -o $@ -c $< $(CPPFLAGS) $(CFLAGS)

install-%-man1:
	$(if $<, install -m755 -d $(ROOT)$(MANDIR)/man1)
	$(if $<, install -m644 $^ $(ROOT)$(MANDIR)/man1)
install-%-man8:
	$(if $<, install -m755 -d $(ROOT)$(MANDIR)/man8)
	$(if $<, install -m644 $^ $(ROOT)$(MANDIR)/man8)

install-%-bin:
	$(if $<, install -m755 -d $(ROOT)$(BINDIR))
	$(if $<, install -m755 $^ $(ROOT)$(BINDIR))

install-%-sbin:
	$(if $<, install -m755 -d $(ROOT)$(SBINDIR))
	$(if $<, install -m755 $^ $(ROOT)$(SBINDIR))

install-%-lib:
	$(if $<, install -m755 -d $(ROOT)$(LIBDIR))
	$(if $<, install -m755 $^ $(ROOT)$(LIBDIR))

install-%-dll:
	$(if $<, install -m755 -d $(ROOT)$(DLLDIR))
	$(if $<, install -m755 $^ $(ROOT)$(DLLDIR))

install-%-inc:
	$(if $<, install -m755 -d $(ROOT)$(INCDIR))
	$(if $<, install -m755 $^ $(ROOT)$(INCDIR))

install-%-event:
	$(if $<, install -m755 -d $(ROOT)$(EVENT_DIR))
	$(if $<, install -m644 $^ $(ROOT)$(EVENT_DIR))

install-%-xsession:
	$(if $<, install -m755 -d $(ROOT)$(XSESSION_DIR))
	$(if $<, install -m755 $^ $(ROOT)$(XSESSION_DIR))

install-%-backup-config:
	$(if $<, install -m755 -d $(ROOT)$(BACKUP_DIR)/applications)
	$(if $<, install -m644 $^ $(ROOT)$(BACKUP_DIR)/applications)

install-%-backup-restore:
	$(if $<, install -m755 -d $(ROOT)$(BACKUP_DIR)/restore.d/always)
	$(if $<, install -m755 $^ $(ROOT)$(BACKUP_DIR)/restore.d/always)

install-%-cud-scripts:
	$(if $<, install -m755 -d $(ROOT)$(CUD_DIR))
	$(if $<, install -m755 $^ $(ROOT)$(CUD_DIR))

install-%-rfs-scripts:
	$(if $<, install -m755 -d $(ROOT)$(RFS_DIR))
	$(if $<, install -m755 $^ $(ROOT)$(RFS_DIR))

FLOWFLAGS += -Xlogging
FLOWFLAGS += -Xxutil
## QUARANTINE FLOWFLAGS += -Xcodec
## QUARANTINE FLOWFLAGS += -Xdbusif
## QUARANTINE FLOWFLAGS += -Xinifile
## QUARANTINE FLOWFLAGS += -Xevent
## QUARANTINE FLOWFLAGS += -Xaction
## QUARANTINE FLOWFLAGS += -Xstrbuf
## QUARANTINE FLOWFLAGS += -Xunique
## QUARANTINE FLOWFLAGS += -Xsighnd

## QUARANTINE FLOWFLAGS += -Mdbusif=libalarm
FLOWFLAGS += -Mclient=libalarm
## QUARANTINE FLOWFLAGS += -Mevent=libalarm
## QUARANTINE FLOWFLAGS += -Maction=libalarm

CFLOW := cflow -v --omit-arguments
#CFLOW += -is
#CFLOW += -sDBusMessage:type

%.eps     : %.ps    ; ps2epsi $< $@
%.ps      : %.dot   ; dot -Tps  $< -o $@
%.pdf     : %.dot   ; dot -Tpdf $< -o $@
%.png     : %.dot   ; dot -Tpng $< -o $@
%.mod.dot : %.cflow ; ./flow.py -c1 $(FLOWFLAGS) < $< > $@
%.fun.dot : %.cflow ; ./flow.py -c0 $(FLOWFLAGS) < $< > $@
%.api.dot : %.cflow ; ./flow.py -i1 $(FLOWFLAGS) < $< > $@
%.top.dot : %.cflow ; ./flow.py -t1 $(FLOWFLAGS) < $< > $@
%.cflow   :         ; $(CFLOW) -o $@ $^

%.1.gz    : %.1     ; gzip $< -9 -c > $@
%.8.gz    : %.8     ; gzip $< -9 -c > $@

clean::
	$(RM) *.cflow
	$(RM) *.mod.dot *.fun.dot *.api.dot
	$(RM) *.mod.pdf *.fun.pdf *.api.pdf
	$(RM) *.mod.png *.fun.png *.api.png
	$(RM) *.1.gz *.8.gz

# ----------------------------------------------------------------------------
# Target dependencies
# ----------------------------------------------------------------------------

# ----------------------------------------------------------------------------
# libalarm
# ----------------------------------------------------------------------------

# we use dbus_message_iter_get_array_len() because
# as far as I can see we have to -> suppress warnings
dbusif.o dbusif.pic.o : CFLAGS += -Wno-deprecated-declarations

libalarm_src =\
	client.c\
	event.c\
	dbusif.c\
	strbuf.c\
	action.c\
	codec.c\
	logging.c\
	recurrence.c\
	serialize.c\
	ticker.c\
	attr.c

libalarm_obj = $(libalarm_src:.c=.o)

libalarm.a : $(libalarm_obj)
#       ar ru $@ $^

libalarm$(SO) : $(libalarm_obj:.o=.pic.o)
	$(CC) -o $@ -shared  $^ $(LDFLAGS) $(LDLIBS)

# ----------------------------------------------------------------------------
# alarmd
# ----------------------------------------------------------------------------

alarmd_src =\
	alarmd.c\
	mainloop.c\
	sighnd.c\
	queue.c\
	server.c\
	inifile.c\
	symtab.c\
	unique.c\
	escape.c\
	hwrtc.c\
	ipc_statusbar.c\
	ipc_dsme.c\
	ipc_systemui.c\
	ipc_icd.c\
	ipc_exec.c

alarmd_obj = $(alarmd_src:.c=.o)

alarmd : LDLIBS += -lrt

alarmd : $(alarmd_obj) libalarm.a

alarmd.cflow : $(alarmd_src) $(libalarm_src) #*.h

alarmd.grep.% : ; crefs $* $(alarmd_src) $(libalarm_src)

## QUARANTINE FLOWFLAGS_ALARMD += -Xcodec,symtab,unique,inifile,xutil
FLOWFLAGS_ALARMD += -xserver_make_reply
FLOWFLAGS_ALARMD += -xserver_parse_args
alarmd.mod.dot alarmd.fun.dot : FLOWFLAGS += $(FLOWFLAGS_ALARMD)

sizes: ; size -t $(alarmd_obj) $(libalarm_obj)
lines: ; cstats $(alarmd_src) $(libalarm_src) $(patsubst %.c,%.h,$(alarmd_src) $(libalarm_src))

# ----------------------------------------------------------------------------
# alarmclient
# ----------------------------------------------------------------------------

alarmclient_src = alarmclient.c
alarmclient_obj = $(alarmclient_src:.c=.o)

alarmclient : LDLIBS += -lrt
alarmclient : $(alarmclient_obj) libalarm.a
#alarmclient : $(alarmclient_obj) libalarm.so.0

alarmclient.o : CFLAGS += -Wno-unused-function

alarmclient.cflow : $(alarmclient_src) $(libalarm_src)

alarmclient.grep.% : ; crefs $* $(alarmclient_src) $(libalarm_src)

# ----------------------------------------------------------------------------
# alarmtool
# ----------------------------------------------------------------------------

alarmtool_src = alarmtool.c
alarmtool_obj = $(alarmtool_src:.c=.o)

alarmtool : LDLIBS += -lrt
#alarmtool : $(alarmtool_obj) libalarm.a
alarmtool : $(alarmtool_obj) libalarm$(SO)

#alarmtool.o : CFLAGS += -Wno-unused-function

alarmtool.cflow : $(alarmtool_src) $(libalarm_src)

alarmtool.grep.% : ; crefs $* $(alarmtool_src) $(libalarm_src)

# ----------------------------------------------------------------------------
# alarmd.deb
# ----------------------------------------------------------------------------

install-alarmd-man8: alarmd.8.gz
install-alarmd-sbin: alarmd
install-alarmd-event:
install-alarmd-xsession: xsession/03alarmd

install-alarmd-backup-config: osso-backup/alarmd.conf
install-alarmd-backup-restore: osso-backup/alarmd_restart.sh

install-alarmd-backup: $(addprefix install-alarmd-backup-, config restore)

install-alarmd-cud-scripts: osso-cud-scripts/alarmd.sh
install-alarmd-rfs-scripts: osso-rfs-scripts/alarmd.sh

install-alarmd-appkiller: install-alarmd-cud-scripts install-alarmd-rfs-scripts

install-alarmd:: $(addprefix install-alarmd-, sbin event xsession appkiller backup man8)
	mkdir -p $(ROOT)/$(CACHEDIR)

distclean::
	$(RM) osso-cud-scripts/alarmd.sh
	$(RM) osso-rfs-scripts/alarmd.sh

# ----------------------------------------------------------------------------
# alarmclient.deb
# ----------------------------------------------------------------------------

install-alarmclient-bin: alarmclient
install-alarmclient-man1: alarmclient.1.gz

install-alarmclient:: $(addprefix install-alarmclient-, bin man1)

# ----------------------------------------------------------------------------
# libalarm.deb
# ----------------------------------------------------------------------------

install-libalarm-dll: libalarm$(SO)

install-libalarm:: $(addprefix install-libalarm-, dll)

# ----------------------------------------------------------------------------
# libalarm-dev.deb
# ----------------------------------------------------------------------------

install-libalarm-dev-inc: libalarm.h alarm_dbus.h
install-libalarm-dev-lib: libalarm.a

install-libalarm-dev:: $(addprefix install-libalarm-dev-, lib inc) alarm.pc
	ln -sf libalarm$(SO) $(ROOT)$(LIBDIR)/libalarm.so
	install -m755 -d $(ROOT)$(PKGCFGDIR)
	install -m644 alarm.pc $(ROOT)$(PKGCFGDIR)/

clean::
	$(RM) alarm.pc

# ----------------------------------------------------------------------------
# libalarm-doc.deb
# ----------------------------------------------------------------------------

install-libalarm-doc-html: dox
	install -m755 -d $(ROOT)$(DEVDOCDIR)/html
	install -m644 $(DOXYWORK)/html/* $(ROOT)$(DEVDOCDIR)/html/

install-libalarm-doc-man: dox
	install -m755 -d $(ROOT)$(MANDIR)/man3
	install -m644 $(DOXYWORK)/man/man3/* $(ROOT)$(MANDIR)/man3/

install-libalarm-doc:: $(addprefix install-libalarm-doc-, html man)

# ----------------------------------------------------------------------------
# Dependency Scanning
# ----------------------------------------------------------------------------

.PHONY: depend

depend::
	gcc -MM $(CPPFLAGS) *.c | ./depend_filter.py > .depend

ifneq ($(MAKECMDGOALS),depend)
include .depend
endif

# ----------------------------------------------------------------------------
# Prototype Scanning
# ----------------------------------------------------------------------------

.PHONY: %.proto P Q

%.proto : %.q
	cproto -s -E0 $< | prettyproto.py | tee $@

%.proto : %.c
	cproto $(CPPFLAGS) $< | prettyproto.py

#       cproto $(MAC) $(INC) $<

Q:	$(patsubst %.c,%.q,$(wildcard *.c))

P:	$(patsubst %.c,%.proto,$(wildcard *.c))

# ----------------------------------------------------------------------------
# CTAG scanning
# ----------------------------------------------------------------------------

.PHONY: tags

tags:
	ctags *.c *.h *.inc

distclean::
	$(RM) tags

%.q : %.c
	$(CC) $(CPPFLAGS) -E $< > $@
#	$(CC) $(CPPFLAGS) -E $< | ./unpp.py > $@

hdrchk:
	./check_header_files.py $(CPPFLAGS) -- *.h

DEBIAN_FILES = debian/alarmd.init debian/alarmd.postinst

debian-files:: $(DEBIAN_FILES)

clean::
	$(RM) $(DEBIAN_FILES)

debian/alarmd.init : alarmd.init
	$(TEMPLATE_COPY)

debian/alarmd.postinst : alarmd.postinst
	$(TEMPLATE_COPY)

fakesb: fakesb.o libalarm.a

fakedsme: fakedsme.o libalarm.a

fakes : libalarm.a fakesb fakedsme
	make -C debug_gtk
	make -C skeleton

test_minutely : test_minutely.o libalarm.a

alarmd_config.h : alarmd_config.h.tpl

clockd_dbus.grep : /usr/include/clockd/libtime.h
	grep > $@ 'define.*CLOCK' $<

clockd_tool : clockd_tool.o ticker.o logging.o

event.d/alarmd : alarmd.event.tpl
	mkdir -p event.d
	$(TEMPLATE_COPY)

skeleton.cflow : skeleton/skeleton.c
	$(CFLOW) -o $@.tmp $^
	sed -e 's:skeleton_::g' < $@.tmp >$@

quicktest : quicktest.o libalarm.a

quicktest.o : CFLAGS += -Wno-unused-function -Wno-error

clean::
	$(RM) *.proto *.q

distclean::
	$(RM) fakedsme fakesb
	$(RM) clockd_tool quicktest
	$(RM) alarmd_config.h clockd_dbus.grep
	$(RM) osso-backup/alarmd.conf

# ----------------------------------------------------------------------------
# Check source code for unused functions
# ----------------------------------------------------------------------------

.PHONY: dead_code

dead_code:: dead_code.txt
	tac $<

dead_code.txt : dead_code.xref
	./dead_code.py < $< > $@

dead_code.xref :  $(wildcard *.c) $(wildcard *.h)
	cflow -x $^ > $@ --preprocess=./dead_cpp.py

distclean::
	$(RM) dead_code.txt dead_code.xref

libalarm.dot : $(libalarm_obj)
	./resolve_syms.py $^ > $@

startup-tracker : startup-tracker.o dbusif.o libalarm.a

.PHONY: normalize

normalize:
	crlf -a *.[ch]
	crlf -M Makefile debian/rules
	crlf -t -e -k debian/changelog
#eof

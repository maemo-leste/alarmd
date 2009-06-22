#! /usr/bin/env python

# ============================================================================
#
#  This file is part of Alarmd
#
#  Copyright (C) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
#
#  Contact: Simo Piiroinen <simo.piiroinen@nokia.com>
#
#  Alarmd is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public License
#  version 2.1 as published by the Free Software Foundation.
#
#  Alarmd is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with Alarmd; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
#  02110-1301 USA
#
# ============================================================================

def dotname(base):
    if base.endswith(".o"):
        base = base[:-2]
        base = base.split("/")[-1]
    elif base.endswith(".syms"):
        base = base[:-5]

    if base.startswith("lib"):
        base = base.upper()

    return '"%s"' % base

import os,sys

ignore = {
"libmemusage.syms" : None,
}

def ignored(base):
    if base in ignore:
        return 1

    if base.startswith("ld-2."):
        return 1

    return 0

libsyms = {}
objsyms = {}

def lookup(name):
    if name in objsyms:
        return objsyms[name]
    if name in libsyms:
        return libsyms[name]
    return "UNKNOWN"

CACHE = "/tmp/syms"

if not os.path.isdir(CACHE):
    os.mkdir(CACHE)

def get_lib_defs(elf):
    syms = {}
    cmd = "nm --defined-only --dynamic '%s'" % elf
    for s in os.popen(cmd):
        a,t,n = s.split()
        syms[n] = None
    syms = syms.keys()
    syms.sort()
    return syms

def get_obj_defs(elf):
    syms = {}
    cmd = "nm --defined-only '%s'" % elf
    for s in os.popen(cmd):
        a,t,n = s.split()
        syms[n] = None
    syms = syms.keys()
    syms.sort()
    return syms

def get_obj_refs(elf):
    syms = {}
    cmd = "nm --undefined-only '%s'" % elf
    for s in os.popen(cmd):
        t,n = s.split()
        if n in ("_GLOBAL_OFFSET_TABLE_",):
            continue
        syms[n] = None
    syms = syms.keys()
    syms.sort()
    return syms

def libsyms_scan(root):
    for base in os.listdir(root):
        if not ".so" in base:
            continue
        srce = os.path.join(root, base)
        if os.path.islink(srce):
            continue
        if not os.path.isfile(srce):
            continue
        base = base.split(".so")[0]
        dest = "%s/%s.syms" % (CACHE, base)
        if not os.path.exists(dest):
            print>>sys.stderr, "%s <- %s" % (dest, srce)
            print>>open(dest,"w"), "\n".join(get_lib_defs(srce))

def libsyms_init():
    libsyms_scan("/lib")
    libsyms_scan("/usr/lib")

    for base in os.listdir(CACHE):
        if ignored(base):
            continue
        srce = os.path.join(CACHE, base)
        for name in open(srce):
            name = name.strip()
            if name:
                libsyms[name] = base

def objsyms_init(objs):
    for base in objs:
        for name in get_obj_defs(base):
            objsyms[name] = base

if __name__ == "__main__":
    hide = (
    "libc.syms",
    "libc-2.5.syms",
    "libgcc_s.syms",
    "libpthread-2.5.syms",
    "src/logging.o",
    "src/logging.pic.o",
    )

    objs = sys.argv[1:]
    #objs = filter(lambda x:not ".pic.o" in x, objs)
    #objs = filter(lambda x: not "logging." in x, objs)

    libsyms_init()
    objsyms_init(objs)

    dot = []
    _ = lambda x:dot.append(x)

    _('digraph foo {')
    _('rankdir=LR;')
    _('ranksep=0.1;')
    _('nodesep=0.1;')
    _('node[shape=box];')
    _('node[width=0.1];')
    _('node[height=0.1];')

    for obj in objs:
        #print "%s:" % obj
        dep = {}
        for name in get_obj_refs(obj):
            srce = lookup(name)
            if srce == "UNKNOWN":
                print>>sys.stderr, "%s:%s <- %s" % (obj, name, srce)
            dep[srce] = None

        for mod in hide:
            if mod in dep:
                del dep[mod]

        obj = dotname(obj)
        dep = map(dotname, dep)
        dep.sort()

        _("%s -> { %s };" % (obj, " ".join(dep)))

    _('}')

    print "\n".join(dot)

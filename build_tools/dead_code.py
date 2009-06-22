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

import sys,os,string

if __name__ == "__main__":

    defs = {}
    refs = {}

    refs["main"] = None

    for s in os.popen("nm --dynamic --defined-only libalarm.so.2"):
        v = s.split()
        if len(v) >= 3:
            refs[v[2]]=None

## QUARANTINE     for s in open("libalarm.syms"):
## QUARANTINE         s = s.strip()
## QUARANTINE         if s: refs[s] = None

    for s in sys.stdin:
        v = s.split(None, 3)
        #print>>sys.stderr, v
        if len(v) == 2:
            symb,xref = v
            assert ':' in xref
            refs[symb] = None
        elif len(v) == 4:
            #print>>sys.stderr, v
            assert v[1] == "*"
            symb,xref = v[0],v[2]
            assert ':' in xref
            file,line = xref.split(":",1)
            defs[symb] = (file,int(line),symb)

    dead = []
    for symb in defs:
        if not refs.has_key(symb):
            dead.append(symb)

    dead = map(defs.get, dead)
    dead.sort()

    print "Dead Code:"
    for file,line,symb in dead:
        print "%s:%d:%s" % (file,line,symb)

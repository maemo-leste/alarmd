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

import sys,os

def isfile(s):
    return os.path.exists(s)

def isnumb(s):
    try: int(s)
    except ValueError:
        return 0
    return 1

base = os.path.abspath(os.getcwd()) + "/"

for s in sys.stdin:
    s = s.rstrip()
    v = s.split(":", 2)
    if len(v) == 3:
        path,line,mesg = v

        if isfile(path) and isnumb(line):
            path = os.path.abspath(path)
            if path.startswith(base):
                path = path[len(base):]

            print "%s:%s:%s" % (path,line,mesg)
            continue
    print s

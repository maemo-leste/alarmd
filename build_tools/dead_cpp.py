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
    inputs = []
    output = []

    args = sys.argv[1:]
    args.reverse()

    block = 0
    while args:
        a = args.pop()
        output.append('# 1 "%s"' % a)
        for s in open(a):
            s = s.rstrip()
            if s.strip().startswith("#ifdef DEAD_CODE"):
                block = 1
            elif block:
                if s.strip().startswith("#"):
                    block = 0
                else:
                    s = ""
            output.append(s)

    print "\n".join(output)

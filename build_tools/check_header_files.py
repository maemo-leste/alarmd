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

if __name__ == "__main__":
    args = sys.argv[1:]
    args.reverse()

    flgs = ["-std=c99"]

    src = 'foobarbaz.c'
    obj = 'foobarbaz.o'

    while args:
        flg = args.pop()
        if flg == "--":
            break
        flgs.append(flg)

    flgs = " ".join(flgs)

    while args:
        hdr = args.pop()
        print>>sys.stderr, "checking: %s ..." % hdr
        assert os.path.isfile(hdr)

        txt = '#include "%s"' % hdr

        print>>open(src,"w"), txt

        if os.system("gcc -c -o'%s' %s '%s'" % (obj, flgs, src)) != 0:
            break

    if os.path.exists(src):
        os.remove(src)
    if os.path.exists(obj):
        os.remove(obj)

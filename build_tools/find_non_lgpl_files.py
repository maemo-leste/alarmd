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

LGPL = """
This file is part of Alarmd

Copyright (C) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).

Contact: Simo Piiroinen <simo.piiroinen@nokia.com>

Alarmd is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public License
version 2.1 as published by the Free Software Foundation.

Alarmd is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with Alarmd.  If not, see <http://www.gnu.org/licenses/>.
"""

LGPL = """
This file is part of Alarmd

Copyright (C) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).

Contact: Simo Piiroinen <simo.piiroinen@nokia.com>

Alarmd is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public License
version 2.1 as published by the Free Software Foundation.

Alarmd is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with Alarmd; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301 USA
"""

LGPL = LGPL.splitlines()
LGPL = filter(lambda x:x, LGPL)

IGNORE_DIRS = {
".svn":None,
"tools":None,
"VOID":None,
"doxyout":None,
}

def ignore_dir(dirname):
    return dirname in IGNORE_DIRS

IGNORE_FILES = (
"~",
".bak",
".o",
".so",
".a",
".png",
".gif",
".css",
".ttf",
".html",
".gz",
".diff",
".tex",
".sty",
"/.depend",
"/Makefile",
"/Doxyfile",
"/COPYING",
"/scrap.txt",
"/debian/changelog",
"/debian/control",
"/debian/compat",
"/debian/rules",
)

def ignore_file(filename):
    for e in IGNORE_FILES:
        if filename.endswith(e):
            return 1
    return 0

ELF = '\x7f' + 'ELF'

for root, dirs, files in os.walk('.', topdown=True):
    i = 0
    while i < len(dirs):
        if ignore_dir(dirs[i]):
            del dirs[i]
        else:
            i += 1

    for path in files:
        path = os.path.join(root, path)

        if ignore_file(path):
            continue

        if os.path.islink(path): continue
        if os.path.getsize(path) == 0: continue

        if open(path,"rb").read(4) == ELF:
            continue

        data = open(path).read().splitlines()
        last = None
        rows = range(len(data))
        miss = []

        for s in LGPL:
            for r in rows:
                t = data[r].replace('\\.','.')
                if t.endswith(s):
                    last = r
                    break
            else:
                miss.append(s)

        if not miss:
            continue

        if last == None:
            print "%s:1: NO LGPL FOUND" % path
        else:
            print "%s:%d: PARTIAL LGPL FOUND" % (path, last)
            for s in miss:
                print "miss\t%s" % s

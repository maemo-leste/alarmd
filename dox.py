#! /usr/bin/env python

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


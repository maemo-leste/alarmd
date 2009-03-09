#! /usr/bin/env python

import sys,os,string

COLORMAP = (
"#e633e6", "#33e633", "#33338c", "#8ce6e6", "#e66e33", "#e6e68c", "#336e33",
"#33ab8c", "#8c6e8c", "#8c3333", "#336ee6", "#8cab33", "#e66ee6", "#8c33e6",
"#e6338c", "#33e6e6", "#8cabe6", "#8ce68c", "#e6ab8c", "#e6e633", "#33ab33",
"#e63333", "#336e8c", "#8c6e33", "#3333e6", "#33e68c", "#8c338c", "#33abe6",
"#8c6ee6", "#8cab8c", "#e66e8c", "#8ce633", "#e6ab33", "#e6abe6",
)

def bgcolor(i):
    def scale(s):
        a,b = 10,40
        return (int(s,16)*a + 255*b)/(a+b)
    rgb = COLORMAP[i % len(COLORMAP)]
    r,g,b = map(scale, (rgb[1:3],rgb[3:5],rgb[5:7]))
    return "#%02x%02x%02x" % (r,g,b)

MAP_MODULE = {}
def set_module_mapping(real, show):
    MAP_MODULE[real] = show
def map_module(name):
    return MAP_MODULE.get(name,name)

IGNORE_FUNCTIONS = {}
IGNORE_MODULES   = { "" : None }
def ignore_function(name):
    IGNORE_FUNCTIONS[name + "()"] = None
def ignore_module(name):
    IGNORE_MODULES[name] = None
def is_ignored(sym,mod):
    if mod in IGNORE_MODULES:
        return 1
    if sym in IGNORE_FUNCTIONS:
        return 1
    return None

def parse_line(s):
    s = s.expandtabs()
    s = s.rstrip()

    if ' at ' in s:
        s,f = s.split(' at ')
        f = f.split(":")[0]
        f = os.path.splitext(f)[0]
    else:
        s,f = s, ""

    i,n = 0,len(s)
    while i < n and s[i] == ' ':
        i += 1
    k = i + 1
    while k < n and s[k] != ' ':
        k += 1

    s = s[i:k]
    assert (i&3) == 0

    f = map_module(f)

    return i/4, s, f

def parse_bool(s):
    try:
        return int(s) != 0
    except ValueError:
        pass
    return s.lower() in ("y","yes","t","true")

if __name__ == "__main__":

    CLUSTERIZE = 0
    INTERFACE  = 0
    TOPLEVEL   = 0

    # - - - - - - - - - - - - - - - - - - - -
    # parse args
    # - - - - - - - - - - - - - - - - - - - -

    args = sys.argv[1:]
    args.reverse()
    while args:
        a = args.pop()
        k,v = a[:2],a[2:]
        if k == "-c":
            CLUSTERIZE = parse_bool(v or args.pop())
        elif k == "-i":
            INTERFACE = parse_bool(v or args.pop())
        elif k == "-t":
            TOPLEVEL = parse_bool(v or args.pop())
        elif k == "-x":
            for s in (v or args.pop()).split(","):
                ignore_function(s)
        elif k == "-X":
            for s in (v or args.pop()).split(","):
                ignore_module(s)
        elif k == "-M":
            for s in (v or args.pop()).split(","):
                real,show = s.split("=")
                set_module_mapping(real,show)
        else:
            print>>sys.stderr, "Unknown option:", a
            sys.exit(1)

    if TOPLEVEL:
        INTERFACE = CLUSTERIZE = 0

    if INTERFACE:
        CLUSTERIZE = 1

    # - - - - - - - - - - - - - - - - - - - -
    # parse cflow output
    # - - - - - - - - - - - - - - - - - - - -

    stk,dep,ign = [],{},999
    for s in sys.stdin:
        lev,sym,mod = parse_line(s)
        while len(stk) > lev:
            stk.pop()
        if len(stk) <= ign:
            ign = 999
        if is_ignored(sym,mod):
            ign = min(ign, lev)
        key = (sym,mod)
        if 0 < len(stk) < ign:
            dep[(stk[-1],key)]=None
        stk.append(key)

    if INTERFACE:
        tmp = {}
        for src,dst in dep:
            if src[1] != dst[1]:
                #print>>sys.stderr, "USE: %s -> %s" % (str(src),str(dst))
                mod = src[1]
                fun = mod.upper()
                src = (fun,mod)
                tmp[(src,dst)]=None
            else:
                pass
                #print>>sys.stderr, "IGN: %s -> %s" % (str(src),str(dst))
## QUARANTINE                 mod = dst[1].upper()
## QUARANTINE                 dst = (mod,mod)
## QUARANTINE                 tmp[(src,dst)]=None
        dep = tmp

    if TOPLEVEL:
        tmp = {}
        for src,dst in dep:
            if src[1] != dst[1]:
                src = (src[1],src[1])
                dst = (dst[1],dst[1])
                tmp[(src,dst)]=None
        dep = tmp
    dep = dep.keys()

    # - - - - - - - - - - - - - - - - - - - -
    # enumerate modules & functions
    # - - - - - - - - - - - - - - - - - - - -

    col = {} # module name -> color
    obj = {} # symbol name -> identifier
    clu = {} # module nane -> list of symbols

    # assign colors from sorted module names
    # -> consistent over different graph types
    tmp = {}
    for src,dst in dep:
        tmp[src[1]] = None
        tmp[dst[1]] = None
    for mod in sorted(tmp):
        if mod == mod.upper():
            col[mod] = None
        else:
            col[mod] = bgcolor(len(col))

    # assign identifiers for symbols
    # group symbols by module
    def ref(cur):
        if not obj.has_key(cur):
            obj[cur] = "node%04d" % len(obj)
        sym,mod = cur
        if not clu.has_key(mod):
            clu[mod] = [cur]
        else:
            clu[mod].append(cur)

    for src,dst in dep:
        ref(src)
        ref(dst)

    # - - - - - - - - - - - - - - - - - - - -
    # generate dot source
    # - - - - - - - - - - - - - - - - - - - -

    dot = []
    _ = lambda x:dot.append(x)

    _('digraph flow {')

    _('rankdir = LR;')
    _('ranksep = 0.4;')
    _('nodesep = 0.2;')

    _('node[width=.1];')
    _('node[height=.1];')
    _('node[fontsize=7];')
    if TOPLEVEL:
        _('node[shape=ellipse];')
    else:
        _('node[shape=box];')
    _('node[style=filled, fillcolor=yellow];')

    _('')
    for cur,name in obj.items():
        if CLUSTERIZE:
            _('%s[label="%s"];' % (name,cur[0]))
        else:
            lab=cur[0]
            pen=col[cur[1]]
            if pen == None:
                _('%s[label="%s",style=filled,shape=box];' % (name,lab))
            else:
                _('%s[label="%s",style=filled,fillcolor="%s"];' % (name,lab,pen))

    _('')
    for src,dst in dep:
        _('%s->%s;' % (obj[src], obj[dst]))

    if CLUSTERIZE:
        for mod,syms in clu.items():
            _('')
            _('subgraph cluster%s {' % mod.replace('-','_'))
            #_('bgcolor="#f0f0f0";')
            _('bgcolor="%s";' % col[mod])
            for sym in syms:
                _('%s;' % obj[sym])
            _('}')

    _('}')

    print "\n".join(dot)

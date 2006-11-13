#!/bin/sh
dbus-send --session --dest=com.nokia.alarmd --type=method_call --print-reply /com/nokia/alarmd com.nokia.alarmd.add_event objpath:/AlarmdEvent uint32:2 string:action objpath:/AlarmdActionExec uint32:2 string:path string:"/bin/echo foo" string:flags int32:17 string:time uint64:$((`date +%s`+10))

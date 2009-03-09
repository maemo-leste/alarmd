#! /bin/sh

@SBINDIR@/alarmd -Xcud || rm -f @CACHEDIR@/alarm_queue.*

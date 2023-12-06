#!/bin/sh
[ "$2" = "-v" ] || exec 1>/dev/null 2>/dev/null
set -x
cadical -n -q $1

#!/bin/sh
name=`basename $1 .cnf`
[ "$2" = "-v" ] || exec 1>/dev/null 2>/dev/null
set -x
proof=`basename $0 .sh`.proof
cadical -q -n $1 $proof

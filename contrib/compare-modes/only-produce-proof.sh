#!/bin/sh
. ./verbose.sh
proof=`basename $0 .sh`.proof
cadical -n$silent --lrat $1 $proof

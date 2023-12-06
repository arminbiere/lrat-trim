#!/bin/sh
. ./verbose.sh
name=`basename $0 .sh`
proof=$name.proof
trimmed=$name.trimmed
cadical -n$silent --lrat $1 $proof
lrat-trim$silent $1 $proof $trimmed

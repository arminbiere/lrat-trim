#!/bin/sh
. ./verbose.sh
cadical -n$silent --lrat $1 - | lrat-trim$silent $1 -

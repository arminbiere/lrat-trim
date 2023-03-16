#!/bin/sh

# Put here the name of the CaDiCaL binary supporting LRAT.

radical=radical

# End of hard-coded configuration.

die () {
  echo "lrat-trim/test/fuzz/produce.sh: error: $*" 1>&2
  exit 1
}

[ $# = 2 ] || die "expected exactly two arguments"

[ -f $1 ] || die "first argument is not a file"

msg () {
  echo "[lrat-trim/test/fuzz/produce.sh] $*"
}

version="`radical --version 2>/dev/null`"

[ x"$version" = x ] && die "could not find 'radical'"

echo "found 'radical'  version '$version'"

cnf="$1"
lrat="$2"

echo "reading CNF '$cnf'"
echo "writing LRAT '$lrat'"

msg "calling 'radical'"
exec $radical --lrat --no-binary $cnf $lrat

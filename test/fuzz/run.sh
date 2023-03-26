#!/bin/sh

# Put here the name of the CaDiCaL binary supporting LRAT.

radical=radical

# End of hard-coded configuration.

tmp="/tmp/lrat-trim-run-$$"

die () {
  echo "lrat-trim/test/fuzz/run.sh: error: $*" 1>&2
  rm -f $tmp*
  exit 1
}

msg () {
  echo "[lrat-trim/test/fuzz/run.sh] $*"
}

lrattrim=../../lrat-trim
[ -f $lrattrim ] || die "could not find '$lrattrim'"

version="`radical --version 2>/dev/null`"

[ x"$version" = x ] && die "could not find 'radical'"

echo "found 'radical'  version '$version'"

cnf=$tmp.cnf
lrat=$tmp.lrat
log1=$tmp.log1
log2=$tmp.log2

rm -f $cnf $lrat

trap "rm -f $tmp*" 2 10 15

msg "saving CNF as '$cnf'"
cat $* > $cnf

msg "calling 'radical'"
#$radical --lrat --no-binary $cnf $lrat > $log1
$radical --lrat $cnf $lrat > $log1
radicalstatus=$?

cat $log1

case $radicalstatus in
  10|20) ;;
  *) die "unexpected 'radical' exit status '$radicalstatus'";;
esac

if false
then

  msg "backward checking with 'lrat-trim'"
  $lrattrim $cnf $lrat > $log2
  lrattrimstatus=$?

else

  msg "forward checking with 'lrat-trim'"
  $lrattrim $cnf $lrat -S --strict > $log2
  lrattrimstatus=$?

fi

cat $log2

case $lrattrimstatus in
  0|10|20) ;;
  *) die "unexpected 'lrat-trim' exit status '$lrattrimstatus'";;
esac

msg "cleaning up"
rm -f $tmp*

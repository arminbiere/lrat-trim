#!/bin/sh

die () {
  echo "lrat-trim/test/parse: $*" 1>&2
  exit 1
}

cd `dirname $0`

lrattrim=../../lrat-trim

[ -f $lrattrim ] || die "could not find 'lrat-trim"

run () {
  name=$1
  lrat=$name.lrat
  log=$name.log
  err=$name.err
  [ -f $lrat ] || die "could not find '$lrat'"
  $lrattrim $lrat 1>$log 2>$err
  status=$?
  if [ $status = 1 ]
  then
    echo "parsing 'test/parse/$lrat' failed as expected"
  else
    echo "parsing 'test/parse/$lrat' succeeded unexpectedly"
    exit 1
  fi
}

run creof
run nodigitatid
run zeroid
run idtoobig1
run idtoobig2
run idnospace
run dnospace
run deof
run addeof1
run addeof2
run addnoinc
run delnoinc
run empty

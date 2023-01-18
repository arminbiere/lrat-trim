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

run addeof1
run addeof2
run addnoinc
run creof
run delnodigit1
run delnodigit2
run delnoinc
run delnonl
run deltoobig1
run deltoobig2
run deltoobig3
run deltwice
run delunexpdig
run deof
run dnospace
run empty
run idnospace
run idtoobig1
run idtoobig2
run nodigitatid
run zeroid

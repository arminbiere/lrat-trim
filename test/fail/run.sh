#!/bin/sh

die () {
  echo "lrat-trim/test/fail: $*" 1>&2
  exit 1
}

cd `dirname $0`

rm -f *.err? *.log?

lrattrim=../../lrat-trim

[ -f $lrattrim ] || die "could not find 'lrat-trim'"

run () {
  name=$1
  cnf=$name.cnf
  lrat=$name.lrat

  [ -f $cnf ] || die "can not find '$cnf'"
  [ -f $lrat ] || die "can not find '$lrat'"

  log1=$name.log1
  err1=$name.err1

  $lrattrim $cnf $lrat 1>$log1 2>$err1
  status=$?
  if [ $status = 1 ]
  then
    echo "lrat-trim test/fail/$cnf test/fail/$lrat # checking failed as expected"
  else
    echo "lrat-trim test/fail/$cnf test/fail/$lrat # unexpected exit code $status"
    exit 1
  fi

  log2=$name.log2
  err2=$name.err2

  $lrattrim -t -v $cnf $lrat 1>$log2 2>$err2
  status=$?
  if [ $status = 1 ]
  then
    echo "lrat-trim -t -v test/fail/$cnf test/fail/$lrat # checking failed as expected"
  else
    echo "lrat-trim -t -v test/fail/$cnf test/fail/$lrat # unexpected exit code $status"
    exit 1
  fi
}

run empty
run blocked
run nounit1

runs=`grep '^run [a-z]' run.sh|wc -l`

cnfs=`ls *.cnf|wc -l`
lrats=`ls *.lrat|wc -l`
log1s=`ls *.log1|wc -l`
err1s=`ls *.err1|wc -l`
log2s=`ls *.log2|wc -l`
err2s=`ls *.err2|wc -l`

[ $runs = $cnfs ] || die "found $runs runs in './run.sh' but $cnfs '.cnf' files"
[ $runs = $lrats ] || die "found $runs runs in './run.sh' but $lrats '.lrat' files"

[ $runs = $log1s ] || die "found $runs runs in './run.sh' but $log1s '.log1' files"
[ $runs = $err1s ] || die "found $runs runs in './run.sh' but $err1s '.err1' files"
[ $runs = $log2s ] || die "found $runs runs in './run.sh' but $log2s '.log2' files"
[ $runs = $err2s ] || die "found $runs runs in './run.sh' but $err2s '.err2' files"

echo "passed $runs tests in 'test/fail/run.sh'"

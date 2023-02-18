#!/bin/sh

die () {
  echo "lrat-trim/test/fail: $*" 1>&2
  exit 1
}

cd `dirname $0`

rm -f *.err *log

lrattrim=../../lrat-trim

[ -f $lrattrim ] || die "could not find 'lrat-trim"

run () {
  name=$1
  cnf=$name.cnf
  lrat=$name.lrat
  log=$name.log
  err=$name.err

  [ -f $cnf ] || die "can not find '$cnf'"
  [ -f $lrat ] || die "can not find '$lrat'"

  $lrattrim $cnf $lrat 1>$log 2>$err
  status=$?
  if [ $status = 1 ]
  then
    echo "lrat-trim test/fail/$cnf test/fail/$lrat # checking failed as expected"
  else
    echo "lrat-trim test/fail/$cnf test/fail/$lrat # unexpected exit code $status"
    exit 1
  fi
}

run emptycnf

runs=`grep '^run [a-z]' run.sh|wc -l`

logs=`ls *.log|wc -l`
errs=`ls *.err|wc -l`
cnfs=`ls *.cnf|wc -l`
lrats=`ls *.lrat|wc -l`

[ $runs = $logs ] || die "found $runs runs in './run.sh' but $logs '.log' files"
[ $runs = $errs ] || die "found $runs runs in './run.sh' but $errs '.err' files"
[ $runs = $cnfs ] || die "found $runs runs in './run.sh' but $cnfs '.cnf' files"
[ $runs = $lrats ] || die "found $runs runs in './run.sh' but $lrats '.lrat' files"

echo "passed $runs tests in 'test/fail/run.sh'"

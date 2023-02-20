#!/bin/sh

die () {
  echo "lrat-trim/test/check: $*" 1>&2
  exit 1
}

cd `dirname $0`

rm -f *.err* *.log* *.lrat1

lrattrim=../../lrat-trim

[ -f $lrattrim ] || die "could not find 'lrat-trim'"

run () {
  name=$1

  cnf=$name.cnf
  lrat=$name.lrat
  log=$name.log
  err=$name.err

  [ -f $cnf ] || die "could not find '$cnf'"
  [ -f $lrat ] || die "could not find '$lrat'"

  if [ x"`grep '^[0-9][0-9]* 0' $lrat`" = x ]
  then
    expected=0
  else
    expected=20
  fi

  $lrattrim $cnf $lrat 1>$log 2>$err
  status=$?
  if [ $status = $expected ]
  then
    echo "./lrat-trim test/check/$cnf test/check/$lrat # checking succeeded with exit status '$status'"
  else
    echo "./lrat-trim test/check/$cnf test/check/$lrat # checking failed with exit status '$status' (expected '$expected')"
    exit 1
  fi

  log1=$name.log1
  err1=$name.err1

  $lrattrim $cnf $lrat -S 1>$log1 2>$err1
  status=$?
  if [ $status = $expected ]
  then
    echo "./lrat-trim test/check/$cnf test/check/$lrat -S # checking succeeded with exit status '$status'"
  else
    echo "./lrat-trim test/check/$cnf test/check/$lrat -S # checking failed with exit status '$status' (expected '$expected')"
    exit 1
  fi
}

runs=0
for i in `ls -S *.lrat`
do
  name=`basename $i .lrat`
  run $name
  runs=`expr $runs + 1`
done

cnfs=`ls *nf|wc -l`
lrats=`ls *lrat|wc -l`
logs=`ls *.log|wc -l`
errs=`ls *.err|wc -l`
err1s=`ls *.err1|wc -l`
log1s=`ls *.log1|wc -l`

[ $runs = $cnfs ] || die "found $runs runs in './run.sh' but $cnfs '.cnf' files"
[ $runs = $lrats ] || die "found $runs runs in './run.sh' but $lrats '.lrat' files"
[ $runs = $logs ] || die "found $runs runs in './run.sh' but $logs '.log' files"
[ $runs = $log1s ] || die "found $runs runs in './run.sh' but $log1s '.log1' files"
[ $runs = $errs ] || die "found $runs runs in './run.sh' but $errs '.err' files"
[ $runs = $err1s ] || die "found $runs runs in './run.sh' but $err1s '.err1' files"

echo "passed $runs checking tests in 'test/check/run.sh'"

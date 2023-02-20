#!/bin/sh

die () {
  echo "lrat-trim/test/regr: $*" 1>&2
  exit 1
}

cd `dirname $0`

rm -f *.err* *.log* *.lrat1

lrattrim=../../lrat-trim

[ -f $lrattrim ] || die "could not find 'lrat-trim'"

run () {
  name=$1

  cnf=$name.cnf
  if [ -f $cnf ]
  then
    includecnf="test/regr/$name.cnf "
  else
    includecnf=""
    cnf=""
  fi

  lrat=$name.lrat
  log=$name.log
  err=$name.err

  [ -f $lrat ] || die "could not find '$lrat'"

  $lrattrim $cnf $lrat 1>$log 2>$err
  status=$?
  if [ $status = 0 ]
  then
    echo "./lrat-trim ${includecnf}test/regr/$lrat # trimming in memory succeeded"
  else
    echo "./lrat-trim ${includecnf}test/regr/$lrat # trimming in memory failed"
    exit 1
  fi

  lrat1=$name.lrat1
  log1=$name.log1
  err1=$name.err1

  $lrattrim $cnf $lrat $lrat1 1>$log1 2>$err1
  status=$?
  if [ $status = 0 ]
  then
    echo "./lrat-trim ${includecnf}test/regr/$lrat test/regr/$lrat1 # trimming succeeded"
  else
    echo "./lrat-trim ${includecnf}test/regr/$lrat test/regr/$lrat1 # trimming failed"
    exit 1
  fi
}

runs=0
for i in regr???.lrat
do
  name=`basename $i .lrat`
  run $name
  runs=`expr $runs + 1`
done

logs=`ls *.log|wc -l`
log1s=`ls *.log1|wc -l`
errs=`ls *.err|wc -l`
err1s=`ls *.err1|wc -l`
lrats=`ls *lrat|wc -l`
lrat1s=`ls *lrat|wc -l`

[ $runs = $logs ] || die "found $runs runs in './run.sh' but $logs '.log' files"
[ $runs = $log1s ] || die "found $runs runs in './run.sh' but $log1s '.log1' files"
[ $runs = $errs ] || die "found $runs runs in './run.sh' but $errs '.err' files"
[ $runs = $err1s ] || die "found $runs runs in './run.sh' but $err1s '.err1' files"
[ $runs = $lrats ] || die "found $runs runs in './run.sh' but $lrats '.lrat' files"
[ $runs = $lrat1s ] || die "found $runs runs in './run.sh' but $lrat1s '.lrat1' files"

echo "passed $runs regression tests in 'test/regr/run.sh'"

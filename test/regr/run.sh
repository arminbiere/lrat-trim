#!/bin/sh

die () {
  echo "lrat-trim/test/regr: $*" 1>&2
  exit 1
}

cd `dirname $0`

rm -f *.err* *.log* *.lrat1

lrattrim=../../lrat-trim

[ -f $lrattrim ] || die "could not find 'lrat-trim"

run () {
  name=$1

  cnf=$name.cnf
  if [ -f $cnf ]
  then
    includecnf="$name.cnf "
  else
    includecnf=""
  fi

  lrat=$name.lrat
  log=$name.log
  err=$name.err

  [ -f $lrat ] || die "could not find '$lrat'"

  $lrattrim ${includecnf}$lrat 1>$log 2>$err
  status=$?
  if [ $status = 0 ]
  then
    echo "trimming with 'lrat-trim ${includecnf}test/trim/$lrat' in memory succeeded"
  else
    echo "trimming with 'lrat-trim ${includecnf}test/trim/$lrat' in memory failed"
    exit 1
  fi

  lrat1=$name.lrat1
  log1=$name.log1
  err1=$name.err1

  $lrattrim ${includecnf}$lrat $lrat1 1>$log1 2>$err1
  status=$?
  if [ $status = 0 ]
  then
    echo "trimming with 'lrat-trim ${includecnf}test/trim/$lrat test/trim/$lrat1' succeeded"
  else
    echo "trimming with 'lrat-trim ${includecnf}test/trim/$lrat test/trim/$lrat1' failed"
    exit 1
  fi
}

for i in regr???.lrat
do
  name=`basename $i .lrat`
  run $name
done

runs=`grep '^run [a-z]' run.sh|wc -l`
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
[ $runs = $err2s ] || die "found $runs runs in './run.sh' but $err1s '.err2' files"
[ $runs = $lrats ] || die "found $runs runs in './run.sh' but $lrats '.lrat' files"
[ $runs = $lrat1s ] || die "found $runs runs in './run.sh' but $lrat1s '.lrat1' files"

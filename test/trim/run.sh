#!/bin/sh
die () {
  echo "lrat-trim/test/trim: $*" 1>&2
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
  if [ $status = 0 ]
  then
    echo "trimming 'test/trim/$lrat' in memory succeeded as expected"
  else
    echo "trimming 'test/trim/$lrat' in memory failed unexpectedly"
    exit 1
  fi

  lrat1=${lrat}1
  log1=${log}1
  err1=${err}1

  $lrattrim $lrat $lrat1 1>$log1 2>$err1
  status=$?
  if [ $status = 0 ]
  then
    echo "trimming 'test/trim/$lrat test/trim/$lrat1' succeeded as expected"
  else
    echo "trimming 'test/trim/$lrat test/trim/$lrat1' failed unexpectedly"
    exit 1
  fi

  lrat2=${lrat}2
  log2=${log}2
  err2=${err}2

  $lrattrim $lrat1 $lrat2 1>$log2 2>$err2
  status=$?
  if [ $status = 0 ]
  then
    echo "trimming 'test/trim/$lrat1 test/trim/$lrat2' succeeded as expected"
  else
    echo "trimming 'test/trim/$lrat1 test/trim/$lrat2' failed unexpectedly"
    exit 1
  fi
}

run empty
run bin1
run full1
run full2
run full3
run full4
run full5
run full6
run full7
run add4
run add8
run ph2
run ph3
run ph4

runs=`grep '^run [a-z]' run.sh|wc -l`
logs=`ls *.log|wc -l`
log1s=`ls *.log1|wc -l`
log2s=`ls *.log2|wc -l`
errs=`ls *.err|wc -l`
err1s=`ls *.err1|wc -l`
err2s=`ls *.err2|wc -l`
lrats=`ls *lrat|wc -l`
lrat1s=`ls *lrat|wc -l`
lrat2s=`ls *lrat|wc -l`

[ $runs = $logs ] || die "found $runs runs in './run.sh' but $logs '.log' files"
[ $runs = $log1s ] || die "found $runs runs in './run.sh' but $log1s '.log1' files"
[ $runs = $log2s ] || die "found $runs runs in './run.sh' but $log2s '.log2' files"
[ $runs = $errs ] || die "found $runs runs in './run.sh' but $errs '.err' files"
[ $runs = $err1s ] || die "found $runs runs in './run.sh' but $err1s '.err1' files"
[ $runs = $err2s ] || die "found $runs runs in './run.sh' but $err1s '.err2' files"
[ $runs = $lrats ] || die "found $runs runs in './run.sh' but $lrats '.lrat' files"
[ $runs = $lrat1s ] || die "found $runs runs in './run.sh' but $lrat1s '.lrat1' files"
[ $runs = $lrat2s ] || die "found $runs runs in './run.sh' but $lrat2s '.lrat2' files"

#!/bin/sh

die () {
  echo "lrat-trim/test/trim: $*" 1>&2
  exit 1
}

cd `dirname $0`

rm -f *.err* *.log* *.lrat[12]

lrattrim=../../lrat-trim

[ -f $lrattrim ] || die "could not find 'lrat-trim'"

runascii () {
  name=$1

  lrat=$name.lrat
  log=$name.log
  err=$name.err

  [ -f $lrat ] || die "could not find '$lrat'"

  $lrattrim $lrat 1>$log 2>$err
  status=$?
  if [ $status = 0 ]
  then
    echo "./lrat-trim test/trim/$lrat # trimming in memory succeeded"
  else
    echo "./lrat-trim test/trim/$lrat # trimming in memory failed"
    exit 1
  fi

  lrat1=${lrat}1
  log1=${log}1
  err1=${err}1

  $lrattrim $lrat $lrat1 1>$log1 2>$err1
  status=$?
  if [ $status = 0 ]
  then
    echo "./lrat-trim test/trim/$lrat test/trim/$lrat1 # trimming succeeded"
  else
    echo "./lrat-trim test/trim/$lrat test/trim/$lrat1 # trimming failed"
    exit 1
  fi

  lrat2=${lrat}2
  log2=${log}2
  err2=${err}2

  $lrattrim $lrat1 $lrat2 1>$log2 2>$err2
  status=$?
  if [ $status = 0 ]
  then
    echo "./lrat-trim test/trim/$lrat1 test/trim/$lrat2 # trimming succeeded"
  else
    echo "./lrat-trim test/trim/$lrat1 test/trim/$lrat2 # trimming failed"
    exit 1
  fi

  runs=`expr $runs + 1`
}

runs=0

for i in `ls -rS *.lrat`
do
  name=`basename $i .lrat`
  runascii $name
done

echo "passed $runs trimming tests in 'test/trim/run.sh'"

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
  log=$name.ascii.log
  err=$name.ascii.err

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

  $lrattrim -a $lrat $lrat1 1>$log1 2>$err1
  status=$?
  if [ $status = 0 ]
  then
    echo "./lrat-trim -a test/trim/$lrat test/trim/$lrat1 # trimming succeeded"
  else
    echo "./lrat-trim -a test/trim/$lrat test/trim/$lrat1 # trimming failed"
    exit 1
  fi

  lrat2=${lrat}2
  log2=${log}2
  err2=${err}2

  $lrattrim -a $lrat1 $lrat2 1>$log2 2>$err2
  status=$?
  if [ $status = 0 ]
  then
    echo "./lrat-trim -a test/trim/$lrat1 test/trim/$lrat2 # trimming succeeded"
  else
    echo "./lrat-trim -a test/trim/$lrat1 test/trim/$lrat2 # trimming failed"
    exit 1
  fi

  runs=`expr $runs + 1`
}

runbinary () {
  name=$1

  lrit=$name.lrit
  log=$name.binary.log
  err=$name.binary.err

  [ -f $lrit ] || die "could not find '$lrit'"

  $lrattrim $lrit 1>$log 2>$err
  status=$?
  if [ $status = 0 ]
  then
    echo "./lrat-trim test/trim/$lrit # trimming in memory succeeded"
  else
    echo "./lrat-trim test/trim/$lrit # trimming in memory failed"
    exit 1
  fi

  lrit1=${lrit}1
  log1=${log}1
  err1=${err}1

  $lrattrim $lrit $lrit1 1>$log1 2>$err1
  status=$?
  if [ $status = 0 ]
  then
    echo "./lrat-trim test/trim/$lrit test/trim/$lrit1 # trimming succeeded"
  else
    echo "./lrat-trim test/trim/$lrit test/trim/$lrit1 # trimming failed"
    exit 1
  fi

  lrit2=${lrit}2
  log2=${log}2
  err2=${err}2

  $lrattrim $lrit1 $lrit2 1>$log2 2>$err2
  status=$?
  if [ $status = 0 ]
  then
    echo "./lrat-trim test/trim/$lrit1 test/trim/$lrit2 # trimming succeeded"
  else
    echo "./lrat-trim test/trim/$lrit1 test/trim/$lrit2 # trimming failed"
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

for i in `ls -rS *.lrit`
do
  name=`basename $i .lrit`
  runbinary $name
done

echo "passed $runs trimming tests in 'test/trim/run.sh'"

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

  count=1

  for opts in "" " -t" " -v" " -t -v"
  do

    log=$name.log$count
    err=$name.err$count

    $lrattrim$opts $cnf $lrat 1>$log 2>$err
    status=$?

    if [ $status = 1 ]
    then
      echo "./lrat-trim$opts test/fail/$cnf test/fail/$lrat # checking failed as expected"
    else
      echo "./lrat-trim$opts test/fail/$cnf test/fail/$lrat # unexpected exit code $status"
      exit 1
    fi

    count=`expr $count + 1`

  done
}

run empty
run blocked
run nounit1
run delnonexist1
run delnonexist2
run deltwice
run cidtoosmall1
run cidtoosmall2

runs=`grep '^run [a-z]' run.sh|wc -l`

cnfs=`ls *.cnf|wc -l`
lrats=`ls *.lrat|wc -l`

[ $runs = $cnfs ] || die "found $runs runs in './run.sh' but $cnfs '.cnf' files"
[ $runs = $lrats ] || die "found $runs runs in './run.sh' but $lrats '.lrat' files"

i=1
while [ $i -lt $count ]
do
  logs=`ls *.log$i|wc -l`
  errs=`ls *.err$i|wc -l`
  [ $runs = $logs ] || die "found $runs runs in './run.sh' but $logs '.log$i' files"
  [ $runs = $errs ] || die "found $runs runs in './run.sh' but $errs '.err$i' files"
  i=`expr $i + 1`
done

echo "passed $runs tests in 'test/fail/run.sh'"

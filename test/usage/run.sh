#!/bin/sh

die () {
  echo "lrat-trim/test/usage: $*" 1>&2
  exit 1
}

cd `dirname $0`

rm -f *.err* *.log* *.lrat[12] *.cnf[12]

lrattrim=../../lrat-trim

[ -f $lrattrim ] || die "could not find 'lrat-trim'"

runs=0

run () {
  expected=$1
  shift
  name=$1
  shift
  pretty="./lrat-trim"
  cmd="$lrattrim"
  while [ $# -gt 0 ]
  do
    case $1 in
      -*) pretty="$pretty $1"; cmd="$cmd $1";;
      *) pretty="$pretty test/usage/$1"; cmd="$cmd $1";;
    esac
    shift
  done
  log=$name.log
  err=$name.err
  #echo "pretty: $pretty"
  #echo "cmd:    $cmd >$log 2>$err"
  $cmd 1>$log 2>$err
  status=$?

  if [ $status = $expected ]
  then
    echo "$pretty # succeeded with expected exit code '$status'"
  else
    echo "$pretty # failed with exit code '$status' (expected '$expected')"
    exit 1
  fi

  runs=`expr $runs + 1`
}

run 0 options1 -h
run 0 options2 --help
run 0 version1 -V
run 0 version2 --version

echo "passed $runs usage tests in 'test/usage/run.sh'"

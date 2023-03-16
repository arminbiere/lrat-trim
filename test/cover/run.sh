#dd!/bin/sh

die () {
  echo "lrat-trim/test/cover: $*" 1>&2
  exit 1
}

cd `dirname $0`

rm -f *.err* *.log* *.cnf1 *.lrat1

lrattrim=../../lrat-trim

[ -f $lrattrim ] || die "could not find 'lrat-trim'"

runs=0

run_cover_allocations () {
  expected=$1
  shift
  name=$1
  shift
  pretty="./lrat-trim"
  cmd="$lrattrim"
  while [ $# -gt 0 ]
  do
    case $1 in
      -*|/dev/null) pretty="$pretty $1"; cmd="$cmd $1";;
      *) pretty="$pretty test/cover/$1"; cmd="$cmd $1";;
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
    echo "$pretty # '$name' succeeded with expected exit code '$status'"
  else
    echo "$pretty # '$name' failed with exit code '$status' (expected '$expected')"
    exit 1
  fi

  runs=`expr $runs + 1`

  numallocations=`awk 'BEGIN{n=0}/^c COVERED allocation/{n++}END{print n}' $log`
  if [ $numallocations -gt 0 ]
  then
    echo "rerunning '$name' for all covered '$numallocations' allocations"  
    allocations=`awk '/^c COVERED allocation/{print \$(NF-1)}' $log`
    for limit in $allocations
    do
      LRAT_TRIM_ALLOCATION_LIMIT=$limit $cmd 1>${log}$limit 2>${err}$limit
      status=$?
      if [ $status = 1 ]
      then
	echo "LRAT_TRIM_ALLOCATION_LIMIT=$limit $pretty # succeeded with expected exit code '1'"
      else
	echo "LRAT_TRIM_ALLOCATION_LIMIT=$limit $pretty # failed with exit code '$status' (expected '1')"
	exit 1
      fi

      runs=`expr $runs + 1`

    done
  else
    echo "no covered allocation information produced for '$name' (use './configure --coverage' for testing coverage)"
  fi
}

run_fakes () {
  expected=$1
  shift
  name=$1
  shift
  pretty="./lrat-trim"
  cmd="$lrattrim"
  while [ $# -gt 0 ]
  do
    case $1 in
      -*|/dev/null) pretty="$pretty $1"; cmd="$cmd $1";;
      *) pretty="$pretty test/cover/$1"; cmd="$cmd $1";;
    esac
    shift
  done
  log=$name.log
  err=$name.err
  $cmd 1>$log 2>$err
  status=$?
  if [ $status = $expected ]
  then
    echo "$pretty # '$name' succeeded with expected exit code '$expected'"
  else
    echo "$pretty # '$name' failed with exit code '$status' (expected '$expected')"
    exit 1
  fi

  runs=`expr $runs + 1`
}

run_cover_allocations 20 add4writeall -q add4.cnf add4.lrat add4.lrat1 add4.cnf1
run_cover_allocations 20 add128writeall -q add128.cnf add128.lrat add128.lrat1 add128.cnf1

LRAT_TRIM_FAKE_FRWRITE_FAILURE="yes" \
run_fakes 1 fakefwritefailure add4.cnf add4.lrat add4.lrat1

echo "passed $runs cover tests in 'test/cover/run.sh'"

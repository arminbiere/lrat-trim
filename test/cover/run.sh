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
    echo "$pretty # succeeded with expected exit code '$status'"
  else
    echo "$pretty # failed with exit code '$status' (expected '$expected')"
    exit 1
  fi

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
    done
  else
    echo "no covered allocation information produced for '$name' (use './configure --coverage' for testing coverage)"
  fi

  runs=`expr $runs + 1`
}

run 20 add128writeall -q add128.cnf add128.lrat add128.lrat1 add128.cnf1

echo "passed $runs cover tests in 'test/cover/run.sh'"

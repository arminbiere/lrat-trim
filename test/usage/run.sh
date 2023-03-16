#dd!/bin/sh

die () {
  echo "lrat-trim/test/usage: $*" 1>&2
  exit 1
}

cd `dirname $0`

rm -f *.err* *.log* *.lrat[12] *.cnf[12]
rm -f add4trim[12].cnf add5.cnf add5.cnf

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
    echo "$pretty # '$name' succeeded with expected exit code '$status'"
  else
    echo "$pretty # '$name' failed with exit code '$status' (expected '$expected')"
    exit 1
  fi

  runs=`expr $runs + 1`
}

run 0 options1 -h
run 0 options2 --help
run 0 version1 -V
run 0 version2 --version
run 1 invalidoption --this-is-not-a-valid-option
run 1 invalidinput this-is-no-a-file
run 1 toomanyfiles a b c d e
run 1 noinputfile
run 1 identical1 a a
run 1 identical2 a b a
run 1 identical3 a b c a
run 1 identical4 a b b
run 1 identical5 a b c b
run 1 identical6 a b c c
run 20 false false.cnf /dev/null
run 20 twocores1a1 -v twocores1.cnf twocores1a.lrat twocores1a.lrat1 twocores1a.cnf1
run 20 twocores1a2 -v twocores1a.cnf1 twocores1a.lrat1 --relax -S
run 20 twocores1a3 -v twocores1a.cnf1 twocores1a.lrat1 --relax -S --track
run 20 twocores1b4 -v twocores1.cnf twocores1b.lrat twocores1b.lrat1 twocores1b.cnf1
run 20 twocores1a5 -v twocores1.cnf twocores1b.lrat1
run 1 twocores1a6 -v twocores1a.cnf1 twocores1a.lrat1 -S
run 20 add4writeall -q add4.cnf add4.lrat add4.lrat1 add4.cnf1
run 20 add4writeallagain1 -v add4.cnf add4.lrat1
run 20 add8writeall -q add8.cnf add8.lrat add8.lrat1 add8.cnf1
run 20 add8writeallagain1 -v add8.cnf add8.lrat1 --relax
run 0 add4null1 add8.cnf /dev/null /dev/null /dev/null
run 0 add4null2 add8.cnf /dev/null /dev/null add8.cnf2
run 0 add4null3 add8.cnf /dev/null add8.lrat2 /dev/null
run 0 add4null4 add8.cnf /dev/null add8.lrat2 add8.cnf2
run 20 add4null5 add8.cnf add8.lrat /dev/null /dev/null
run 20 add4null6 add8.cnf add8.lrat /dev/null add8.cnf2
run 20 add4null7 add8.cnf add8.lrat add8.lrat2 /dev/null
run 20 add4null8 add8.cnf add8.lrat add8.lrat2 add8.cnf2
run 0 empty1 empty.cnf /dev/null -q
run 0 empty2 empty.cnf /dev/null /dev/null /dev/null
run 0 empty3 empty.cnf /dev/null - /dev/null
run 0 empty4 empty.cnf /dev/null /dev/null -
run 1 canotwrite empty.cnf /dev/null non/writable/path
run 1 proofoutfw empty.cnf /dev/null /dev/null -S
run 0 cnfws cnfws.cnf /dev/null
run 1 writenotrim1 empty.cnf /dev/null /dev/null --no-trim
run 0 writenotrim2 empty.cnf /dev/null /dev/null --no-check
run 20 falsenotrim false.cnf /dev/null --no-trim
run 20 add8notrimbw add8.cnf add8.lrat --no-trim
run 20 add8notrimfw add8.cnf add8.lrat --no-trim -S
run 0 add4nocheck add4.cnf add4.lrat add4.lrat2 add4.cnf2 --no-check
run 1 deltwiceignored1 full2.cnf deltwiceignored.lrat
run 1 deltwiceignored2 full2.cnf deltwiceignored.lrat -v
run 1 deltwiceignored3 full2.cnf deltwiceignored.lrat -v -t
run 20 deltwiceignored4 full2.cnf deltwiceignored.lrat --relax
run 20 deltwiceignored5 full2.cnf deltwiceignored.lrat -v --relax
run 20 deltwiceignored6 full2.cnf deltwiceignored.lrat -v -t --relax
run 1 singlecnfile full2.cnf
run 1 add51 add4.lrat add5.cnf
run 1 add52 add4.lrat add5.dimacs
run 0 add53 add4.lrat add5.cnf --force
run 0 add54 add4.lrat add5.dimacs --force
run 20 add4wrn add4.cnf add4.lrat --force
run 1 add4notrim add4.lrat add4notrim.lrat1 --no-trim
run 1 add4trim1 add4.cnf add4.lrat add4trim1.cnf
run 20 add4trim2 add4.cnf add4.lrat add4trim2.cnf --force
run 0 lratonlyforce1 /dev/null --force
run 0 lratonlyforce2 /dev/null --no-check
run 0 lratonlyforce3 /dev/null --forward
run 1 twodashes1 - - /dev/null
run 1 twodashes2 /dev/null /dev/null - -
run 0 stdin - </dev/null
run 0 twicenull /dev/null /dev/null
run 0 noproofascii -a /dev/null
bzip2 -d -c proofbomb.bz2 | run 0 fillbuffer -

$lrattrim -l -h >/dev/null 2>/dev/null && \
run 20 add4log add4.cnf add4.lrat -l

echo "passed $runs usage tests in 'test/usage/run.sh'"

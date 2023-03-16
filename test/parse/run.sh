#!/bin/sh

die () {
  echo "lrat-trim/test/parse: $*" 1>&2
  exit 1
}

cd `dirname $0`

rm -f *.err *log

lrattrim=../../lrat-trim

[ -f $lrattrim ] || die "could not find 'lrat-trim"

runcnf () {
  name=$1
  cnf=$name.cnf
  log=$name.log
  err=$name.err
  [ -f $cnf ] || die "can not find '$cnf'"
  $lrattrim $cnf /dev/null 1>$log 2>$err
  status=$?
  if [ $status = 1 ]
  then
    echo "lrat-trim test/parse/$cnf /dev/null # parsing failed as expected"
  else
    echo "lrat-trim test/parse/$cnf /dev/null # unexpected exit code $status"
    exit 1
  fi
}

runlrat () {
  name=$1
  lrat=$name.lrat
  log=$name.log
  err=$name.err
  [ -f $lrat ] || die "can not find '$lrat'"
  $lrattrim $lrat 1>$log 2>$err
  status=$?
  if [ $status = 1 ]
  then
    echo "lrat-trim test/parse/$lrat # parsing failed as expected"
  else
    echo "lrat-trim test/parse/$lrat # unexpected exit code $status"
    exit 1
  fi
}

runcnf clausemissing
runcnf clausesmissing
runcnf eofincomment1
runcnf eofincomment2
runcnf expectedcnfafterp
runcnf expectedigitafterpcnf
runcnf expectedigitaftervars
runcnf expectedspaceafterp
runcnf expectedspaceafterpcnf
runcnf idxexceedsvars
runcnf invalidcnfchar
runcnf litoobig1
runcnf litoobig2
runcnf nodigitaftersignedlit
runcnf nonlafterheader
runcnf nospaceafterlit
runcnf nospaceaftervars
runcnf nozero
runcnf numclausesexceedsintmax1
runcnf numclausesexceedsintmax2
runcnf numvarsexceedsintmax1
runcnf numvarsexceedsintmax2
runcnf toomanyclauses
runcnf unexpectedchar
runcnf zeroaftersignedlit
runcnf zerovar

runlrat addantedoublezero
runlrat addantetoobig1
runlrat addantetoobig2
runlrat addantetoobig3
runlrat addeof1
runlrat addeof2
runlrat addlitdoublezero
runlrat addlitoobig1
runlrat addlitoobig2
runlrat addlitoobig3
runlrat addnodigafterantesign
runlrat addnodigafterlitsign
runlrat addnoinc
runlrat addnonlafterzero
runlrat addnospaceafterante
runlrat addnospaceafterlit
runlrat addnospaceafterzero
runlrat addzeroafterantedsign
runlrat addzeroafterlitsign
runlrat creof
runlrat delnodigit1
runlrat delnodigit2
runlrat delnoinc
runlrat delnonl
runlrat deltoobig1
runlrat deltoobig2
runlrat deltoobig3
runlrat deltwice
runlrat delunexpdig
runlrat deof
runlrat dnospace
runlrat idnospace
runlrat idtoobig1
runlrat idtoobig2
runlrat nodigitatid
runlrat zeroneid

lratruns=`grep '^runlrat [a-z]' run.sh|wc -l`
cnfruns=`grep '^runcnf [a-z]' run.sh|wc -l`
runs=`expr $lratruns + $cnfruns`

logs=`ls *.log|wc -l`
errs=`ls *.err|wc -l`
cnfs=`ls *.cnf|wc -l`
lrats=`ls *.lrat|wc -l`

[ $runs = $logs ] || die "found $runs runs in './run.sh' but $logs '.log' files"
[ $runs = $errs ] || die "found $runs runs in './run.sh' but $errs '.err' files"
[ $cnfruns = $cnfs ] || die "found $cnfruns CNF runs in './run.sh' but $cnfs '.cnf' files"
[ $lratruns = $lrats ] || die "found $lartruns LRAT runs in './run.sh' but $lrats '.lrat' files"

echo "passed $runs parsing tests in 'test/parse/run.sh' ($cnfruns CNFs, $lratruns LRATs)"

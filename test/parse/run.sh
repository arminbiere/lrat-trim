#!/bin/sh

die () {
  echo "lrat-trim/test/parse: $*" 1>&2
  exit 1
}

cd `dirname $0`

rm -f *.err? *.log?

lrattrim=../../lrat-trim

[ -f $lrattrim ] || die "could not find 'lrat-trim'"

runcnf () {
  name=$1
  cnf=$name.cnf
  [ -f $cnf ] || die "can not find '$cnf'"

  log1=$name.log1
  err1=$name.err1

  $lrattrim $cnf /dev/null 1>$log1 2>$err1
  status=$?
  if [ $status = 1 ]
  then
    echo "./lrat-trim test/parse/$cnf /dev/null # parsing failed as expected"
  else
    echo "./lrat-trim test/parse/$cnf /dev/null # unexpected exit code $status"
    exit 1
  fi

  log2=$name.log2
  err2=$name.err2

  $lrattrim -t -v $cnf /dev/null 1>$log2 2>$err2
  status=$?
  if [ $status = 1 ]
  then
    echo "./lrat-trim -t -v test/parse/$cnf /dev/null # parsing failed as expected"
  else
    echo "./lrat-trim -t -v test/parse/$cnf /dev/null # unexpected exit code $status"
    exit 1
  fi
}

runlrat () {
  name=$1
  lrat=$name.lrat

  [ -f $lrat ] || die "can not find '$lrat'"

  log1=$name.log1
  err1=$name.err1

  $lrattrim $lrat 1>$log1 2>$err1
  status=$?
  if [ $status = 1 ]
  then
    echo "./lrat-trim test/parse/$lrat # parsing failed as expected"
  else
    echo "./lrat-trim test/parse/$lrat # unexpected exit code $status"
    exit 1
  fi

  log2=$name.log2
  err2=$name.err2

  $lrattrim -t -v $lrat 1>$log2 2>$err2
  status=$?
  if [ $status = 1 ]
  then
    echo "./lrat-trim -t -v test/parse/$lrat # parsing failed as expected"
  else
    echo "./lrat-trim -t -v test/parse/$lrat # unexpected exit code $status"
    exit 1
  fi
}

runlrit () {
  name=$1
  lrit=$name.lrit

  [ -f $lrit ] || die "can not find '$lrit'"

  log1=$name.log1
  err1=$name.err1

  $lrattrim $lrit 1>$log1 2>$err1
  status=$?
  if [ $status = 1 ]
  then
    echo "./lrat-trim test/parse/$lrit # parsing failed as expected"
  else
    echo "./lrat-trim test/parse/$lrit # unexpected exit code $status"
    exit 1
  fi

  log2=$name.log2
  err2=$name.err2

  $lrattrim -t -v $lrit 1>$log2 2>$err2
  status=$?
  if [ $status = 1 ]
  then
    echo "./lrat-trim -t -v test/parse/$lrit # parsing failed as expected"
  else
    echo "./lrat-trim -t -v test/parse/$lrit # unexpected exit code $status"
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
runlrat antenonexist
runlrat antedel
runlrat antenonl
runlrat nondigitatlinestart

runlrit eofid1
runlrit eofid2
runlrit invalidzeroinid
runlrit invalidexcessiveid
runlrit eofdnte1
runlrit eofdnte2
runlrit invalidodddnte
runlrit invalidzeroindnte
runlrit invalidexcessivednte
runlrit eoflit1
runlrit eoflit2
runlrit eofexcessivelit
runlrit invalidzeroinlit
runlrit invalidfirstbyte
runlrit expectedaord
runlrit eofaftera
runlrit eofafterd
runlrit bincidtoolarge
runlrit binzeroclauseid
runlrit delzero
runlrit addeof
runlrit addexessiveante
runlrit addzeroante
runlrit addeofante

lritruns=`grep '^runlrit [a-z]' run.sh|wc -l`
lratruns=`grep '^runlrat [a-z]' run.sh|wc -l`
cnfruns=`grep '^runcnf [a-z]' run.sh|wc -l`
runs=`expr $lritruns + $lratruns + $cnfruns`

cnfs=`ls *.cnf|wc -l`
lrats=`ls *.lrat|wc -l`
lrits=`ls *.lrit|wc -l`

log1s=`ls *.log1|wc -l`
err1s=`ls *.err1|wc -l`
log2s=`ls *.log2|wc -l`
err2s=`ls *.err2|wc -l`

[ $cnfruns = $cnfs ] || die "found $cnfruns CNF runs in './run.sh' but $cnfs '.cnf' files"
[ $lratruns = $lrats ] || die "found $lartruns LRAT runs in './run.sh' but $lrats '.lrat' files"
[ $lritruns = $lrits ] || die "found $lirtruns LRAT runs in './run.sh' but $lrits '.lrit' files"

[ $runs = $log1s ] || die "found $runs runs in './run.sh' but $log1s '.log' files"
[ $runs = $err1s ] || die "found $runs runs in './run.sh' but $err1s '.err' files"
[ $runs = $log2s ] || die "found $runs runs in './run.sh' but $log2s '.log' files"
[ $runs = $err2s ] || die "found $runs runs in './run.sh' but $err2s '.err' files"

echo "passed $runs parsing tests in 'test/parse/run.sh' ($cnfruns CNFs, $lratruns LRATs, $lritruns LRITs)"

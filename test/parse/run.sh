#!/bin/sh
die () {
  echo "lrat-trim/test/parse: $*" 1>&2
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
  if [ $status = 1 ]
  then
    echo "parsing with 'lart-trim test/parse/$lrat' failed as expected"
  else
    echo "parsing with 'lart-trim test/parse/$lrat' succeeded unexpectedly"
    exit 1
  fi
}

run addantedoublezero
run addantetoobig1
run addantetoobig2
run addantetoobig3
run addeof1
run addeof2
run addlitdoublezero
run addlitoobig1
run addlitoobig2
run addlitoobig3
run addnodigafterantesign
run addnodigafterlitsign
run addnoinc
run addnonlafterzero
run addnospaceafterante
run addnospaceafterlit
run addnospaceafterzero
run addzeroafterantedsign
run addzeroafterlitsign
run creof
run delnodigit1
run delnodigit2
run delnoinc
run delnonl
run deltoobig1
run deltoobig2
run deltoobig3
run deltwice
run delunexpdig
run deof
run dnospace
run empty
run idnospace
run idtoobig1
run idtoobig2
run nodigitatid
run zeroid

runs=`grep '^run [a-z]' run.sh|wc -l`
logs=`ls *.log|wc -l`
errs=`ls *.err|wc -l`
lrats=`ls *lrat|wc -l`

[ $runs = $logs ] || die "found $runs runs in './run.sh' but $logs '.log' files"
[ $runs = $errs ] || die "found $runs runs in './run.sh' but $errs '.err' files"
[ $runs = $lrats ] || die "found $runs runs in './run.sh' but $lrats '.lrat' files"

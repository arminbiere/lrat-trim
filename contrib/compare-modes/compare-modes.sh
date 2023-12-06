#!/bin/sh
usage () {
cat <<EOF
usage: compare-modes.sh [-v|-h] <cnf>
EOF
}
die () {
  echo "compare-modes.sh: error: $*" 1>&2
  exit 1
}
for needed in runlim cadical lrat-trim
do
  $needed --version 1>/dev/null 2>/dev/null || \
    die "could not find '$needed' in your path"
done
cnf=none
verbose=""
while [ $# -gt 0 ]
do
  case "$1" in
    -h) usage; exit 0;;
    -v) verbose=" -v";;
    *)
      [ "$cnf" = none ] || die "multiple CNF files '$cnf' and '$1'"
      [ -f "$1" ] || die "expected file but got '$1'"
      case "$1" in
	*.cnf);;
	*) die "file '$1' does not have '.cnf' extension";;
      esac
      cnf="$1"
      ;;
  esac
  shift
done

[ "$cnf" = none ] && die "CNF file argument missing"

printf "%-20s %12s %12s %12s\n" mode "time (sec)" "memory (MB)" "disk (MB)"
for mode in \
  plain-solving.sh \
  only-produce-proof.sh
do
  name=`basename $mode .sh`
  echo -n "$name"
  [ x"$verbose" = x ] || echo
  out=$name.out
  proof=$name.proof
  trimmed=$name.trimmed
  rm -f $proof $trimmed
  touch $proof $trimmed
  runlim -o $out ./$mode "$cnf"$verbose
  time=`awk '/real:/{print $3}' $out`
  memory=`awk '/space:/{print $3}' $out`
  disk=`ls -l $proof $trimmed | awk '{s+=$5/1024/1024}END{printf "%.0f", s}'`
  printf '\r%-20s %12.2f %12d %12d\n' $name $time $memory $disk
done

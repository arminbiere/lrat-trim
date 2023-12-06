#!/bin/sh
usage () {
cat <<EOF
usage: compare-modes.sh [ <option> ... ] <cnf>
-h print this comand line option summary
-v increase verbosity level (could be repeated)
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
verbosity=0
while [ $# -gt 0 ]
do
  case "$1" in
    -h) usage; exit 0;;
    -v) verbosity=`expr $verbosity + 1`;;
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

printf "%-40s %12s %12s %12s\n" mode "time (sec)" "memory (MB)" "disk (MB)"
for mode in \
  plain-solving.sh \
  only-produce-proof.sh \
  produce-and-trim-proof.sh \
  produce-and-check-proof.sh \
  produce-and-forward-check-proof.sh \
  produce-and-forward-online-check-proof.sh \
  produce-and-online-check-proof.sh
do
  name=`basename $mode .sh`
  echo -n "$name"
  [ $verbosity -gt 1 ] && echo
  out=$name.out
  proof=$name.proof
  trimmed=$name.trimmed
  rm -f $proof $trimmed
  touch $proof $trimmed
  export verbosity
  runlim -o $out ./$mode "$cnf"$verbose
  time=`awk '/real:/{print $3}' $out`
  memory=`awk '/space:/{print $3}' $out`
  disk=`ls -l $proof $trimmed | awk '{s+=$5/1024/1024}END{printf "%.0f", s}'`
  [ $verbosity = 0 ] && printf '\r'
  printf '%-40s %12.2f %12d %12d\n' $name $time $memory $disk
done

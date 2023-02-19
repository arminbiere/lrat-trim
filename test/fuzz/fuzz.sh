#!/bin/sh

# Number of parallel runs.

cores=4

# End of hard-coded configuration.

die () {
  echo "lrat-trim/test/fuzz/fuzz.sh: error: $*" 1>&2
  exit 1
}

msg () {
  echo "[lrat-trim/test/fuzz/fuzz.sh] $*"
}

[ x"`runcnfuzz -h 2>/dev/null`" = x ] && \
  die "could not find 'runcnfuzz' script'"

trap "killall runcnfuzz" 2 10 15

i=1
while [ $i -lt $cores ]
do
  runcnfuzz -i ./run.sh &
  i=`expr $i + 1`
done
wait


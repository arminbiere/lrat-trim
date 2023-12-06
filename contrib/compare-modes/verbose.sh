if [ $verbosity = 0 ]
then
  silent=" -q"
  set +x
  exec 1>/dev/null 2>/dev/null
elif [ $verbosity = 1 ]
then
  silent=" -q"
  set -x
else
  silent=""
  set -x
fi

# Semi-manual Fuzzing

You need 'radical' to produce LRAT proofs and then also the 'runcnfuzz'
script from 'cnfuzz', as well as the 'cnfuzz' and 'cnfdd' binaries all in
your path.  Then `./fuzz.sh` (or just 'make') will run multiple times the
'runcnfuzz' in parallel to produce delta-debugged CNFs that lead to a bug
using 'run.sh' to produce an LRAT proof with 'radical' and then check it
with 'lrat-trim'.  The exact checking mode is set in 'run.sh'.

If for instance 'red-1019640895.cnf' is such failing delta-debugged CNF
file, then you could first save it with 'cp red-1019640895.cnf bug.cnf' and
then issue './produce.sh bug.cnf bug.lrat' before debugging the failure,
i.e., running 'lrat-trim bug.cnf bug.lrat`.  You can delete all the
generated CNFs and log files from the fuzzing with 'make clean'.

## Version 0.2.0

- The proof parser now ignores comments in proof files which allows to
  interleave the output of the SAT solver and its proof via the same
  pipe to `lrat-trim` as in 
  ```
  cadical prime65537.cnf --lrat - | lrat-trim prime65537.cnf -
  ```
  Note that CaDiCaL is in this example instructed to write the proof to
  stdout and `lrat-trim` vice versa to read it from stdin, both through
  giving a dash `-` as second file argument.

- Ids in binary proofs are now all signed also for the clause id when
  adding a clause, which was not the case before. This matches the
  corresponding change in LRAT proof tracing in CaDiCaL 1.9.0.

- Binary proof format parser checks antecedents to be valid.

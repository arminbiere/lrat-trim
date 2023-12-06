Comparing Input Modes
---------------------

The `compare-modes.sh` script performs some simple measurements of
time, memory and disk usage of different modes of using `lrat-trim`.
It requires `runlim`, `cadical` as well as `lrat-trim` to be installed
in your path.

A simple example is produced with the provided `Makefile`:

```
$ make
./compare-modes.sh prime4294967297.cnf
mode                                       time (sec)  memory (MB)    disk (MB)
plain-solving                                    1.30            9            0
only-produce-proof                               1.33            9           15
produce-and-trim-proof                           1.45           30           26
produce-and-check-proof                          1.43           30           15
produce-and-forward-check-proof                  1.40            9           15
produce-and-forward-online-check-proof           1.39           13            0
produce-and-online-check-proof                   1.49           35            0
```

// clang-format off

static const char * usage =
"usage: lrat-trim [ <option> ... ] <input-lrat-proof> <output-lrat-proof\n"
"\n"
"where '<option>' is one of the following:\n"
"\n"
"  -h   print this command line option summary\n"
"  -v   enable verbose messages\n"
"  -q   no messages (quiet)\n" 
;

// clang-format on

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct file {
  const char *path;
  FILE * file;
  size_t bytes;
  size_t lines;
  int binary;
  int close;
};

struct file input, output;

static int verbosity;

static void die (const char *, ...) __attribute__ ((format (printf, 1, 2)));
static void msg (const char *, ...) __attribute__ ((format (printf, 1, 2)));
static void vrb (const char *, ...) __attribute__ ((format (printf, 1, 2)));

static void die (const char *fmt, ...) {
  fputs ("lrat-trim: error: ", stderr);
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

static void msg (const char *fmt, ...) {
  if (verbosity < 0)
    return;
  va_list ap;
  fputs ("c ", stdout);
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  fputc ('\n', stdout);
  fflush (stdout);
}

static void vrb (const char *fmt, ...) {
  if (verbosity < 1)
    return;
  va_list ap;
  fputs ("c ", stdout);
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  fputc ('\n', stdout);
  fflush (stdout);
}

int main (int argc, char **argv) {

  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp (arg, "-h")) {
      fputs (usage, stdout);
      exit (0);
    } else if (!strcmp (arg, "-q"))
      verbosity = -1;
    else if (!strcmp (arg, "-v"))
      verbosity = 1;
    else
      die ("invalid option '%s' (try '-h')", arg);
  }

  if (!input.path)
    die ("no input proof given");
  if (!output.path)
    die ("no output proof given");

  if (!strcmp (input.path, "-")) {
    input.file = stdin;
    input.path = "<stdin>";
    assert (!input.close);
  } else if (!(input.file = fopen (input.path, "r")))
    die ("can not read input proof file '%s'", input.path);
  else
    input.close = 1;
  msg ("reading '%s'", input.path);
  if (input.close)
    fclose (input.file);
  vrb ("read %zu lines %.0f MB", 
       input.lines, input.bytes / (double) (1u <<20));

  if (!strcmp (output.path, "-")) {
    output.file = stdout;
    output.path = "<stdout>";
    assert (!output.close);
  } else if (!(output.file = fopen (output.path, "w")))
    die ("can not write output proof file '%s'", output.path);
  if (output.close)
    fclose (output.file);
  vrb ("wrote %zu lines %.0f MB", 
       output.lines, output.bytes / (double) (1u <<20));

  return 0;
}

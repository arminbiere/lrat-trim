// clang-format off

static const char * usage =
"usage: lrat-trim [ <option> ... ] <input-lrat-proof> [ <output-lrat-proof ]\n"
"\n"
"where '<option>' is one of the following:\n"
"\n"
"  -h   print this command line option summary\n"
#ifdef LOGGING
"  -l   print all messages including logging messages\n"
#endif
"  -q   do not print any messages (be quiet)\n" 
"  -v   enable verbose messages\n"
"\n"
"The input proof in LRAT format is parsed, trimmed and then written to the\n"
"output file if the latter is specified.\n"
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
  FILE *file;
  size_t bytes;
  size_t lines;
  size_t added;
  size_t deleted;
  int binary;
  int close;
};

struct file input, output;

static int verbosity;

#if 0

struct char_stack {
  char *begin, *end, *allocated;
};

#endif

struct int_stack {
  char *begin, *end, *allocated;
};

struct size_map {
  size_t *begin, *end;
};

struct ints_stack {
  int **begin, **end, **allocated;
};

static struct int_stack line;
static struct ints_stack clauses;
static struct size_map deleted;

static void die (const char *, ...) __attribute__ ((format (printf, 1, 2)));
static void err (const char *, ...) __attribute__ ((format (printf, 1, 2)));
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

static void err (const char *fmt, ...) {
  fprintf (stderr,
           "lrat-trim: parse error in '%s' at line %zu: ", input.path,
           input.lines + 1);
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
  fputs ("c ", stdout);
  va_list ap;
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  fputc ('\n', stdout);
  fflush (stdout);
}

static void vrb (const char *fmt, ...) {
  if (verbosity < 1)
    return;
  fputs ("c ", stdout);
  va_list ap;
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  fputc ('\n', stdout);
  fflush (stdout);
}

#define SIZE(STACK) ((STACK).end - (STACK).begin)
#define FULL(STACK) ((STACK).end == (STACK).allocated)

#define ENLARGE(STACK) \
  do { \
    size_t OLD_CAPACITY = SIZE (STACK); \
    size_t NEW_CAPACITY = OLD_CAPACITY ? 2 * OLD_CAPACITY : 1; \
    size_t NEW_BYTES = NEW_CAPACITY * sizeof *(STACK).begin; \
    if (!((STACK).begin = realloc ((STACK).begin, NEW_BYTES))) \
      die ("out-of-memory enlarging '" #STACK "' stack"); \
    (STACK).end = (STACK).begin + OLD_CAPACITY; \
    (STACK).allocated = (STACK).begin + NEW_CAPACITY; \
  } while (0)

#define PUSH(STACK, DATA) \
  do { \
    if (FULL (STACK)) \
      ENLARGE (STACK); \
    *(STACK).end++ = (DATA); \
  } while (0)

#ifdef LOGGING

static int logging () { return verbosity == INT_MAX; }

static void debug (const char *, ...)
    __attribute__ ((format (printf, 1, 2)));

static void debug_clause (int *, const char *, ...)
    __attribute__ ((format (printf, 2, 3)));

static void debug (const char *fmt, ...) {
  if (!logging ())
    return;
  fputs ("c LOGGING ", stdout);
  va_list ap;
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  fputc ('\n', stdout);
  fflush (stdout);
}

static void debug_clause (int *c, const char *fmt, ...) {
  assert (c);
  if (!logging ())
    return;
  fputs ("c ", stdout);
  va_list ap;
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  printf (" clause[%d]", *c);
  int *p = c + 1, tmp;
  while ((tmp = *p++))
    printf (" %d", tmp);
  fputs (" 0", stdout);
  while ((tmp = *p++))
    printf (" %d", tmp);
  fputs (" 0\n", stdout);
  fflush (stdout);
}

#endif

static inline int read_char (void) {
  int res = getc_unlocked (input.file);
  if (res == '\r') {
    res = getc_unlocked (input.file);
    if (res != '\n')
      err ("carriage-return without following new-line");
  }
  if (res == '\n')
    input.lines++;
  return res;
}

#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

static double process_time () {
  struct rusage u;
  double res;
  if (getrusage (RUSAGE_SELF, &u))
    return 0;
  res = u.ru_utime.tv_sec + 1e-6 * u.ru_utime.tv_usec;
  res += u.ru_stime.tv_sec + 1e-6 * u.ru_stime.tv_usec;
  return res;
}

static size_t maximum_resident_set_size (void) {
  struct rusage u;
  if (getrusage (RUSAGE_SELF, &u))
    return 0;
  return ((size_t)u.ru_maxrss) << 10;
}

static double mega_bytes (void) {
  return maximum_resident_set_size () / (double)(1 << 20);
}

static double percent (double a, double b) { return b ? a / b : 0; }

int main (int argc, char **argv) {

  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp (arg, "-h")) {
      fputs (usage, stdout);
      exit (0);
    } else if (!strcmp (arg, "-l"))
      verbosity = INT_MAX;
    else if (!strcmp (arg, "-q"))
      verbosity = -1;
    else if (!strcmp (arg, "-v")) {
      if (verbosity <= 0)
        verbosity = 1;
    } else if (arg[0] == '-' && arg[1])
      die ("invalid option '%s' (try '-h')", arg);
    else if (output.path)
      die ("too many arguments '%s', '%s' and '%s'", input.path,
           output.path, arg);
    else if (input.path)
      output.path = arg;
    else
      input.path = arg;
  }

  if (!input.path)
    die ("no input proof given");

  if (output.path && !strcmp (input.path, output.path) &&
      strcmp (input.path, "-") && strcmp (input.path, "/dev/null"))
    die ("input and output path are both '%s'", input.path);

  if (!strcmp (input.path, "-")) {
    input.file = stdin;
    input.path = "<stdin>";
    assert (!input.close);
  } else if (!(input.file = fopen (input.path, "r")))
    die ("can not read input proof file '%s'", input.path);
  else
    input.close = 1;
  msg ("reading '%s'", input.path);
  int ch, empty = 0, last_id = 0, min_added = 0;
  for (;;) {
    ch = read_char ();
    if (ch == EOF)
      break;
    if (!isdigit (ch))
      err ("expected digit as first character of line");
    if (ch == '0')
      err ("expected non-zero digit as first character of line");
    int id = (ch - '0');
    while (isdigit (ch = read_char ())) {
      if (INT_MAX / 10 < id)
        err ("line identifier exceeds 'INT_MAX'");
      id *= 10;
      int digit = (ch - '0');
      if (INT_MAX - digit < id)
        err ("line identifier exceeds 'INT_MAX'");
      id += digit;
    }
    if (ch != ' ')
      err ("expected space after line identifier '%d'", id);
    if (id < last_id)
      err ("line identifier '%d' smaller than last '%d'", id, last_id);
    ch = read_char ();
    if (ch == 'd') {
      ch = read_char ();
      if (ch != ' ')
        err ("expected space after 'd'");
      assert (id != INT_MIN);
      PUSH (line, -id);
      int last = 0;
      for (;;) {
        ch = read_char ();
        if (!isdigit (ch)) {
          if (last)
            err ("expected digit after '%d '", last);
          else
            err ("expected digit after 'd '");
        }
        int other = ch - '0';
        while (isdigit ((ch = read_char ()))) {
          if (!other)
            err ("unexpected digit '%c' after '0'", ch);
          if (INT_MAX / 10 < other)
            err ("deleted clause identifier exceeds 'INT_MAX'");
          other *= 10;
          int digit = (ch - '0');
          if (INT_MAX - digit < other)
            err ("deleted clause identifier exceeds 'INT_MAX'");
          other += digit;
        }
        if (other) {
          if (ch != ' ')
            err ("expected space after '%d'", other);
          if (other > id)
            err ("deleted clause identifier '%d' "
                 "large than line identifier '%d'",
                 other, id);
          if (other >= min_added) {
            size_t old_size = SIZE (deleted);
            if ((size_t)other < old_size) {
              size_t new_size = old_size ? 2 * old_size : 1;
              while ((size_t)other <= new_size)
                new_size *= 2;
              size_t *old_begin = deleted.begin;
              if (!(deleted.begin = calloc (new_size, sizeof (size_t))))
                die ("out-of-memory reallocating deleted table");
              free (old_begin);
              deleted.end = deleted.begin + new_size;
            }
	    size_t * deleted_pointer = deleted.begin + other;
            size_t deleted_before = *deleted_pointer;
            if (deleted_before)
              err ("clause[%d] already deleted in line %zu",
                   other, deleted_before);
	    *deleted_pointer = input.lines + 1;
          }
          last = other;
        } else {
          if (ch != '\n')
            err ("expected new-line after '0' at end of deletion line");
          break;
        }
      }
    } else {
      if (id == last_id)
        err ("line identifier '%d' of addition line does not increase", id);
      line.end = line.begin;
      PUSH (line, id);
      while ((ch = read_char ()) != '\n')
        if (ch == EOF)
          err ("unexpected end-of-file in clause addition line");
      if (!min_added) {
        debug ("first added clauses clause[%d]", id);
        min_added = id;
      }
    }
    last_id = id;
  }
  if (input.close)
    fclose (input.file);
  if (!empty)
    die ("no empty clause added in '%s'", input.path);

  vrb ("read %zu lines %.0f MB", input.lines,
       input.bytes / (double)(1 << 20));

  vrb ("parsing finished in %.2f seconds and used %.0f MB", process_time (),
       mega_bytes ());

  free (line.begin);

  if (output.path) {
    if (!strcmp (output.path, "-")) {
      output.file = stdout;
      output.path = "<stdout>";
      assert (!output.close);
    } else if (!(output.file = fopen (output.path, "w")))
      die ("can not write output proof file '%s'", output.path);
    msg ("writing '%s'", output.path);
    if (output.close)
      fclose (output.file);
    vrb ("wrote %zu lines %.0f MB", output.lines,
         output.bytes / (double)(1 << 20));
  } else
    msg ("no output file specified");

  for (int **p = clauses.begin; p != clauses.end; p++)
    if (*p)
      free (*p);
  free (clauses.begin);

  free (deleted.begin);

  msg ("trimmed %9zu addition lines to %9zu lines %3.0f%%", input.added,
       output.added, percent (output.added, input.added));
  msg ("trimmed %9zu deletion lines to %9zu lines %3.0f%%", input.deleted,
       output.deleted, percent (output.deleted, input.deleted));
  msg ("trimmed %9zu lines          to %9zu lines %3.0f%%", input.lines,
       output.lines, percent (output.lines, input.lines));

  if (output.path)
    msg ("trimmed %9.0f MB             to %9.0f MB    %3.0f%%",
         input.bytes / (double)(1 << 20), output.bytes / (double)(1 << 20),
         percent (output.bytes, input.bytes));

  msg ("used %.2f seconds and %.0f MB", process_time (), mega_bytes ());

  return 0;
}

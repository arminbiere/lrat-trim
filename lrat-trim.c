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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct file {
  const char *path;
  FILE *file;
  size_t bytes;
  size_t lines;
  int close;
};

struct bool_stack {
  bool *begin, *end, *allocated;
};

struct int_stack {
  int *begin, *end, *allocated;
};

struct ints_map {
  int **begin, **end;
};

struct deletion {
  size_t line;
  int id;
};

struct deletion_map {
  struct deletion *begin, *end;
};

struct statistics {
  struct {
    struct {
      size_t added, deleted;
    } cnf, proof;
  } original, trimmed;
} statistics;

static struct file input;
static struct file output;
static int verbosity;

static int *map;
static int *links;
static int *heads;
static int *used;

static int empty_clause;
static int first_clause_added_in_proof;

static struct int_stack work;
static struct int_stack added;
static struct ints_map literals;
static struct ints_map antecedents;
static struct deletion_map deleted;

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
#define CAPACITY(STACK) ((STACK).allocated - (STACK).begin)

#define EMPTY(STACK) ((STACK).end == (STACK).begin)
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

#define POP(STACK) (assert (!EMPTY (STACK)), *--(STACK).end)

#define CLEAR(STACK) \
  do { \
    (STACK).end = (STACK).begin; \
  } while (0)

#define RELEASE(STACK) free ((STACK).begin)

#define ADJUST(MAP, NEEDED_SIZE) \
  do { \
    size_t OLD_SIZE = SIZE (MAP); \
    size_t COPY_OF_NEEDED_SIZE = (NEEDED_SIZE); \
    if (OLD_SIZE < COPY_OF_NEEDED_SIZE) { \
      if (OLD_SIZE < COPY_OF_NEEDED_SIZE) { \
        size_t NEW_SIZE = OLD_SIZE ? 2 * OLD_SIZE : 1; \
        while (NEW_SIZE < COPY_OF_NEEDED_SIZE) \
          NEW_SIZE *= 2; \
        void *OLD_BEGIN = (MAP).begin, *NEW_BEGIN; \
        size_t NEW_BYTES = NEW_SIZE * sizeof *(MAP).begin; \
        if (OLD_BEGIN) { \
          size_t OLD_BYTES = OLD_SIZE * sizeof *(MAP).begin; \
          NEW_BEGIN = realloc (OLD_BEGIN, NEW_BYTES); \
          if (!NEW_BEGIN) \
            die ("out-of-memory adjusting '" #MAP "' map"); \
          size_t DELTA_BYTES = NEW_BYTES - OLD_BYTES; \
          memset ((char *)NEW_BEGIN + OLD_BYTES, 0, DELTA_BYTES); \
        } else { \
          NEW_BEGIN = calloc (NEW_SIZE, sizeof *(MAP).begin); \
          if (!NEW_BEGIN) \
            die ("out-of-memory initializing '" #MAP "' map"); \
        } \
        (MAP).begin = NEW_BEGIN; \
        (MAP).end = (MAP).begin + NEW_SIZE; \
      } \
      (MAP).end = (MAP).begin + COPY_OF_NEEDED_SIZE; \
    } \
  } while (0)

#ifndef NDEBUG

static void release_ints_map (struct ints_map *map) {
  int **begin = map->begin;
  int **end = map->end;
  for (int **p = begin; p != end; p++)
    if (*p)
      free (*p);
  free (begin);
}

#endif

#ifdef LOGGING

static bool logging () { return verbosity == INT_MAX; }

static void logging_prefix (const char *, ...)
    __attribute__ ((format (printf, 1, 2)));

static void logging_prefix (const char *fmt, ...) {
  assert (logging ());
  fputs ("c LOGGING ", stdout);
  va_list ap;
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
}

static void logging_suffix () {
  assert (logging ());
  fputc ('\n', stdout);
  fflush (stdout);
}

#define dbg(...) \
  do { \
    if (!logging ()) \
      break; \
    logging_prefix (__VA_ARGS__); \
    logging_suffix (); \
  } while (0)

#define dbgs(INTS, ...) \
  do { \
    if (!logging ()) \
      break; \
    logging_prefix (__VA_ARGS__); \
    const int *P = (INTS); \
    while (*P) \
      printf (" %d", *P++); \
    logging_suffix (); \
  } while (0)

#else

#define dbg(...) \
  do { \
  } while (0)

#define dbgs(...) \
  do { \
  } while (0)

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
  if (res != EOF)
    input.bytes++;
  return res;
}

static inline void write_char (unsigned ch) {
  assert (output.file);
  fputc_unlocked (ch, output.file);
  output.bytes++;
  if (ch == '\n')
    output.lines++;
}

static inline void write_new_line () { write_char ('\n'); }

static inline void write_space () { write_char (' '); }

static inline void write_str (const char * str) {
  for (const char * p = str; *p; p++)
    write_char (*p);
}

static inline void write_int (int i) {
  char buffer[16];
  sprintf (buffer, "%d", i);
  write_str (buffer);
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

static double average (double a, double b) { return b ? a / b : 0; }
static double percent (double a, double b) { return average (100 * a, b); }

static bool is_original_clause (int id) {
  assert (0 < id);
  return !first_clause_added_in_proof || id < first_clause_added_in_proof;
}

static bool mark_used (int id, int used_where) {
  assert (0 < id);
  assert (0 < used_where);
  int *w = used + id;
  int used_before = *w;
  if (used_before >= used_where)
    return true;
  *w = used_where;
  dbg ("updated clause %d to be used in clause %d", id, used_where);
  if (used_before)
    return true;
  if (is_original_clause (id))
    statistics.trimmed.cnf.added++;
  else
    statistics.trimmed.proof.added++;
  return false;
}

static bool marked_added (int id) {
  assert (0 < id);
  if (id >= SIZE (added))
    return false;
  return added.begin[id];
}

static bool has_been_added (int id) {
  assert (0 < id);
  return is_original_clause (id) || marked_added (id);
}

static int map_id (int id) {
  assert (id != INT_MIN);
  int abs_id = abs (id);
  int res = id < first_clause_added_in_proof ? id : map[abs_id];
  if (id < 0)
    res = -res;
  return res;
}

static const char *exceeds_int_max (int n, int ch) {
  static char buffer[32];
  const size_t size = sizeof buffer - 5;
  assert (isdigit (ch));
  sprintf (buffer, "%d", n);
  size_t i = strlen (buffer);
  do {
    assert (i < sizeof buffer);
    buffer[i++] = ch;
  } while (i < size && isdigit (ch = read_char ()));
  if (ch == '\n') {
    assert (input.lines);
    input.lines--;
  }
  if (i == size) {
    assert (i + 3 < sizeof buffer);
    buffer[i++] = '.';
    buffer[i++] = '.';
    buffer[i++] = '.';
  }
  assert (i < sizeof buffer);
  buffer[i] = 0;
  return buffer;
}

int main (int argc, char **argv) {

  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp (arg, "-h")) {
      fputs (usage, stdout);
      exit (0);
    } else if (!strcmp (arg, "-l"))
#ifdef LOGGING
      verbosity = INT_MAX;
#else
      die ("invalid option '-l' (build without logging support)");
#endif
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
  int ch, last_id = 0;
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
      assert (id);
      if (INT_MAX / 10 < id)
      LINE_IDENTIFIER_EXCEEDS_INT_MAX:
        err ("line identifier '%s' exceeds 'INT_MAX'",
             exceeds_int_max (id, ch));
      id *= 10;
      int digit = (ch - '0');
      if (INT_MAX - digit < id) {
        id /= 10;
        goto LINE_IDENTIFIER_EXCEEDS_INT_MAX;
      }
      id += digit;
    }
    if (ch != ' ')
      err ("expected space after identifier '%d'", id);
    if (id < last_id)
      err ("identifier '%d' smaller than last '%d'", id, last_id);
    ch = read_char ();
    if (ch == 'd') {
      ch = read_char ();
      if (ch != ' ')
        err ("expected space after '%d d' in deletion %d", id, id);
      assert (EMPTY (work));
      int last = 0;
      do {
        ch = read_char ();
        if (!isdigit (ch)) {
          if (last)
            err ("expected digit after '%d ' in deletion %d", last, id);
          else
            err ("expected digit after '%d d ' in deletion %d", id, id);
        }
        int other = ch - '0';
        while (isdigit ((ch = read_char ()))) {
          if (!other)
            err ("unexpected digit '%c' after '0' in deletion %d", ch, id);
          if (INT_MAX / 10 < other)
          DELETED_CLAUSE_IDENTIFIER_EXCEEDS_INT_MAX:
            err ("deleted clause identifier '%s' exceeds 'INT_MAX' "
                 "in deletion %d",
                 exceeds_int_max (other, ch), id);
          other *= 10;
          int digit = (ch - '0');
          if (INT_MAX - digit < other) {
            other /= 10;
            goto DELETED_CLAUSE_IDENTIFIER_EXCEEDS_INT_MAX;
          }
          other += digit;
        }
        if (other) {
          if (ch != ' ')
            err ("expected space after '%d' in deletion %d", other, id);
          if (other > id)
            err ("deleted clause '%d' "
                 "larger than deletion identifier '%d'",
                 other, id);
          if (!has_been_added (other))
            err ("deleted clause '%d' in deletion %d is neither "
                 "an original clause nor has been added",
                 other, id);
          size_t needed_clauses_size = (size_t)other + 1;
          ADJUST (deleted, needed_clauses_size);
          struct deletion *d = deleted.begin + other;
          if (d->id) {
            assert (d->line);
            err ("clause %d requested to be deleted in deletion %d "
                 "was already deleted in deletion %d at line %zu",
                 other, id, d->id, d->line);
          }
          size_t deleted_line = input.lines + 1;
          dbg ("marked clause %d to be deleted at line %zu in deletion %d",
               other, deleted_line, id);
          d->line = deleted_line;
          d->id = id;
          if (is_original_clause (id))
            statistics.original.cnf.deleted++;
          else
            statistics.original.proof.deleted++;
        } else if (ch != '\n')
          err ("expected new-line after '0' at end of deletion %d", id);
#if !defined(NDEBUG) || defined(LOGGING)
        PUSH (work, other);
#endif
        last = other;
      } while (last);
#if !defined(NDEBUG) || defined(LOGGING)
      dbgs (work.begin, "parsed deletion %d and deleted clauses", id);
      CLEAR (work);
#endif
    } else {
      if (id == last_id)
        err ("line identifier '%d' of addition line does not increase", id);
      if (!first_clause_added_in_proof) {
        vrb ("adding first clause %d", id);
        first_clause_added_in_proof = id;
        if (!statistics.original.cnf.added)
          statistics.original.cnf.added = id - 1;
      }
      assert (EMPTY (work));
      bool first = true;
      int last = id;
      assert (last);
      while (last) {
        int sign;
        if (first)
          first = false;
        else
          ch = read_char ();
        if (ch == '-') {
          if (!isdigit (ch = read_char ()))
            err ("expected digit after '%d -' in clause %d", last, id);
          if (ch == '0')
            err ("expected non-zero digit after '%d -'", last);
          sign = -1;
        } else if (!isdigit (ch))
          err ("expected literal or '0' after '%d ' in clause %d", last,
               id);
        else
          sign = 1;
        int idx = ch - '0';
        while (isdigit (ch = read_char ())) {
          if (!idx)
            err ("unexpected second '%c' after '%d 0' in clause %d", ch,
                 last, id);
          if (INT_MAX / 10 < idx) {
          VARIABLE_INDEX_EXCEEDS_INT_MAX:
            if (sign < 0)
              err ("variable index in literal '-%s' "
                   "exceeds 'INT_MAX' in clause %d",
                   exceeds_int_max (idx, ch), id);
            else
              err ("variable index '%s' exceeds 'INT_MAX' in clause %d",
                   exceeds_int_max (idx, ch), id);
          }
          idx *= 10;
          int digit = ch - '0';
          if (INT_MAX - digit < idx) {
            idx /= 10;
            goto VARIABLE_INDEX_EXCEEDS_INT_MAX;
          }
          idx += digit;
        }
        int lit = sign * idx;
        if (ch != ' ') {
          if (idx)
            err ("expected space after literal '%d' in clause %d", lit, id);
          else
            err ("expected space after literals and '0' in clause %d", id);
        }
        PUSH (work, lit);
        last = lit;
      }
      dbgs (work.begin, "clause %d literals", id);
      size_t needed_clauses_size = (size_t)id + 1;
      {
        size_t size_literals = SIZE (work);
        size_t bytes_literals = size_literals * sizeof (int);
        int *l = malloc (bytes_literals);
        if (!l) {
          assert (size_literals);
          die ("out-of-memory allocating literals of size %zu clause %d",
               size_literals - 1, id);
        }
        memcpy (l, work.begin, bytes_literals);
        ADJUST (literals, needed_clauses_size);
        literals.begin[id] = l;
        if (size_literals == 1) {
          if (!empty_clause) {
            vrb ("found empty clause %d", id);
            empty_clause = id;
          }
        }
      }
      CLEAR (work);
      assert (!last);
      do {
        int sign;
        if ((ch = read_char ()) == '-') {
          if (!isdigit (ch = read_char ()))
            err ("expected digit after '%d -' in clause %d", last, id);
          if (ch == '0')
            err ("expected non-zero digit after '%d -'", last);
          sign = -1;
        } else if (!isdigit (ch))
          err ("expected clause identifier after '%d ' "
               "in clause %d",
               last, id);
        else
          sign = 1;
        int other = ch - '0';
        while (isdigit (ch = read_char ())) {
          if (!other)
            err ("unexpected second '%c' after '%d 0' in clause %d", ch,
                 last, id);
          if (INT_MAX / 10 < other) {
          ANTECEDENT_IDENTIFIER_EXCEEDS_INT_MAX:
            if (sign < 0)
              err ("antecedent '-%s' exceeds 'INT_MAX' in clause %d",
                   exceeds_int_max (other, ch), id);
            else
              err ("antecedent '%s' exceeds 'INT_MAX' in clause %d",
                   exceeds_int_max (other, ch), id);
          }
          other *= 10;
          int digit = ch - '0';
          if (INT_MAX - digit < other) {
            other /= 10;
            goto ANTECEDENT_IDENTIFIER_EXCEEDS_INT_MAX;
          }
          other += digit;
        }
        int signed_other = sign * other;
        if (other) {
          if (ch != ' ')
            err ("expected space after antecedent '%d' in clause %d",
                 signed_other, id);
          if (other >= id)
            err ("antecedent '%d' in clause %d exceeds clause",
                 signed_other, id);
          if (!has_been_added (other))
            err ("antecedent '%d' in clause %d "
                 "is neither an original clause nor has been added",
                 signed_other, id);
        } else {
          if (ch != '\n')
            err ("expected new-line after '0' at end of clause %d", id);
        }
        PUSH (work, signed_other);
        last = signed_other;
      } while (last);
      dbgs (work.begin, "clause %d antecedents", id);
      {
        size_t size_antecedents = SIZE (work);
        size_t bytes_antecedents = size_antecedents * sizeof (int);
        int *a = malloc (bytes_antecedents);
        if (!a) {
          assert (size_antecedents);
          die ("out-of-memory allocating antecedents of size %zu clause "
               "%d",
               size_antecedents - 1, id);
        }
        memcpy (a, work.begin, bytes_antecedents);
        CLEAR (work);
        ADJUST (antecedents, needed_clauses_size);
        antecedents.begin[id] = a;
      }
      ADJUST (added, needed_clauses_size);
      added.begin[id] = true;
      statistics.original.proof.added++;
    }
    last_id = id;
  }
  if (input.close)
    fclose (input.file);

  free (added.begin);
  free (deleted.begin);

  vrb ("read %zu lines %.0f MB", input.lines,
       input.bytes / (double)(1 << 20));

  vrb ("parsing finished in %.2f seconds and used %.0f MB", process_time (),
       mega_bytes ());

  if (!empty_clause)
    die ("no empty clause added in '%s'", input.path);

  size_t needed_clauses_size = (size_t)empty_clause + 1;
  used = calloc (needed_clauses_size, sizeof *used);
  if (!used)
    die ("out-of-memory allocating used stamps");

  assert (EMPTY (work));
  mark_used (empty_clause, empty_clause);
  if (!is_original_clause (empty_clause))
    PUSH (work, empty_clause);

  while (!EMPTY (work)) {
    unsigned id = POP (work);
    assert (marked_added (id));
    assert (used[id]);
    int *a = antecedents.begin[id];
    assert (a);
    for (int *p = a, other; (other = abs (*p)); p++)
      if (!mark_used (other, id) && !is_original_clause (other))
        PUSH (work, other);
  }

  msg ("trimmed %zu original clauses in CNF to %zu clauses %.0f%%",
       statistics.original.cnf.added, statistics.trimmed.cnf.added,
       percent (statistics.trimmed.cnf.added,
                statistics.original.cnf.added));

  msg ("trimmed %zu original proof lemmas to %zu clauses %.0f%%",
       statistics.original.proof.added, statistics.trimmed.proof.added,
       percent (statistics.trimmed.proof.added,
                statistics.original.proof.added));

  free (work.begin);

  if (output.path) {
    if (!strcmp (output.path, "-")) {
      output.file = stdout;
      output.path = "<stdout>";
      assert (!output.close);
    } else if (!(output.file = fopen (output.path, "w")))
      die ("can not write output proof file '%s'", output.path);
    msg ("writing '%s'", output.path);
  } else {
    msg ("no output file specified");
    assert (!output.file);
  }

  {
    for (int id = 1; id != first_clause_added_in_proof; id++)
      if (!used[id]) {
	if (output.file) {
	  if (!statistics.trimmed.cnf.deleted) {
	    write_int (first_clause_added_in_proof -1);
	    write_str (" d");
	  }
	  write_char (' ');
	  write_int (id);
	}
	statistics.trimmed.cnf.deleted++;
      }

    if (statistics.trimmed.cnf.deleted) {
      if (output.file)
	write_str (" 0\n");

      vrb ("trimmed proof has %zu CNF deleted clauses initially", 
           statistics.trimmed.cnf.deleted);
    }
  }

  {
    int id = first_clause_added_in_proof, mapped = id;
    map = calloc (needed_clauses_size, sizeof *map);
    if (!map)
      die ("out-of-memory allocating identifier map");
    links = calloc (needed_clauses_size, sizeof *links);
    if (!links)
      die ("out-of-memory allocating used links");
    heads = calloc (needed_clauses_size, sizeof *heads);
    if (!heads)
      die ("out-of-memory allocating used list headers");
    for (;;) {
      int where = used[id];
      if (where) {
        if (where != id) {
          assert (id < where);
          links[id] = heads[where];
          heads[where] = id;
          map[id] = mapped;
        } else
          assert (where == empty_clause);
        if (output.file) {
	  write_int (mapped);
          int *l = literals.begin[id];
	  assert (l);
          for (const int *p = l; *p; p++)
	    write_space (), write_int (*p);
	  write_str (" 0");
	  int *a = antecedents.begin[id];
	  assert (a);
          for (const int *p = a; *p; p++) {
	    write_space ();
	    int other = *p;
	    assert (abs (other) < id);
	    write_int (map_id (other));
	  }
	  write_str (" 0\n");
        }
        int head = heads[id];
        if (head) {
          for (int link = head, next; link; link = next) {
            if (is_original_clause (link))
              statistics.trimmed.cnf.deleted++;
            else
              statistics.trimmed.proof.deleted++;
            next = links[link];
          }
        }
        mapped++;
      }
      if (id++ == empty_clause)
        break;
    }
  }

  if (output.file) {
    if (output.close)
      fclose (output.file);
    vrb ("wrote %zu lines %.0f MB", output.lines,
         output.bytes / (double)(1 << 20));
  }

#ifndef NDEBUG
  free (map);
  free (links);
  free (heads);
  free (used);
  release_ints_map (&literals);
  release_ints_map (&antecedents);
#endif

  if (output.path)
    msg ("trimmed %zu bytes (%.0f MB) to %zu bytes (%.0f MB) %.0f%%",
         input.bytes, input.bytes / (double)(1 << 20),
	 output.bytes, output.bytes / (double)(1 << 20),
         percent (output.bytes, input.bytes));

  msg ("used %.2f seconds and %.0f MB", process_time (), mega_bytes ());

  return 0;
}

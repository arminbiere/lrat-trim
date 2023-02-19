static const char *version = "0.1.0";

// clang-format off

static const char * usage =

"usage: lrat-trim [ <option> ... ] <file> ...\n"
"\n"
"where '<option> ...' is a potentially empty list of the following options\n"
"\n"
"  -f | --force    overwrite CNF alike second file with proof\n"
"  -S | --forward  forward check all added clauses eagerly\n"
"  -h | --help     print this command line option summary\n"
#ifdef LOGGING
"  -l | --log      print all messages including logging messages\n"
#endif
"  -q | --quiet    be quiet and do not print any messages\n" 
"  -t | --track    track line information for clauses\n"
"  -v | --verbose  enable verbose messages\n"
"  -V | --version  print version only\n"
"\n"
"  --no-check      disable checking clauses (default without CNF)\n"
"  --no-trim       disable trimming (assume all clauses used)\n"
"\n"
"and '<file> ...' is a non-empty list of at most four DIMACS and LRAT files:\n"
"\n"
"  <input-proof>\n"
"  <input-cnf> <input-proof>\n"
"\n"
"  <input-proof> <output-proof>\n"
"  <input-cnf> <input-proof> <output-proof>\n"
"  <input-cnf> <input-proof> <output-proof> <output-cnf>\n"
"\n"

"The required input proof in LRAT format is parsed and trimmed and\n"
"optionally written to the output proof file if it is specified.  Otherwise\n"
"the proof is trimmed only in memory producing trimming statistics.\n"
"\n"
"If an input CNF is also specified then it is assumed to be in DIMACS format\n"
"and parsed before reading the LRAT proof.  Providing a CNF triggers to\n"
"check and not only trim a proof.  If checking fails an error message is\n"
"produced and the program aborts with exit code '1'.  If checking succeeds\n"
"the exit code is '0', if no empty clause was derived. Otherwise if the CNF\n"
"or proof contains an empty clause and checking succeeds, then the exit\n"
"code is '20', i.e., the same exit code as for unsatisfiable formulas in\n"
"the SAT competition.  In this case 's VERIFIED' is printed too.\n"
"\n"
"The status of clauses, i.e., whether they are added or have been deleted\n"
"is always tracked and checked precisely.  It is considered and error if\n"
"a clause is used in a proof line which was deleted before.  In order to\n"
"determine in which proof line exactly the offending clause was deleted\n"
"the user can additionally specify '--track' to track this information,\n"
"which can then give a more informative error message.\n"
"\n"
"If the CNF or the proof contains an empty clause, proof checking is by\n"
"default restricted to the trimmed proof.  Without empty clause, neither\n"
"in the CNF nor in the proof, trimming is skipped.  The same effect can\n"
"be achieved by using '--no-trim', which has the additional benefit to\n"
"enforce forward on-the-fly checking while parsing the proof. This mode\n"
"allows to delete clauses eagerly and gives the chance to reduce memory\n"
"usage substantially.\n"
"\n"
"At most one of the input path names can be '-' which leads to reading\n"
"the corresponding input from '<stdin>'.  Similarly using '-' for one\n"
"of the output files writes to '<stdout>'.  When exactly two files are\n"
"given the first file is opened and read first and to determine its format\n"
"(LRAT or DIMACS) by checking the first read character ('p' or 'c' gives\n"
"DIMACS format).  The result also determines the type of the second file\n"
"as either proof output or as proof input.  Two files can not have the\n"
"same specified file path except for '-' and '/dev/null'.  The latter is a\n"
"hard-coded name and will not actually be opened nor written to '/dev/null'\n"
"(whether it exists or not on the system).\n"

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
  int eof;
  int last;
  int saved;
};

struct bool_stack {
  bool *begin, *end, *allocated;
};

struct int_stack {
  int *begin, *end, *allocated;
};

struct char_map {
  signed char *begin, *end;
};

struct int_map {
  int *begin, *end;
};

struct ints_map {
  int **begin, **end;
};

struct addition {
  size_t line;
};

struct deletion {
  size_t line;
  int id;
};

struct addition_map {
  struct addition *begin, *end;
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
  struct {
    struct {
      size_t total;
      size_t empty;
    } checked;
    size_t resolved;
  } clauses;
  struct {
    size_t assigned;
  } literals;
} statistics;

// At-most three files set up during option parsing.

static struct file files[4];
static size_t size_files;

// Current input and output file for writing and reading functions.

// As we only work on one input sequentially during 'parse_proof' or
// before optionally in 'parse_cnf' we keep the current 'input' file as
// static global data structures which helps the compiler to optimize
// 'read_buffer' and 'read_char' as well code into which theses are inlined.
// In particular see the discussion below on 'faster_than_default_isdigit'.

// A similar argument applies to the 'output' file.

static struct file input, output;

struct {
  struct file *input, *output;
} cnf, proof;

static const char *force;
static const char *forward;
static const char *nocheck;
static const char *notrim;
static const char *track;
static int verbosity;

static bool checking;
static bool trimming;

static int empty_clause;
static int last_clause_added_in_cnf;
static int first_clause_added_in_proof;

static struct { struct char_map values; } variables;

static struct int_stack trail;

static struct {
  struct char_map status;
  struct ints_map literals;
  struct ints_map antecedents;
  struct deletion_map deleted;
  struct addition_map added;
  struct int_map used;
  struct int_map heads;
  struct int_map links;
  struct int_map map;
} clauses;

static void die (const char *, ...) __attribute__ ((format (printf, 1, 2)));
static void prr (const char *, ...) __attribute__ ((format (printf, 1, 2)));
static void msg (const char *, ...) __attribute__ ((format (printf, 1, 2)));
static void vrb (const char *, ...) __attribute__ ((format (printf, 1, 2)));
static void wrn (const char *, ...) __attribute__ ((format (printf, 1, 2)));

static void die (const char *fmt, ...) {
  fputs ("lrat-trim: error: ", stderr);
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

static void prr (const char *fmt, ...) {
  assert (input.path);
  size_t line = input.lines + 1;
  if (input.last == '\n')
    line--;
  fprintf (stderr,
           "lrat-trim: parse error in '%s' %s line %zu: ", input.path,
           input.eof && input.last == '\n' ? "after" : "in", line);
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

static void wrn (const char *fmt, ...) {
  if (verbosity < 0)
    return;
  fputs ("c WARNING ", stdout);
  va_list ap;
  va_start (ap, fmt);
  vprintf (fmt, ap);
  va_end (ap);
  fputc ('\n', stdout);
  fflush (stdout);
}

#define ZERO(E) \
  do { \
    memset (&(E), 0, sizeof (E)); \
  } while (0)

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

#define ACCESS(STACK, OFFSET) \
  ((STACK).begin[assert ((OFFSET) < SIZE (STACK)), (OFFSET)])

#define POP(STACK) (assert (!EMPTY (STACK)), *--(STACK).end)

#define CLEAR(STACK) \
  do { \
    (STACK).end = (STACK).begin; \
  } while (0)

#define RELEASE(STACK) free ((STACK).begin)

#define ADJUST(MAP, N) \
  do { \
    size_t NEEDED_SIZE = (size_t)(N) + 1; \
    size_t OLD_SIZE = SIZE (MAP); \
    if (OLD_SIZE >= NEEDED_SIZE) \
      break; \
    size_t NEW_SIZE = OLD_SIZE ? 2 * OLD_SIZE : 1; \
    void *OLD_BEGIN = (MAP).begin; \
    void *NEW_BEGIN; \
    while (NEW_SIZE < NEEDED_SIZE) \
      NEW_SIZE *= 2; \
    if (OLD_SIZE) { \
      size_t NEW_BYTES = NEW_SIZE * sizeof *(MAP).begin; \
      assert (OLD_BEGIN); \
      NEW_BEGIN = realloc (OLD_BEGIN, NEW_BYTES); \
      if (!NEW_BEGIN) \
        die ("out-of-memory resizing '" #MAP "' map"); \
      size_t OLD_BYTES = OLD_SIZE * sizeof *(MAP).begin; \
      size_t DELTA_BYTES = NEW_BYTES - OLD_BYTES; \
      memset ((char *)NEW_BEGIN + OLD_BYTES, 0, DELTA_BYTES); \
    } else { \
      assert (!OLD_BEGIN); \
      NEW_BEGIN = calloc (NEW_SIZE, sizeof *(MAP).begin); \
      if (!NEW_BEGIN) \
        die ("out-of-memory initializing '" #MAP "' map"); \
    } \
    (MAP).begin = NEW_BEGIN; \
    (MAP).end = (MAP).begin + NEW_SIZE; \
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

// Having a statically allocated read buffer allows to inline more character
// reading code into integer parsing routines and thus speed up overall
// parsing time substantially (saw 30% improvement).

#define size_buffer (1u << 20)

struct buffer {
  char chars[size_buffer];
  size_t pos, end;
} buffer;

static size_t fill_buffer () {
  if (!input.file)
    return buffer.pos = buffer.end = 0;
  buffer.pos = 0;
  buffer.end = fread (buffer.chars, 1, size_buffer, input.file);
  return buffer.end;
}

// These three functions were not inlined with gcc-11 but should be despite
// having declared them as 'inline'.

static inline int read_buffer (void) __attribute__ ((always_inline));
static inline void count_read (int ch) __attribute__ ((always_inline));
static inline int read_char (void) __attribute__ ((always_inline));

static inline int read_buffer (void) {
  if (buffer.pos == buffer.end && !fill_buffer ()) {
    input.eof = true;
    return EOF;
  }
  return buffer.chars[buffer.pos++];
}

static inline void count_read (int ch) {
  if (ch == '\n')
    input.lines++;
  if (ch != EOF) {
    input.bytes++;
    input.last = ch;
  }
}

static inline int read_char (void) {
  assert (input.file);
  assert (input.saved == EOF);
  int res = read_buffer ();
  if (res == '\r') {
    res = read_buffer ();
    if (res != '\n')
      prr ("carriage-return without following new-line");
  }
  count_read (res);
  return res;
}

// We only need a look-ahead for the very first byte to determined whether
// the first input file is a DIMACS file or not (if exactly two files are
// specified).  In both cases we save this very first character as 'saved'
// in the input file and then when coming back to parsing this file
// will give back this saved character as first read character.

// Note that statistics of the file are adjusted during reading the
// saved character the firs time do not need to be updated here again.

// Originally we simply only had one 'read_char' function, but factoring out
// this rare situation and restricting it to the beginning of parsing helped
// the compiler to produce better code for the hot-stop which merges the
// code of the inlined 'read_char' and 'isdigit'.

static int read_first_char (void) {
  assert (input.file);
  int res = input.saved;
  if (res == EOF)
    res = read_char ();
  else
    input.saved = EOF;
  return res;
}

static void flush_buffer () {
  assert (output.file);
  size_t bytes = buffer.pos;
  if (!bytes)
    return;
  size_t written = fwrite (buffer.chars, 1, bytes, output.file);
  if (written != bytes) {
    if (output.path)
      die ("flushing %zu bytes of write buffer to '%s' failed", bytes,
           output.path);
    else
      die ("flushing %zu bytes of write buffer failed", bytes);
  }
  buffer.pos = 0;
}

static inline void write_char (unsigned ch) {
  if (buffer.pos == size_buffer)
    flush_buffer ();
  buffer.chars[buffer.pos++] = ch;
  output.bytes++;
  if (ch == '\n')
    output.lines++;
}

static inline void write_space () { write_char (' '); }

static inline void write_str (const char *str) {
  for (const char *p = str; *p; p++)
    write_char (*p);
}

static inline void write_int (int i) __attribute__ ((always_inline));

static char int_buffer[16];

static inline void write_int (int i) {
  if (i) {
    char *p = int_buffer + sizeof int_buffer - 1;
    assert (!*p);
    assert (i != INT_MIN);
    unsigned tmp = abs (i);
    while (tmp) {
      *--p = '0' + (tmp % 10);
      tmp /= 10;
    }
    if (i < 0)
      *--p = '-';
    write_str (p);
  } else
    write_char ('0');
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

static inline void assign_literal (int lit) {
  assert (lit);
  assert (lit != INT_MIN);
  dbg ("assigning literal %d", lit);
  int idx = abs (lit);
  signed char value = lit < 0 ? -1 : 1;
  signed char *v = &ACCESS (variables.values, idx);
  assert (!*v);
  *v = value;
  PUSH (trail, lit);
  statistics.literals.assigned++;
}

static inline void unassign_literal (int lit) {
  assert (lit);
  assert (lit != INT_MIN);
  dbg ("unassigning literal %d", lit);
  int idx = abs (lit);
  signed char *v = &ACCESS (variables.values, idx);
#ifndef NDEBUG
  signed char value = lit < 0 ? -1 : 1;
  assert (*v == value);
#endif
  *v = 0;
}

static void backtrack () {
  for (int *t = trail.begin; t != trail.end; t++)
    unassign_literal (*t);
  CLEAR (trail);
}

static inline signed char assigned_literal (int) __attribute ((always_inline));

static inline signed char assigned_literal (int lit) {
  assert (lit);
  assert (lit != INT_MIN);
  int idx = abs (lit);
  int res = ACCESS (variables.values, idx);
  if (lit < 0)
    res = -res;
  return res;
}

static void crr (int, const char *, ...)
    __attribute__ ((format (printf, 2, 3)));

static void crr (int id, const char *fmt, ...) {
  fputs ("lrat-trim: ", stderr);
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fprintf (stderr, " while checking clause '%d'", id);
  if (track) {
    struct addition *addition = &ACCESS (clauses.added, id);
    fprintf (stderr, " at line '%zu' ", addition->line);
    assert (proof.input);
    assert (proof.input->path);
    fprintf (stderr, "in '%s'", proof.input->path);
  }
  fputs (": ", stderr);
  int * l = ACCESS (clauses.literals, id);
  while (*l)
    fprintf (stderr, "%d ", *l++);
  fputs ("0\n", stderr);
  exit (1);
}

static void check_clause (int id, int *literals, int *antecedents) {
  assert (EMPTY (trail));
  statistics.clauses.resolved++;
  statistics.clauses.checked.total++;
  if (!*literals)
    statistics.clauses.checked.empty++;
  for (int *l = literals, lit; (lit = *l); l++) {
    signed char value = assigned_literal (lit);
    if (value < 0) {
      dbg ("skipping duplicated literal '%d' in clause '%d'", lit, id);
      continue;
    }
    if (value > 0) {
      dbg ("skipping tautological literal '%d' and '%d' "
           "in clause '%d'",
           -lit, lit, id);
    CHECKED:
      backtrack ();
      return;
    }
    assign_literal (-lit);
  }
  for (int *a = antecedents, aid; (aid = *a); a++) {
    if (aid < 0)
      crr (id, "checking negative RAT antecedent '%d' not supported", aid);
    int *als = ACCESS (clauses.literals, aid);
    dbgs (als, "resolving antecedent %d clause", aid);
    statistics.clauses.resolved++;
    int unit = 0;
    for (int *l = als, lit; (lit = *l); l++) {
      signed char value = assigned_literal (lit);
      if (value < 0)
        continue;
      if (unit)
        crr (id, "antecedent '%d' does not produce unit", aid);
      unit = lit;
      if (!value)
        assign_literal (lit);
    }
    if (!unit) {
      dbgs (als,
            "conflicting antecedent '%d' thus checking "
            " of clause '%d' succeeded",
            aid, id);
      goto CHECKED;
    }
  }
  crr (id, "propagating antecedents does not yield conflict");
}

static inline bool is_original_clause (int id) {
  return !id || !first_clause_added_in_proof ||
         id < first_clause_added_in_proof;
}

// Apparently the hot-spot of the parser is checking the loop condition for
// integer parsing which reads the next character from an input file and
// then asks 'isdigit' whether the integer parsed at this point should be
// extended by another digit or the first character (space or new-line)
// after the integer has been reached.  It seems that the claimed fast
// 'isdigit' from 'libc', which we assume is implemented by a table look-up,
// prevents some local compiler optimization as soon the character reading
// code is also inlined (which even for 'getc_unlocked' happened).

// Using the good old range based checked (assuming an ASCII encoding) seems
// to help the compiler to produce better code (around 5% faster).

// We use 'ISDIGIT' instead of 'isdigit' as the later can itself be a macro.

#define ISDIGIT faster_than_default_isdigit

static inline bool faster_than_default_isdigit (int ch) {
  return '0' <= ch && ch <= '9';
}

// If the user does have huge integers (larger than 'INT_MAX') in proofs we
// still want to print those integers in the triggered error message.  This
// function takes the integer 'n' parsed so far and the digit 'ch'
// triggering the overflow as argument and then continues reading digits
// from the input file (for a while) and prints the complete parsed integer
// string to a statically allocated buffer which is returned.

static const char *exceeds_int_max (int n, int ch) {
  static char buffer[32];
  const size_t size = sizeof buffer - 5;
  assert (ISDIGIT (ch));
  sprintf (buffer, "%d", n);
  size_t i = strlen (buffer);
  do {
    assert (i < sizeof buffer);
    buffer[i++] = ch;
  } while (i < size && ISDIGIT (ch = read_char ()));
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

static void parse_cnf () {
  if (!cnf.input)
    return;
  double start = process_time ();
  vrb ("starting parsing CNF after %.2f seconds", start);
  input = *cnf.input;
  msg ("reading CNF from '%s'", input.path);
  int ch;
  for (ch = read_first_char (); ch != 'p'; ch = read_char ())
    if (ch != 'c')
      prr ("expected 'c' or 'p' as first character");
    else
      while ((ch = read_char ()) != '\n')
        if (ch == EOF)
          prr ("unexpected end-of-file in comment before header");
  if (read_char () != ' ')
    prr ("expected space after 'p'");
  if (read_char () != 'c' || read_char () != 'n' || read_char () != 'f')
    prr ("expected 'p cnf'");
  if (read_char () != ' ')
    prr ("expected space after 'p cnf'");
  ch = read_char ();
  if (!ISDIGIT (ch))
    prr ("expected digit after 'p cnf '");
  int header_variables = ch - '0';
  while (ISDIGIT (ch = read_char ())) {
    if (INT_MAX / 10 < header_variables)
    NUMBER_OF_VARIABLES_EXCEEDS_INT_MAX:
      prr ("number of variables '%s' exceeds 'INT_MAX'",
           exceeds_int_max (header_variables, ch));
    header_variables *= 10;
    int digit = ch - '0';
    if (INT_MAX - digit < header_variables) {
      header_variables /= 10;
      goto NUMBER_OF_VARIABLES_EXCEEDS_INT_MAX;
    }
    header_variables += digit;
  }
  if (ch != ' ')
    prr ("expected space after 'p cnf %d", header_variables);
  ch = read_char ();
  if (!ISDIGIT (ch))
    prr ("expected digit after 'p cnf %d '", header_variables);
  int header_clauses = ch - '0';
  while (ISDIGIT (ch = read_char ())) {
    if (INT_MAX / 10 < header_clauses)
    NUMBER_OF_CLAUSES_EXCEEDS_INT_MAX:
      prr ("number of clauses '%s' exceeds 'INT_MAX'",
           exceeds_int_max (header_clauses, ch));
    header_clauses *= 10;
    int digit = ch - '0';
    if (INT_MAX - digit < header_clauses) {
      header_clauses /= 10;
      goto NUMBER_OF_CLAUSES_EXCEEDS_INT_MAX;
    }
    header_clauses += digit;
  }
  if (ch != '\n')
    prr ("expected new-line after 'p cnf %d %d'", header_variables,
         header_clauses);
  msg ("found 'p cnf %d %d' header", header_variables, header_clauses);
  ADJUST (variables.values, header_variables);
  ADJUST (clauses.literals, header_clauses);
  ADJUST (clauses.status, header_clauses);
  int lit = 0, parsed_clauses = 0;
  struct int_stack parsed_literals;
  ZERO (parsed_literals);
  for (;;) {
    ch = read_char ();
    if (ch == ' ' || ch == '\t' || ch == '\n')
      continue;
    if (ch == EOF) {
      assert (input.eof);
      if (lit)
        prr ("'0' missing after clause before end-of-file");
      if (parsed_clauses < header_clauses) {
        if (parsed_clauses + 1 == header_clauses)
          prr ("clause missing");
        else
          prr ("%d clauses missing", header_clauses - parsed_clauses);
      }
      break;
    }
    if (ch == 'c') {
    SKIP_COMMENT_AFTER_HEADER:
      while ((ch = read_char ()) != '\n')
        if (ch == EOF)
          prr ("unexpected end-of-file in comment after header");
      continue;
    }
    int sign;
    if (ch == '-') {
      ch = read_char ();
      if (!ISDIGIT (ch))
        prr ("expected digit after '-'");
      if (ch == '0')
        prr ("expected non-zero digit after '-'");
      sign = -1;
    } else {
      if (!ISDIGIT (ch))
        prr ("unexpected character instead of literal");
      sign = 1;
    }
    int idx = ch - '0';
    while (ISDIGIT (ch = read_char ())) {
      if (!idx)
        prr ("unexpected digit '%c' after '0'", ch);
      if (INT_MAX / 10 < idx)
      VARIABLE_EXCEEDS_INT_MAX:
        prr ("variable '%s' exceeds 'INT_MAX'", exceeds_int_max (idx, ch));
      idx *= 10;
      int digit = ch - '0';
      if (INT_MAX - digit < idx) {
        idx /= 10;
        goto VARIABLE_EXCEEDS_INT_MAX;
      }
      idx += digit;
    }
    lit = sign * idx;
    if (idx > header_variables)
      prr ("literal '%d' exceeds maximum variable '%d'", lit,
           header_variables);
    if (ch != 'c' && ch != ' ' && ch != '\t' && ch != '\n')
      prr ("expected white space after '%d'", lit);
    if (parsed_clauses >= header_clauses)
      prr ("too many clauses");
    PUSH (parsed_literals, lit);
    if (!lit) {
      parsed_clauses++;
      statistics.original.cnf.added++;
      dbgs (parsed_literals.begin, "clause %d parsed", parsed_clauses);
      size_t size_literals = SIZE (parsed_literals);
      size_t bytes_literals = size_literals * sizeof (int);
      int *l = malloc (bytes_literals);
      if (!l) {
        assert (size_literals);
        die ("out-of-memory allocating literals of size %zu clause %d",
             size_literals - 1, parsed_clauses);
      }
      memcpy (l, parsed_literals.begin, bytes_literals);
      assert (parsed_clauses < SIZE (clauses.literals));
      clauses.literals.begin[parsed_clauses] = l;
      CLEAR (parsed_literals);
      assert (parsed_clauses < SIZE (clauses.status));
      clauses.status.begin[parsed_clauses] = 1;
    }
    if (ch == 'c')
      goto SKIP_COMMENT_AFTER_HEADER;
  }
  assert (parsed_clauses == header_clauses);
  assert (EMPTY (parsed_literals));
  RELEASE (parsed_literals);

  if (input.close)
    fclose (input.file);
  *cnf.input = input;

  vrb ("read %zu CNF lines with %zu bytes (%.0f MB)", input.lines,
       input.bytes, input.bytes / (double)(1 << 20));

  last_clause_added_in_cnf = parsed_clauses;
  msg ("parsed CNF with %zu added clauses", statistics.original.cnf.added);

  double end = process_time (), duration = end - start;
  vrb ("finished parsing CNF after %.2f seconds", end);
  msg ("parsing original CNF took %.2f seconds and needed %.0f MB memory",
       duration, mega_bytes ());
}

static void parse_proof () {
  double start = process_time ();
  vrb ("starting parsing proof after %.2f seconds", start);
  assert (proof.input);
  input = *proof.input;
  msg ("reading proof from '%s'", input.path);
  fill_buffer ();
  int last_id = 0;
  int ch = read_first_char ();
  struct int_stack parsed_literals;
  struct int_stack parsed_antecedents;
  ZERO (parsed_literals);
  ZERO (parsed_antecedents);
  size_t line = 0;
  bool first = true;
  while (ch != EOF) {
    if (!isdigit (ch)) {
      if (first && (ch == 'c' || ch == 'p'))
        prr ("unexpected '%c' as first character: "
             "did you use a CNF instead of a proof file?",
             ch);
      else
        prr ("expected digit as first character of line");
    }
    first = false;
    line = input.lines;
    int id = ch - '0';
    while (ISDIGIT (ch = read_char ())) {
      if (!id)
        prr ("unexpected digit '%c' after '0'", ch);
      if (INT_MAX / 10 < id)
      LINE_IDENTIFIER_EXCEEDS_INT_MAX:
        prr ("line identifier '%s' exceeds 'INT_MAX'",
             exceeds_int_max (id, ch));
      id *= 10;
      int digit = ch - '0';
      if (INT_MAX - digit < id) {
        id /= 10;
        goto LINE_IDENTIFIER_EXCEEDS_INT_MAX;
      }
      id += digit;
    }
    if (ch != ' ')
      prr ("expected space after identifier '%d'", id);
    if (id < last_id)
      prr ("identifier '%d' smaller than last '%d'", id, last_id);
    dbg ("parsed clause identifier %d at line %zu", id, line + 1);
    ADJUST (clauses.status, id);
    ch = read_char ();
    if (ch == 'd') {
      ch = read_char ();
      if (ch != ' ')
        prr ("expected space after '%d d' in deletion %d", id, id);
      assert (EMPTY (parsed_antecedents));
      int last = 0;
      do {
        ch = read_char ();
        if (!ISDIGIT (ch)) {
          if (last)
            prr ("expected digit after '%d ' in deletion %d", last, id);
          else
            prr ("expected digit after '%d d ' in deletion %d", id, id);
        }
        int other = ch - '0';
        while (ISDIGIT ((ch = read_char ()))) {
          if (!other)
            prr ("unexpected digit '%c' after '0' in deletion %d", ch, id);
          if (INT_MAX / 10 < other)
          DELETED_CLAUSE_IDENTIFIER_EXCEEDS_INT_MAX:
            prr ("deleted clause identifier '%s' exceeds 'INT_MAX' "
                 "in deletion %d",
                 exceeds_int_max (other, ch), id);
          other *= 10;
          int digit = ch - '0';
          if (INT_MAX - digit < other) {
            other /= 10;
            goto DELETED_CLAUSE_IDENTIFIER_EXCEEDS_INT_MAX;
          }
          other += digit;
        }
        if (other) {
          if (ch != ' ')
            prr ("expected space after '%d' in deletion %d", other, id);
          if (id && other > id)
            prr ("deleted clause '%d' "
                 "larger than deletion identifier '%d'",
                 other, id);
          if (!first_clause_added_in_proof)
            ADJUST (clauses.status, other);
          signed char *status_ptr = &ACCESS (clauses.status, other);
          signed char status = *status_ptr;
          *status_ptr = -1;
          if (!status && first_clause_added_in_proof) {
            assert (first_clause_added_in_proof <= other);
            prr ("deleted clause '%d' in deletion %d "
                 "is neither an original clause nor has been added",
                 other, id);
          }
          if (track) {
            ADJUST (clauses.deleted, other);
            struct deletion *other_deletion =
                &ACCESS (clauses.deleted, other);
            if (!status && first_clause_added_in_proof) {
              assert (first_clause_added_in_proof <= other);
              prr ("deleted clause '%d' in deletion %d "
                   "is neither an original clause nor has been added",
                   other, id);
            } else if (status < 0) {
              assert (other_deletion->id);
              assert (other_deletion->line);
              prr ("clause %d requested to be deleted in deletion %d "
                   "was already deleted in deletion %d at line %zu",
                   other, id, other_deletion->id, other_deletion->line);
            }
            *status_ptr = -1;
            size_t deleted_line = input.lines + 1;
            dbg ("marked clause %d to be deleted "
                 "at line %zu in deletion %d",
                 other, deleted_line, id);
            other_deletion->line = deleted_line;
            other_deletion->id = id;
          } else if (status < 0)
            prr ("clause %d requested to be deleted in deletion %d "
                 "was already deleted before "
                 "(run with '--track' for more information)",
                 other, id);
          if (is_original_clause (id))
            statistics.original.cnf.deleted++;
          else
            statistics.original.proof.deleted++;
          if (!trimming || (checking && forward)) {
            assert (!proof.output);
            assert (!cnf.output);
#if 0
            if (checking && forward)
#endif
              assert (EMPTY (clauses.antecedents));
#if 0
            else if (trimming) {
	      cov
              int **a = &ACCESS (clauses.antecedents, other);
              free (*a);
              *a = 0;
            }
#endif
            int **l = &ACCESS (clauses.literals, other);
            free (*l);
            *l = 0;
          }
        } else if (ch != '\n')
          prr ("expected new-line after '0' at end of deletion %d", id);
#if !defined(NDEBUG) || defined(LOGGING)
        PUSH (parsed_antecedents, other);
#endif
        last = other;
      } while (last);
#if !defined(NDEBUG) || defined(LOGGING)
      dbgs (parsed_antecedents.begin,
            "parsed deletion %d and deleted clauses", id);
      CLEAR (parsed_antecedents);
#endif
    } else {
      if (id == last_id)
        prr ("line identifier '%d' of addition line does not increase", id);
      if (!first_clause_added_in_proof) {
        if (last_clause_added_in_cnf) {
          if (last_clause_added_in_cnf == id)
            prr ("first added clause %d in proof "
                 "has same identifier as last original clause",
                 id);
          else if (last_clause_added_in_cnf > id)
            prr ("first added clause %d in proof "
                 "has smaller identifier as last original clause %d",
                 id, last_clause_added_in_cnf);
        }
        vrb ("adding first clause %d in proof", id);
        first_clause_added_in_proof = id;
        if (!last_clause_added_in_cnf) {
          signed char *begin = clauses.status.begin;
          signed char *end = begin + id;
          for (signed char *p = begin + 1; p != end; p++) {
            signed char status = *p;
            if (status)
              assert (status < 0);
            else
              *p = 1;
          }
          assert (!statistics.original.cnf.added);
          statistics.original.cnf.added = id - 1;
        }
      }
      assert (EMPTY (parsed_literals));
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
          if (!ISDIGIT (ch = read_char ()))
            prr ("expected digit after '%d -' in clause %d", last, id);
          if (ch == '0')
            prr ("expected non-zero digit after '%d -'", last);
          sign = -1;
        } else if (!ISDIGIT (ch))
          prr ("expected literal or '0' after '%d ' in clause %d", last,
               id);
        else
          sign = 1;
        int idx = ch - '0';
        while (ISDIGIT (ch = read_char ())) {
          if (!idx)
            prr ("unexpected second '%c' after '%d 0' in clause %d", ch,
                 last, id);
          if (INT_MAX / 10 < idx) {
          VARIABLE_INDEX_EXCEEDS_INT_MAX:
            if (sign < 0)
              prr ("variable index in literal '-%s' "
                   "exceeds 'INT_MAX' in clause %d",
                   exceeds_int_max (idx, ch), id);
            else
              prr ("variable index '%s' exceeds 'INT_MAX' in clause %d",
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
            prr ("expected space after literal '%d' in clause %d", lit, id);
          else
            prr ("expected space after literals and '0' in clause %d", id);
        }
        PUSH (parsed_literals, lit);
        last = lit;
      }
      dbgs (parsed_literals.begin, "clause %d literals", id);

      size_t size_literals = SIZE (parsed_literals);
      size_t bytes_literals = size_literals * sizeof (int);
      int *l = malloc (bytes_literals);
      if (!l) {
        assert (size_literals);
        die ("out-of-memory allocating literals of size %zu clause %d",
             size_literals - 1, id);
      }
      memcpy (l, parsed_literals.begin, bytes_literals);
      ADJUST (clauses.literals, id);
      ACCESS (clauses.literals, id) = l;
      if (size_literals == 1) {
        if (!empty_clause) {
          vrb ("found empty clause %d", id);
          empty_clause = id;
        }
      }

      CLEAR (parsed_literals);
      assert (EMPTY (parsed_antecedents));

      assert (!last);
      do {
        int sign;
        if ((ch = read_char ()) == '-') {
          if (!ISDIGIT (ch = read_char ()))
            prr ("expected digit after '%d -' in clause %d", last, id);
          if (ch == '0')
            prr ("expected non-zero digit after '%d -'", last);
          sign = -1;
        } else if (!ISDIGIT (ch))
          prr ("expected clause identifier after '%d ' "
               "in clause %d",
               last, id);
        else
          sign = 1;
        int other = ch - '0';
        while (ISDIGIT (ch = read_char ())) {
          if (!other)
            prr ("unexpected second '%c' after '%d 0' in clause %d", ch,
                 last, id);
          if (INT_MAX / 10 < other) {
          ANTECEDENT_IDENTIFIER_EXCEEDS_INT_MAX:
            if (sign < 0)
              prr ("antecedent '-%s' exceeds 'INT_MAX' in clause %d",
                   exceeds_int_max (other, ch), id);
            else
              prr ("antecedent '%s' exceeds 'INT_MAX' in clause %d",
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
            prr ("expected space after antecedent '%d' in clause %d",
                 signed_other, id);
          if (other >= id)
            prr ("antecedent '%d' in clause %d exceeds clause",
                 signed_other, id);
          signed char s = ACCESS (clauses.status, other);
          if (!s)
            prr ("antecedent '%d' in clause %d "
                 "is neither an original clause nor has been added",
                 signed_other, id);
          else if (s < 0) {
            if (track) {
              struct deletion *other_deletion =
                  &ACCESS (clauses.deleted, other);
              assert (other_deletion->id);
              assert (other_deletion->line);
              prr ("antecedent %d in clause %d "
                   "was already deleted in deletion %d at line %zu",
                   signed_other, id, other_deletion->id,
                   other_deletion->line);
            } else
              prr ("antecedent %d in clause %d was already deleted before"
                   "(run with '--track' for more information)",
                   other, id);
          }
        } else {
          if (ch != '\n')
            prr ("expected new-line after '0' at end of clause %d", id);
        }
        PUSH (parsed_antecedents, signed_other);
        last = signed_other;
      } while (last);
      dbgs (parsed_antecedents.begin, "clause %d antecedents", id);
      size_t size_antecedents = SIZE (parsed_antecedents);
      assert (size_antecedents > 0);
      if (track) {
        ADJUST (clauses.added, id);
        struct addition *addition = &ACCESS (clauses.added, id);
        addition->line = line;
      }
      statistics.original.proof.added++;
      if (checking && forward) {
        check_clause (id, l, parsed_antecedents.begin);
        dbg ("forward checked clause %d", id);
      } else if (trimming) {
        size_t bytes_antecedents = size_antecedents * sizeof (int);
        int *a = malloc (bytes_antecedents);
        if (!a) {
          assert (size_antecedents);
          die ("out-of-memory allocating antecedents of size %zu clause %d",
               size_antecedents - 1, id);
        }
        memcpy (a, parsed_antecedents.begin, bytes_antecedents);
        ADJUST (clauses.antecedents, id);
        ACCESS (clauses.antecedents, id) = a;
      }
      CLEAR (parsed_antecedents);
      ACCESS (clauses.status, id) = 1;
    }
    last_id = id;
    ch = read_char ();
  }
  RELEASE (parsed_antecedents);
  RELEASE (parsed_literals);
  if (input.close)
    fclose (input.file);
  *proof.input = input;

  RELEASE (clauses.deleted);
  RELEASE (clauses.added);
  RELEASE (clauses.status);

  if (!empty_clause)
    wrn ("no empty clause added in input proof");

  vrb ("read %zu proof lines with %zu bytes (%.0f MB)", input.lines,
       input.bytes, input.bytes / (double)(1 << 20));

  msg ("parsed original proof with %zu added and %zu deleted clauses",
       statistics.original.proof.added, statistics.original.proof.deleted);

  double end = process_time (), duration = end - start;
  vrb ("finished parsing proof after %.2f seconds", end);
  msg ("parsing original proof took %.2f seconds and needed %.0f MB memory",
       duration, mega_bytes ());
}

static inline bool mark_used (int id, int used_where) {
  assert (0 < id);
  assert (0 < used_where);
  int *w = &ACCESS (clauses.used, id);
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

static void trim_proof () {

  if (!trimming)
    return;

  double start = process_time ();
  vrb ("starting trimming after %.2f seconds", start);

  ADJUST (clauses.used, empty_clause);

  static struct int_stack work;
  ZERO (work);

  if (empty_clause) {
    assert (EMPTY (work));
    mark_used (empty_clause, empty_clause);
    if (!is_original_clause (empty_clause))
      PUSH (work, empty_clause);

    while (!EMPTY (work)) {
      unsigned id = POP (work);
      assert (ACCESS (clauses.used, id));
      int *a = ACCESS (clauses.antecedents, id);
      assert (a);
      for (int *p = a, other; (other = abs (*p)); p++)
        if (!mark_used (other, id) && !is_original_clause (other))
          PUSH (work, other);
    }
  }

  msg ("trimmed %zu original clauses in CNF to %zu clauses %.0f%%",
       statistics.original.cnf.added, statistics.trimmed.cnf.added,
       percent (statistics.trimmed.cnf.added,
                statistics.original.cnf.added));

  msg ("trimmed %zu added clauses in original proof to %zu clauses %.0f%%",
       statistics.original.proof.added, statistics.trimmed.proof.added,
       percent (statistics.trimmed.proof.added,
                statistics.original.proof.added));

  RELEASE (work);

  double end = process_time (), duration = end - start;
  vrb ("finished trimming after %.2f seconds", end);
  msg ("trimming proof took %.2f seconds", duration);
}

static void check_proof () {

  if (!checking || forward || !empty_clause)
    return;

  double start = process_time ();
  vrb ("starting backward checking after %.2f seconds", start);

  int id = first_clause_added_in_proof;
  for (;;) {
    int where = ACCESS (clauses.used, id);
    if (where) {
      int * l = ACCESS (clauses.literals, id);
      int * a = ACCESS (clauses.antecedents, id);
      dbgs (l, "checking clause %d literals", id);
      dbgs (a, "checking clause %d antecedents", id);
      check_clause (id, l, a);
    }
    if (id++ == empty_clause)
      break;
  }

  double end = process_time (), duration = end - start;
  vrb ("finished backward checking after %.2f seconds", end);
  msg ("backward checking proof took %.2f seconds", duration);
}

static struct file *write_file (struct file *file) {
  assert (file->path);
  if (!strcmp (file->path, "/dev/null")) {
    assert (!file->file);
    assert (!file->close);
  } else if (!strcmp (file->path, "-")) {
    file->file = stdout;
    file->path = "<stdout>";
    assert (!file->close);
  } else if (!(file->file = fopen (file->path, "w")))
    die ("can not write '%s'", file->path);
  else
    file->close = 1;
  return file;
}

static void close_output_proof () {
  assert (proof.output);
  flush_buffer ();
  if (output.close)
    fclose (output.file);
  *proof.output = output;
  vrb ("wrote %zu lines with %zu bytes (%.0f MB)", output.lines,
       output.bytes, output.bytes / (double)(1 << 20));
  msg ("trimmed %zu bytes (%.0f MB) to %zu bytes (%.0f MB) %.0f%%",
       proof.input->bytes, proof.input->bytes / (double)(1 << 20),
       proof.output->bytes, proof.output->bytes / (double)(1 << 20),
       percent (proof.output->bytes, proof.input->bytes));
}

static int map_id (int id) {
  assert (id != INT_MIN);
  int abs_id = abs (id);
  int res;
  if (id < first_clause_added_in_proof)
    res = id;
  else
    res = ACCESS (clauses.map, abs_id);
  if (id < 0)
    res = -res;
  return res;
}

static void write_non_empty_proof () {

  assert (output.path);
  assert (output.file);

  assert (empty_clause > 0);
  ADJUST (clauses.links, empty_clause);
  ADJUST (clauses.heads, empty_clause);

  for (int id = 1; id != first_clause_added_in_proof; id++) {
    int where = ACCESS (clauses.used, id);
    if (where) {
      assert (id < where);
      assert (!is_original_clause (where));
      ACCESS (clauses.links, id) = ACCESS (clauses.heads, where);
      ACCESS (clauses.heads, where) = id;
    } else {
      if (!statistics.trimmed.cnf.deleted) {
        write_int (first_clause_added_in_proof - 1);
        write_str (" d");
      }
      write_space ();
      write_int (id);
      statistics.trimmed.cnf.deleted++;
      statistics.trimmed.cnf.added++;
    }
  }

  if (statistics.trimmed.cnf.deleted) {
    write_str (" 0\n");

    vrb ("deleting %zu original CNF clauses initially",
         statistics.trimmed.cnf.deleted);
  }

  ADJUST (clauses.map, empty_clause);

  int id = first_clause_added_in_proof;
  int mapped = id;

  for (;;) {
    int where = ACCESS (clauses.used, id);
    if (where) {
      if (id != empty_clause) {
        assert (id < where);
        ACCESS (clauses.links, id) = ACCESS (clauses.heads, where);
        ACCESS (clauses.heads, where) = id;
        ACCESS (clauses.map, id) = mapped;
      }
      write_int (mapped);
      int *l = ACCESS (clauses.literals, id);
      assert (l);
      for (const int *p = l; *p; p++)
        write_space (), write_int (*p);
      write_str (" 0");
      int *a = ACCESS (clauses.antecedents, id);
      assert (a);
      for (const int *p = a; *p; p++) {
        write_space ();
        int other = *p;
        assert (abs (other) < id);
        write_int (map_id (other));
      }
      write_str (" 0\n");
      int head = ACCESS (clauses.heads, id);
      if (head) {
        write_int (mapped);
        write_str (" d");
        for (int link = head, next; link; link = next) {
          if (is_original_clause (link))
            statistics.trimmed.cnf.deleted++;
          else
            statistics.trimmed.proof.deleted++;
          write_space ();
          write_int (map_id (link));
          next = ACCESS (clauses.links, link);
        }
        write_str (" 0\n");
      }
      mapped++;
    }
    if (id++ == empty_clause)
      break;
  }
}

static void write_empty_proof () {
  msg ("writing empty proof without empty clause in input proof");
}

static void write_proof () {
  if (!proof.output)
    return;
  double start = process_time ();
  vrb ("starting writing proof after %.2f seconds", start);
  buffer.pos = 0;
  output = *write_file (proof.output);
  msg ("writing proof to '%s'", output.path);
  if (empty_clause)
    write_non_empty_proof ();
  else
    write_empty_proof ();
  close_output_proof ();
  double end = process_time (), duration = end - start;
  vrb ("finished writing proof after %.2f seconds", end);
  msg ("writing proof took %.2f seconds", duration);
}

static void write_cnf () {
  if (!cnf.output)
    return;
  output = *cnf.output;
  wrn ("writing the clausal core as CNF is not implemented yet");
  wrn ("(only trimming and writing the input proof)");
  if (output.close)
    fclose (output.file);
  *cnf.output = output;
}

static void release () {
#ifndef NDEBUG
  RELEASE (clauses.heads);
  RELEASE (clauses.links);
  RELEASE (clauses.map);
  RELEASE (clauses.used);
  RELEASE (variables.values);
  RELEASE (trail);
  release_ints_map (&clauses.literals);
  release_ints_map (&clauses.antecedents);
#endif
}

static const char *numeral (size_t i) {
  if (i == 0)
    return "1st";
  if (i == 1)
    return "2nd";
  assert (i == 2);
  return "3rd";
}

static void options (int argc, char **argv) {
  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp (arg, "-h") || !strcmp (arg, "--help")) {
      fputs (usage, stdout);
      exit (0);
    }
    if (!strcmp (arg, "-f") || !strcmp (arg, "--force"))
      force = arg;
    else if (!strcmp (arg, "-S") || !strcmp (arg, "--forward"))
      forward = arg;
    else if (!strcmp (arg, "-l") || !strcmp (arg, "--log"))
#ifdef LOGGING
      verbosity = INT_MAX;
#else
      die ("invalid option '-l' (build without logging support)");
#endif
    else if (!strcmp (arg, "-q") || !strcmp (arg, "--quiet"))
      verbosity = -1;
    else if (!strcmp (arg, "-t") || !strcmp (arg, "--track"))
      track = arg;
    else if (!strcmp (arg, "-v") || !strcmp (arg, "--verbose")) {
      if (verbosity <= 0)
        verbosity = 1;
    } else if (!strcmp (arg, "--no-check"))
      nocheck = arg;
    else if (!strcmp (arg, "--no-trim"))
      notrim = arg;
    else if (!strcmp (arg, "-V") || !strcmp (arg, "--version"))
      fputs (version, stdout), fputc ('\n', stdout), exit (0);
    else if (arg[0] == '-' && arg[1])
      die ("invalid option '%s' (try '-h')", arg);
    else if (size_files == 4)
      die ("too many files '%s', '%s', '%s' and '%s' (try '-h')",
           files[0].path, files[1].path, files[2].path, arg);
    else
      files[size_files++].path = arg;
  }

  if (!size_files)
    die ("no input file given (try '-h')");

  if (size_files > 2 && notrim)
    die ("can not write to '%s' with '%s", files[2].path, notrim);

  for (size_t i = 0; i + 1 != size_files; i++)
    if (strcmp (files[i].path, "-") && strcmp (files[i].path, "/dev/null"))
      for (size_t j = i + 1; j != size_files; j++)
        if (!strcmp (files[i].path, files[j].path))
          die ("identical %s and %s file '%s'", numeral (i), numeral (j),
               files[i].path);

  if (size_files > 2 && !strcmp (files[0].path, "-") &&
      !strcmp (files[1].path, "-"))
    die ("can not use '<stdin>' for both first two input files");

  if (size_files == 4 && !strcmp (files[2].path, "-") &&
      !strcmp (files[3].path, "-"))
    die ("can not use '<stdout>' for both last two output files");
}

static struct file *read_file (struct file *file) {
  assert (file->path);
  if (!strcmp (file->path, "/dev/null")) {
    assert (!file->file);
    assert (!file->close);
  } else if (!strcmp (file->path, "-")) {
    file->file = stdin;
    file->path = "<stdin>";
    assert (!file->close);
  } else if (!(file->file = fopen (file->path, "r")))
    die ("can not read '%s'", file->path);
  else
    file->close = 1;
  file->saved = EOF;
  return file;
}

static bool has_suffix (const char *str, const char *suffix) {
  size_t l = strlen (str), k = strlen (suffix);
  return l >= k && !strcasecmp (str + l - k, suffix);
}

static bool looks_like_a_dimacs_file (const char *path) {
  assert (path);
  if (!strcmp (path, "-"))
    return false;
  if (has_suffix (path, ".cnf"))
    return true;
  if (has_suffix (path, ".cnf.gz"))
    return true;
  if (has_suffix (path, ".cnf.bz2"))
    return true;
  if (has_suffix (path, ".cnf.xz"))
    return true;
  if (has_suffix (path, ".dimacs"))
    return true;
  if (has_suffix (path, ".dimacs.gz"))
    return true;
  if (has_suffix (path, ".dimacs.bz2"))
    return true;
  if (has_suffix (path, ".dimacs.xz"))
    return true;
  FILE *file = fopen (path, "r");
  if (!file)
    return false;
  int ch = getc (file);
  fclose (file);
  return ch == 'c' || ch == 'p';
}

static void open_input_files () {
  assert (size_files);
  if (size_files == 1)
    proof.input = read_file (&files[0]);
  else if (size_files == 2) {
    struct file *file = &files[0];
    input = *read_file (file);
    int ch = getc (input.file);
    count_read (ch);
    input.saved = ch;
    *file = input;
    if (ch == 'c' || ch == 'p') {
      cnf.input = file;
      proof.input = read_file (&files[1]);
      if (force)
        wrn ("using '%s' with CNF as first file '%s' does not make sense",
             force, files[0].path);
    } else {
      proof.input = file;
      if (notrim)
        die ("can not write to '%s' with '%s'", files[1].path, notrim);
      if (looks_like_a_dimacs_file (files[1].path)) {
        if (force)
          wrn ("forced to overwrite second file '%s' with trimmed proof "
               "even though it looks like a CNF in DIMACS format",
               files[1].path);
        else
          die ("will not overwrite second file '%s' with trimmed proof "
               "as it looks like a CNF in DIMACS format (use '--force' to "
               "overwrite nevertheless)",
               files[1].path);
      } else
        wrn ("using '%s' while second file '%s' does not look a CNF "
             "does not make sense",
             force, files[1].path);
      proof.output = &files[1];
    }
  } else {
    assert (size_files < 4);
    cnf.input = read_file (&files[0]);
    proof.input = read_file (&files[1]);
    proof.output = &files[2];
    if (size_files == 4)
      cnf.output = &files[3];
  }
  if (force && size_files != 2)
    wrn ("using '%s' without two files does not make sense", force);
  if (!cnf.input && nocheck)
    wrn ("using '%s' without CNF does not make sense", nocheck);
  if (!cnf.input && forward)
    wrn ("using '%s' without CNF does not make sense", forward);

  checking = !nocheck && cnf.input;
  trimming = !notrim && (!forward || proof.output || cnf.output);
}

static void print_banner () {
  if (verbosity < 0)
    return;
  printf ("c LRAT-TRIM Version %s trims LRAT proofs\n"
          "c Copyright (c) 2023 Armin Biere University of Freiburg\n",
          version);
  fflush (stdout);
}

static void print_mode () {
  if (verbosity < 0)
    return;

  const char *mode;
  if (cnf.input) {
    if (proof.output) {
      if (cnf.output)
        mode = "reading CNF and LRAT files and writing them too";
      else
        mode = "reading CNF and LRAT files and writing LRAT file";
    } else if (cnf.output)
      mode = "reading CNF and LRAT files and writing CNF file";
    else
      mode = "reading CNF and LRAT files";
  } else {
    if (proof.output)
      mode = "reading and writing LRAT files";
    else
      mode = "only reading LRAT file";
  }
  printf ("c %s\n", mode);

  if (checking) {
    if (forward) {
      if (trimming)
        mode = "forward checking all clauses followed by trimming proof";
      else
        mode = "forward checking all clauses without trimming proof";
    } else {
      if (trimming)
        mode = "backward checking trimmed clauses after trimming proof";
      else
        mode = "backward checking all clauses without trimming proof";
    }
  } else {
    if (trimming)
      mode = "trimming proof without checking clauses";
    else
      mode = "neither trimming proof not checking clauses";
  }
  printf ("c %s\n", mode);

  fflush (stdout);
}

static void print_statistics () {
  double t = process_time ();
  if (checking) {
    msg ("checked %zu clauses %.0f per second",
         statistics.clauses.checked.total,
         average (statistics.clauses.checked.total, t));
    msg ("resolved %zu clauses %.2f per checked clause",
         statistics.clauses.resolved,
         average (statistics.clauses.resolved,
                  statistics.clauses.checked.total));
    msg ("assigned %zu literals %.2f per checked clause",
         statistics.literals.assigned,
         average (statistics.literals.assigned,
                  statistics.clauses.checked.total));
  }
  msg ("maximum memory usage of %.0f MB", mega_bytes ());
  msg ("total time of %.2f seconds", t);
}

int main (int argc, char **argv) {
  options (argc, argv);
  open_input_files ();
  print_banner ();
  print_mode ();
  parse_cnf ();
  parse_proof ();
  trim_proof ();
  check_proof ();
  write_proof ();
  write_cnf ();
  int res = 0;
  if (statistics.clauses.checked.empty) {
    printf ("s VERIFIED\n");
    fflush (stdout);
    res = 20;
  } else
    msg ("no empty clause found and checked");
  release ();
  print_statistics ();
  return res;
}

static const char *version = "0.0.2";

// clang-format off

static const char * usage =

"usage: lrat-trim [ <option> ... ] <file> ...\n"
"\n"
"where '<option> ...' is a potentially empty list of the following options\n"
"\n"
"  -f | --force            force overwriting second file (looking like a CNF)\n"
"  -h | --help             print this command line option summary\n"
#ifdef LOGGING
"  -l | --log[ging]        print all messages including logging messages\n"
#endif
"  -n | --no-trim[ming]    disable trimming as described below\n"
"  -q | --quiet            be quiet and do not print any messages\n" 
"  -t | --track[-deleted]  track more deletion information\n"
"  -v | --verbose          enable verbose messages\n"
"  -V | --version          print version only\n"
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
"and parsed before reading the LRAT proof.  Providing a CNF allows to check\n"
"and not only trim a proof.  If checking fails an error message is produced\n"
"and the program aborts with a non-zero exit code.  If checking succeeds\n"
"the exit code is zero. If further an empty clause was found 's VERIFIED'\n"
"is printed.\n"
"\n"
"The status of clauses, i.e., whether they are added or have been deleted\n"
"is always tracked and checked precisely.  It is considered and error if\n"
"a clause is used in a proof line which been deleted before.  In order to\n"
"determine in which proof line exactly the offending clause was deleted\n"
"the user can specify '--track-deleted' to track this information which\n"
"will then yield a more informative error message.\n"
"\n"
"If the CNF or the proof contains an empty clause, then proof checking\n"
"is restricted to the trimmed proof.  Without empty clause, neither in\n"
"the CNF nor in the proof, trimming is skipped.  The same effect can be\n"
"achieved by using '--no-trimming', which has the additional benefit to\n"
"enforce forward on-the-fly checking while parsing the proof. This mode\n"
"allows to delete clauses eagerly and gives the chance to reduce memory\n"
"usage substantially.  Without trimming no output files are written.\n"
"\n"
"At most one of the input path names can be '-' which leads to reading the\n"
"corresponding input from '<stdin>'.  Similarly using '-' for one of the\n"
"outputs writes to '<stdout>'.  When exactly two files are given the first\n"
"file is opened and read first and its format (LRAT or DIMACS) is determined\n"
"by checking the first read character ('p' or 'c' gives DIMACS format).\n"
"This then also determines the type of the second file as proof output or\n"
"input.  Two files can not have the same specified file path except for '-'\n"
"and '/dev/null'.  The latter is a hard-coded name and will not actually be\n"
"opened nor written to '/dev/null' (whether it exists or not on the system).\n"
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

static const char *no_trimming;
static bool track_deleted;
static int verbosity;
static bool force;

static int *map;
static int *links;
static int *heads;
static int *used;

static int empty_clause;
static int last_clause_added_in_cnf;
static int first_clause_added_in_proof;

static struct int_stack work;
static struct char_map status;
static struct char_map marked;
static struct ints_map literals;
static struct ints_map antecedents;
static struct deletion_map deleted;
#if 0
static struct int_stack resolved;
#endif

static void die (const char *, ...) __attribute__ ((format (printf, 1, 2)));
static void err (const char *, ...) __attribute__ ((format (printf, 1, 2)));
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

static void err (const char *fmt, ...) {
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
      err ("carriage-return without following new-line");
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
  if (written != buffer.pos) {
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

static inline void mark_literal (int lit) {
  assert (lit);
  assert (lit != INT_MIN);
  int idx = abs (lit);
  signed char mark = lit < 0 ? -1 : 1;
  assert (idx < SIZE (marked));
  assert (!marked.begin[idx]);
  marked.begin[idx] = mark;
}

static inline void unmark_literal (int lit) {
  assert (lit);
  assert (lit != INT_MIN);
  int idx = abs (lit);
#ifndef NDEBUG
  signed char mark = lit < 0 ? -1 : 1;
  assert (idx < SIZE (marked));
  assert (marked.begin[idx] == mark);
#endif
  marked.begin[idx] = 0;
}

static inline signed char marked_literal (int lit) {
  assert (lit);
  assert (lit != INT_MIN);
  int idx = abs (lit);
  assert (idx < SIZE (marked));
  int res = marked.begin[idx];
  if (lit < 0)
    res = -res;
  return res;
}

#if 0
static void resolve_antecedents (int id, int *a) {
  assert (EMPTY (resolved.empty));
  for (int * p = a; *p; p++)
    ;
}
#endif

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
  int ch;
  for (ch = read_first_char (); ch != 'p'; ch = read_char ())
    if (ch != 'c')
      err ("expected 'c' or 'p' as first character");
    else
      while ((ch = read_char ()) != '\n')
        if (ch == EOF)
          err ("unexpected end-of-file in comment before header");
  if (read_char () != ' ')
    err ("expected space after 'p'");
  if (read_char () != 'c' || read_char () != 'n' || read_char () != 'f')
    err ("expected 'p cnf'");
  if (read_char () != ' ')
    err ("expected space after 'p cnf'");
  ch = read_char ();
  if (!ISDIGIT (ch))
    err ("expected digit after 'p cnf '");
  int variables = ch - '0';
  while (ISDIGIT (ch = read_char ())) {
    if (INT_MAX / 10 < variables)
    NUMBER_OF_VARIABLES_EXCEEDS_INT_MAX:
      err ("number of variables '%s' exceeds 'INT_MAX'",
           exceeds_int_max (variables, ch));
    variables *= 10;
    int digit = ch - '0';
    if (INT_MAX - digit < variables) {
      variables /= 10;
      goto NUMBER_OF_VARIABLES_EXCEEDS_INT_MAX;
    }
    variables += digit;
  }
  if (ch != ' ')
    err ("expected space after 'p cnf %d", variables);
  ch = read_char ();
  if (!ISDIGIT (ch))
    err ("expected digit after 'p cnf %d '", variables);
  int clauses = ch - '0';
  while (ISDIGIT (ch = read_char ())) {
    if (INT_MAX / 10 < clauses)
    NUMBER_OF_CLAUSES_EXCEEDS_INT_MAX:
      err ("number of clauses '%s' exceeds 'INT_MAX'",
           exceeds_int_max (clauses, ch));
    clauses *= 10;
    int digit = ch - '0';
    if (INT_MAX - digit < clauses) {
      clauses /= 10;
      goto NUMBER_OF_CLAUSES_EXCEEDS_INT_MAX;
    }
    clauses += digit;
  }
  if (ch != '\n')
    err ("expected new-line after 'p cnf %d %d'", variables, clauses);
  msg ("found 'p cnf %d %d' header", variables, clauses);
  ADJUST (marked, variables);
  ADJUST (literals, clauses);
  ADJUST (status, clauses);
  int lit = 0, parsed = 0;
  assert (EMPTY (work));
  for (;;) {
    ch = read_char ();
    if (ch == ' ' || ch == '\t' || ch == '\n')
      continue;
    if (ch == EOF) {
      assert (input.eof);
      if (lit)
        err ("'0' missing after clause before end-of-file");
      if (parsed < clauses) {
        if (parsed + 1 == clauses)
          err ("clause missing");
        else
          err ("%d clauses missing", clauses - parsed);
      }
      break;
    }
    if (ch == 'c') {
    SKIP_COMMENT_AFTER_HEADER:
      while ((ch = read_char ()) != '\n')
        if (ch == EOF)
          err ("unexpected end-of-file in comment after header");
      continue;
    }
    int sign;
    if (ch == '-') {
      ch = read_char ();
      if (!ISDIGIT (ch))
        err ("expected digit after '-'");
      if (ch == '0')
        err ("expected non-zero digit after '-'");
      sign = -1;
    } else {
      if (!ISDIGIT (ch))
        err ("unexpected character instead of literal");
      sign = 1;
    }
    int idx = ch - '0';
    while (ISDIGIT (ch = read_char ())) {
      if (!idx)
        err ("unexpected digit '%c' after '0'", ch);
      if (INT_MAX / 10 < idx)
      VARIABLE_EXCEEDS_INT_MAX:
        err ("variable '%s' exceeds 'INT_MAX'", exceeds_int_max (idx, ch));
      idx *= 10;
      int digit = ch - '0';
      if (INT_MAX - digit < idx) {
        idx /= 10;
        goto VARIABLE_EXCEEDS_INT_MAX;
      }
      idx += digit;
    }
    lit = sign * idx;
    if (idx > variables)
      err ("literal '%d' exceeds maximum variable '%d'", lit, variables);
    if (ch != 'c' && ch != ' ' && ch != '\t' && ch != '\n')
      err ("expected white space after '%d'", lit);
    if (parsed >= clauses)
      err ("too many clauses");
    PUSH (work, lit);
    if (!lit) {
      parsed++;
      statistics.original.cnf.added++;
      dbgs (work.begin, "clause %d parsed", parsed);
      size_t size_literals = SIZE (work);
      size_t bytes_literals = size_literals * sizeof (int);
      int *l = malloc (bytes_literals);
      if (!l) {
        assert (size_literals);
        die ("out-of-memory allocating literals of size %zu clause %d",
             size_literals - 1, parsed);
      }
      memcpy (l, work.begin, bytes_literals);
      assert (parsed < SIZE (literals));
      literals.begin[parsed] = l;
      CLEAR (work);
      assert (parsed < SIZE (status));
      status.begin[parsed] = 1;
    }
    if (ch == 'c')
      goto SKIP_COMMENT_AFTER_HEADER;
  }
  assert (EMPTY (work));
  assert (parsed == clauses);

  if (input.close)
    fclose (input.file);
  *cnf.input = input;

  vrb ("read %zu CNF lines with %zu bytes (%.0f MB)", input.lines,
       input.bytes, input.bytes / (double)(1 << 20));

  last_clause_added_in_cnf = parsed;
  msg ("parsed CNF proof with %zu added clauses",
       statistics.original.cnf.added);

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
  while (ch != EOF) {
    if (!isdigit (ch))
      err ("expected digit as first character of line");
    int id = (ch - '0');
    while (ISDIGIT (ch = read_char ())) {
      if (!id)
        err ("unexpected digit '%c' after '0'", ch);
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
    ADJUST (status, id);
    ch = read_char ();
    if (ch == 'd') {
      ch = read_char ();
      if (ch != ' ')
        err ("expected space after '%d d' in deletion %d", id, id);
      assert (EMPTY (work));
      int last = 0;
      do {
        ch = read_char ();
        if (!ISDIGIT (ch)) {
          if (last)
            err ("expected digit after '%d ' in deletion %d", last, id);
          else
            err ("expected digit after '%d d ' in deletion %d", id, id);
        }
        int other = ch - '0';
        while (ISDIGIT ((ch = read_char ()))) {
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
          if (id && other > id)
            err ("deleted clause '%d' "
                 "larger than deletion identifier '%d'",
                 other, id);
          if (first_clause_added_in_proof)
            assert (other < SIZE (status));
          else
            ADJUST (status, other);
          signed char *status_ptr = status.begin + other;
          signed char status = *status_ptr;
          *status_ptr = -1;
          if (!status && first_clause_added_in_proof) {
            assert (first_clause_added_in_proof <= other);
            err ("deleted clause '%d' in deletion %d "
                 "is neither an original clause nor has been added",
                 other, id);
          }
          if (track_deleted) {
            ADJUST (deleted, other);
            struct deletion *other_deletion = deleted.begin + other;
            if (!status && first_clause_added_in_proof) {
              assert (first_clause_added_in_proof <= other);
              err ("deleted clause '%d' in deletion %d "
                   "is neither an original clause nor has been added",
                   other, id);
            } else if (status < 0) {
              assert (other_deletion->id);
              assert (other_deletion->line);
              err ("clause %d requested to be deleted in deletion %d "
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
            err ("clause %d requested to be deleted in deletion %d "
                 "was already deleted before "
                 "(run with '--track-deleted' for more information)",
                 other, id);
          if (is_original_clause (id))
            statistics.original.cnf.deleted++;
          else
            statistics.original.proof.deleted++;
          if (no_trimming) {
            assert (!proof.output);
            assert (!cnf.output);
            assert (other < SIZE (antecedents));
            free (antecedents.begin[other]);
            antecedents.begin[other] = 0;
            assert (other < SIZE (literals));
            free (literals.begin[other]);
            literals.begin[other] = 0;
          }
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
        if (last_clause_added_in_cnf) {
          if (last_clause_added_in_cnf == id)
            err ("first added clause %d in proof "
                 "has same identifier as last original clause",
                 id);
          else if (last_clause_added_in_cnf > id)
            err ("first added clause %d in proof "
                 "has smaller identifier as last original clause %d",
                 id, last_clause_added_in_cnf);
        }
        vrb ("adding first clause %d in proof", id);
        first_clause_added_in_proof = id;
        if (!last_clause_added_in_cnf) {
          signed char *begin = status.begin;
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
          if (!ISDIGIT (ch = read_char ()))
            err ("expected digit after '%d -' in clause %d", last, id);
          if (ch == '0')
            err ("expected non-zero digit after '%d -'", last);
          sign = -1;
        } else if (!ISDIGIT (ch))
          err ("expected literal or '0' after '%d ' in clause %d", last,
               id);
        else
          sign = 1;
        int idx = ch - '0';
        while (ISDIGIT (ch = read_char ())) {
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

      size_t size_literals = SIZE (work);
      size_t bytes_literals = size_literals * sizeof (int);
      int *l = malloc (bytes_literals);
      if (!l) {
        assert (size_literals);
        die ("out-of-memory allocating literals of size %zu clause %d",
             size_literals - 1, id);
      }
      memcpy (l, work.begin, bytes_literals);
      ADJUST (literals, id);
      literals.begin[id] = l;
      if (size_literals == 1) {
        if (!empty_clause) {
          vrb ("found empty clause %d", id);
          empty_clause = id;
        }
      }

      CLEAR (work);
      assert (!last);
      do {
        int sign;
        if ((ch = read_char ()) == '-') {
          if (!ISDIGIT (ch = read_char ()))
            err ("expected digit after '%d -' in clause %d", last, id);
          if (ch == '0')
            err ("expected non-zero digit after '%d -'", last);
          sign = -1;
        } else if (!ISDIGIT (ch))
          err ("expected clause identifier after '%d ' "
               "in clause %d",
               last, id);
        else
          sign = 1;
        int other = ch - '0';
        while (ISDIGIT (ch = read_char ())) {
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
          assert (other < SIZE (status));
          signed char s = status.begin[other];
          if (!s)
            err ("antecedent '%d' in clause %d "
                 "is neither an original clause nor has been added",
                 signed_other, id);
          else if (s < 0) {
            assert (other < SIZE (deleted));
            if (track_deleted) {
              struct deletion *other_deletion = deleted.begin + other;
              assert (other_deletion->id);
              assert (other_deletion->line);
              err ("antecedent %d in clause %d "
                   "was already deleted in deletion %d at line %zu",
                   signed_other, id, other_deletion->id,
                   other_deletion->line);
            } else
              err ("antecedent %d in clause %d was already deleted before"
                   "(run with '--track-deleted' for more information)",
                   other, id);
          }
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
	if (!size_antecedents)
	  err ("empty list of antecedents of clause %d", id);
	if (size_antecedents == 1)
	  err ("single antecedent of clause %d", id);
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
        ADJUST (antecedents, id);
        antecedents.begin[id] = a;
      }
      status.begin[id] = 1;
      statistics.original.proof.added++;
    }
    last_id = id;
    ch = read_char ();
  }
  if (input.close)
    fclose (input.file);
  *proof.input = input;

  free (status.begin);
  free (marked.begin);
  free (deleted.begin);

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

static void trim_proof () {

  if (no_trimming)
    return;

  double start = process_time ();
  vrb ("starting trimming after %.2f seconds", start);

  size_t needed_clauses_size = (size_t)empty_clause + 1;
  used = calloc (needed_clauses_size, sizeof *used);
  if (!used)
    die ("out-of-memory allocating used stamps");

  if (empty_clause) {
    assert (EMPTY (work));
    mark_used (empty_clause, empty_clause);
    if (!is_original_clause (empty_clause))
      PUSH (work, empty_clause);

    while (!EMPTY (work)) {
      unsigned id = POP (work);
      assert (used[id]);
      int *a = antecedents.begin[id];
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

  free (work.begin);

  double end = process_time (), duration = end - start;
  vrb ("finished trimming after %.2f seconds", end);
  msg ("trimming proof took %.2f seconds", duration);
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
  int res = id < first_clause_added_in_proof ? id : map[abs_id];
  if (id < 0)
    res = -res;
  return res;
}

static void write_non_empty_proof () {

  assert (output.path);
  assert (output.file);

  assert (empty_clause > 0);
  size_t needed_clauses_size = (size_t)empty_clause + 1;

  links = calloc (needed_clauses_size, sizeof *links);
  if (!links)
    die ("out-of-memory allocating used links");
  heads = calloc (needed_clauses_size, sizeof *heads);
  if (!heads)
    die ("out-of-memory allocating used list headers");

  for (int id = 1; id != first_clause_added_in_proof; id++) {
    int where = used[id];
    if (where) {
      assert (id < where);
      assert (!is_original_clause (where));
      links[id] = heads[where];
      heads[where] = id;
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

  map = calloc (needed_clauses_size, sizeof *map);
  if (!map)
    die ("out-of-memory allocating identifier map");

  int id = first_clause_added_in_proof;
  int mapped = id;

  for (;;) {
    int where = used[id];
    if (where) {
      if (id != empty_clause) {
        assert (id < where);
        links[id] = heads[where];
        heads[where] = id;
        map[id] = mapped;
      }
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
      int head = heads[id];
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
          next = links[link];
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
  double start = process_time ();
  vrb ("starting writing proof after %.2f seconds", start);
  if (!proof.output)
    return;
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
  free (map);
  free (links);
  free (heads);
  free (used);
#if 0
  free (resolvent.begin);
#endif
  release_ints_map (&literals);
  release_ints_map (&antecedents);
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
      force = true;
    else if (!strcmp (arg, "-l") || !strcmp (arg, "--log") ||
             !strcmp (arg, "--logging"))
#ifdef LOGGING
      verbosity = INT_MAX;
#else
      die ("invalid option '-l' (build without logging support)");
#endif
    else if (!strcmp (arg, "-q") || !strcmp (arg, "--quiet"))
      verbosity = -1;
    else if (!strcmp (arg, "-t") || !strcmp (arg, "--track") ||
             !strcmp (arg, "--track-deleted"))
      track_deleted = true;
    else if (!strcmp (arg, "-v") || !strcmp (arg, "--verbose")) {
      if (verbosity <= 0)
        verbosity = 1;
    } else if (!strcmp (arg, "-n") || !strcmp (arg, "--no-trim") ||
               !strcmp (arg, "--no-trimming"))
      no_trimming = arg;
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

  if (size_files > 2 && no_trimming)
    die ("can not write to '%s' with '%s", files[2].path, no_trimming);

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
    } else {
      proof.input = file;
      if (no_trimming)
        die ("can not write to '%s' with '%s'", files[1].path, no_trimming);
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
      }
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
}

static void print_banner () {
  if (verbosity < 0)
    return;
  printf ("c LRAT-TRIM Version %s trims LRAT proofs\n"
          "c Copyright (c) 2023 Armin Biere University of Freiburg\n",
          version);

  assert (proof.input);
  const char *mode;
  if (cnf.input) {
    if (proof.output) {
      if (cnf.output)
        mode = "reading and writing both CNF and LRAT files";
      else
        mode = "reading CNF and LRAT files and writing LRAT file";
    } else if (cnf.output)
      mode = "reading CNF and LRAT files and writing CNF file";
    else
      mode = "reading CNF and LRAT file";
  } else {
    if (proof.output)
      mode = "reading and writing LRAT file";
    else
      mode = "only reading LRAT file";
  }
  printf ("c %s\n", mode);
  fflush (stdout);
}

static void resources () {
  msg ("maximum memory usage of %.0f MB", mega_bytes ());
  msg ("total time of %.2f seconds", process_time ());
}

int main (int argc, char **argv) {
  options (argc, argv);
  open_input_files ();
  print_banner ();
  parse_cnf ();
  parse_proof ();
  trim_proof ();
  write_proof ();
  write_cnf ();
  release ();
  resources ();
  return 0;
}

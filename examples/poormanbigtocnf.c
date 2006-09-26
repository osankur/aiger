/*------------------------------------------------------------------------*/
/* (C)opyright 2006, Armin Biere, Johannes Kepler University, see LICENSE */
/*------------------------------------------------------------------------*/

/* This utility 'poormanbigtocnf' is an example on how an AIG in binary
 * AIGER format can be read easily if a third party tool can not use the
 * AIGER library.  It even supports files compressed with 'gzip'.  Error
 * handling is complete but diagnostics could be more detailed.
 *
 * In principle reading can be further speed up, by for instance using
 * 'fread'.  In our experiments this gave a factor of sometimes 5 if no
 * output is produces ('--read-only').  However, we want to keep this
 * implementation simple and clean and writing the CNF dominates the overall
 * run time clearly
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

static unsigned M, I, L, O, A;

static int read_only;

static void
die (const char * fmt, ...)
{
  va_list ap;
  fputs ("*** poormanbigtocnf: ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

static int
u2i (unsigned l)
{
  /* We need one more literal in the CNF for TRUE.  This is the first
   * after the original literals in the BIG file.  Signs of literals in the
   * AIGER format are given by the LSB, while for DIMACS it is the sign.
   */
  if (l == 0)
    return -(M + 1);
  
  if (l == 1)
    return M + 1;	

  return ((l & 1) ? -1 : 1) * (l >> 1);
}

/* Print a unary clause.
 */
static void
c1 (unsigned a)
{
  if (!read_only)
    printf ("%d 0\n", u2i (a));
}

/* Print a binary clause.
 */
static void
c2 (unsigned a, unsigned b)
{
  if (!read_only)
    printf ("%d %d 0\n", u2i (a), u2i (b));
}

/* Print a ternary clause.
 */
static void
c3 (unsigned a, unsigned b, unsigned c)
{
  if (!read_only)
    printf ("%d %d %d 0\n", u2i (a), u2i (b), u2i (c));
}

static unsigned char
get (FILE * file)
{
  int ch = getc (file);
  if (ch == EOF)
    die ("unexpected end of file");
  return (unsigned char) ch;
}

static unsigned
decode (FILE * file)
{
  unsigned x = 0, i = 0;
  unsigned char ch;

  while ((ch = get (file)) & 0x80)
    x |= (ch & 0x7f) << (7 * i++);

  return x | (ch << (7 * i));
}

int
main (int argc, char ** argv)
{
  int close_file = 0, pclose_file = 0, verbose = 0;
  unsigned i, l, sat, lhs, rhs0, rhs1, delta;
  FILE * file = 0;

  for (i = 1; i < argc; i++)
    {
      if (!strcmp (argv[i], "-h"))
	{
	  fprintf (stderr, 
	           "usage: "
		   "poormanbigtocnf [-h][-v][--read-only][file.big[.gz]]\n");
	  exit (0);
	}
      else if (!strcmp (argv[i], "--read-only"))
	read_only = 1;
      else if (!strcmp (argv[i], "-v"))
	verbose = 1;
      else if (file)
	die ("more than one file specified");
      else if ((l = strlen (argv[i])) > 2 && !strcmp (argv[i] + l - 3, ".gz"))
	{
	  char * cmd = malloc (l + 20);
	  sprintf (cmd, "gunzip -c %s", argv[i]);
          if (!(file = popen (cmd, "r")))
	    die ("failed to open gzipped filed '%s' for reading", argv[i]);
	  free (cmd);
	  pclose_file = 1;
	}
      else if (!(file = fopen (argv[i], "r")))
	die ("failed to open '%s' for reading", argv[i]);
      else
	close_file = 1;
    }

  if (!file)
    file = stdin;

  if (fscanf (file, "big %u %u %u %u %u\n", &M, &I, &L, &O, &A) != 5)
    die ("invalid header");

  if (verbose)
    fprintf (stderr, "[poormanbigtocnf] big %u %u %u %u %u\n", M, I, L, O, A);

  if (L)
    die ("can not handle sequential models");

  if (O != 1)
    die ("expected exactly one output");

  if (fscanf (file, "%u\n", &sat) != 1)
    die ("failed to read single output literal");

  if (!read_only)
    printf ("p cnf %u %u\n", M + 1, A * 3 + 2);

  for (lhs = 2 * (I + L + 1); A--; lhs += 2)
    {
      delta = decode (file);
      if (delta >= lhs)
	die ("invalid byte encoding");
      rhs0 = lhs - delta;

      delta = decode (file);
      if (delta > rhs0)
	die ("invalid byte encoding");
      rhs1 = rhs0 - delta;

      c2 (lhs^1, rhs0);
      c2 (lhs^1, rhs1);
      c3 (lhs, rhs0^1, rhs1^1);
    }

  assert (lhs == 2 * (M + 1));

  c1 (lhs);	/* true */
  c1 (sat);	/* output */

  if (close_file)
    fclose (file);

  if (pclose_file)
    pclose (file);

  return 0;
}

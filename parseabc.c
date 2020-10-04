/* 
 * parseabc.c - code to parse an abc file. This file is used by the
 * following 3 programs :
 * abc2midi - program to convert abc files to MIDI files.
 * abc2abc  - program to manipulate abc files.
 * yaps     - program to convert abc to PostScript music files.
 * Copyright (C) 1999 James Allwright
 * e-mail: J.R.Allwright@westminster.ac.uk
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */


/* Macintosh port 30th July 1996 */
/* DropShell integration   27th Jan  1997 */
/* Wil Macaulay (wil@syndesis.com) */


#define TAB 9
#include "abc.h"
#include "parseabc.h"
#include <stdio.h>
#include <stdlib.h>
/* [JM] 2018-02-22 to handle strncasecmp() */
#include <string.h>

/* #define SIZE_ABBREVIATIONS ('Z' - 'H' + 1) [SS] 2016-09-20 */
#define SIZE_ABBREVIATIONS 58

/* [SS] 2015-09-28 changed _snprintf_s to _snprintf */
#ifdef _MSC_VER
#define snprintf _snprintf
#define strncasecmp strnicmp
#endif


#ifdef _MSC_VER
#define ANSILIBS
#define casecmp stricmp
#define _CRT_SECURE_NO_WARNINGS
#else
#define casecmp strcasecmp
#endif
#define	stringcmp	strcmp

#ifdef __MWERKS__
#define __MACINTOSH__ 1
#endif /* __MWERKS__ */

#ifdef __MACINTOSH__
#define main macabc2midi_main
#define STRCHR
#endif /* __MACINTOSH__ */

/* define USE_INDEX if your C libraries have index() instead of strchr() */
#ifdef USE_INDEX
#define strchr index
#endif

#ifdef ANSILIBS
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#else
extern char *malloc ();
extern char *strchr ();
#endif

int lineno;
int parsing_started = 0;
int parsing, slur;
int ignore_line = 0; /* [SS] 2017-04-12 */
int inhead, inbody;
int parserinchord;
static int ingrace = 0; /* [SS] 2020-06-01 */
int chorddecorators[DECSIZE];
char decorations[] = ".MLRH~Tuv'OPS"; /* 2020-05-01 */
char *abbreviation[SIZE_ABBREVIATIONS];

int voicecodes = 0;
/* [SS] 2015-03-16 allow 24 voices */
/*char voicecode[16][30];       for interpreting V: string */
char voicecode[24][30];		/*for interpreting V: string */

int decorators_passback[DECSIZE];
/* this global array is linked as an external to store.c and 
 * yaps.tree.c and is used to pass back decorator information
 * from event_instruction to parsenote.
*/

char inputline[512];		/* [SS] 2011-06-07 2012-11-22 */
char *linestart;		/* [SS] 2011-07-18 */
int lineposition;		/* [SS] 2011-07-18 */
char timesigstring[16];		/* [SS] 2011-08-19 links with stresspat.c */

int nokey = 0;			/* K: none was encountered */
int nokeysig = 0;               /* links with toabc.c [SS] 2016-03-03 */
int chord_n, chord_m;		/* for event_chordoff */
int fileline_number = 1;
int intune = 1;
int inchordflag;		/* [SS] 2012-03-30 */
struct fraction setmicrotone;	/* [SS] 2014-01-07 */
int microtone;			/* [SS] 2014-01-19 */
int temperament = 0;            /* [SS] 2020-06-25 */

extern programname fileprogram;
int oldchordconvention = 0;
char * abcversion = "2.0"; /* [SS] 2014-08-11 */
char lastfieldcmd = ' '; /* [SS] 2014-08-15 */

char *mode[10] = { "maj", "min", "m",
  "aeo", "loc", "ion", "dor", "phr", "lyd", "mix"
};

int modeshift[10] = { 0, -3, -3,
  -3, -5, 0, -2, -4, 1, -1
};

int modeminor[10] = { 0, 1, 1,
  1, 0, 0, 0, 0, 0, 0
};
int modekeyshift[10] = { 0, 5, 5, 5, 6, 0, 1, 2, 3, 4 };

int *
checkmalloc (bytes)
/* malloc with error checking */
     int bytes;
{
  int *p;

  p = (int *) malloc (bytes);
  if (p == NULL)
    {
      printf ("Out of memory error - malloc failed!\n");
      exit (0);
    };
  return (p);
}

char *
addstring (s)
/* create space for string and store it in memory */
     char *s;
{
  char *p;

  p = (char *) checkmalloc (strlen (s) + 1);
  strcpy (p, s);
  return (p);
}

/* [SS] 2014-08-16 [SDG] 2020-06-03 */
char * concatenatestring(s1,s2)
   char * s1;
   char * s2;
{ 
   int len = strlen(s1) + strlen(s2) + 1;
   char *p = (char *) checkmalloc(len);
   snprintf(p,len, "%s%s",s1,s2);
  return p;
}


void
initvstring (s)
     struct vstring *s;
/* initialize vstring (variable length string data structure) */
{
  s->len = 0;
  s->limit = 40;
  s->st = (char *) checkmalloc (s->limit + 1);
  *(s->st) = '\0';
}

void
extendvstring (s)
     struct vstring *s;
/* doubles character space available in string */
{
  char *p;

  if (s->limit > 0)
    {
      s->limit = s->limit * 2;
      p = (char *) checkmalloc (s->limit + 1);
      strcpy (p, s->st);
      free (s->st);
      s->st = p;
    }
  else
    {
      initvstring (s);
    };
}

void
addch (ch, s)
     char ch;
     struct vstring *s;
/* appends character to vstring structure */
{
  if (s->len >= s->limit)
    {
      extendvstring (s);
    };
  *(s->st + s->len) = ch;
  *(s->st + (s->len) + 1) = '\0';
  s->len = (s->len) + 1;
}

void
addtext (text, s)
     char *text;
     struct vstring *s;
/* appends a string to vstring data structure */
{
  int newlen;

  newlen = s->len + strlen (text);
  while (newlen >= s->limit)
    {
      extendvstring (s);
    };
  strcpy (s->st + s->len, text);
  s->len = newlen;
}

void
clearvstring (s)
     struct vstring *s;
/* set string to empty */
/* does not deallocate memory ! */
{
  *(s->st) = '\0';
  s->len = 0;
}

void
freevstring (s)
     struct vstring *s;
/* deallocates memory allocated for string */
{
  if (s->st != NULL)
    {
      free (s->st);
      s->st = NULL;
    };
  s->len = 0;
  s->limit = 0;
}

void
parseron ()
{
  parsing = 1;
  slur = 0;
  parsing_started = 1;
}

void
parseroff ()
{
  parsing = 0;
  slur = 0;
}

/* [SS] 2017-04-12 */
void handle_abc2midi_parser (line) 
char *line;
{
char *p;
p = line;
if (strncasecmp(p,"%%MidiOff",9) == 0) {
  ignore_line = 1;
  }
if (strncasecmp(p,"%%MidiOn",8) == 0) {
  ignore_line = 0;
  }
}


int
getarg (option, argc, argv)
/* look for argument 'option' in command line */
     char *option;
     char *argv[];
     int argc;
{
  int j, place;

  place = -1;
  for (j = 0; j < argc; j++)
    {
      if (strcmp (option, argv[j]) == 0)
	{
	  place = j + 1;
	};
    };
  return (place);
}

void
skipspace (p)
     char **p;
{
  /* skip space and tab */
  while (((int) **p == ' ') || ((int) **p == TAB))
    *p = *p + 1;
}

void
skiptospace (p)
     char **p;
{
  while (((int) **p != ' ') && ((int) **p != TAB) && (int) **p != '\0')
    *p = *p + 1;
}


int
isnumberp (p)
     char **p;
/* returns 1 if positive number, returns 0 if not a positive number */
/* ie -4 or 1A both return 0. This function is needed to get the    */
/* voice number.                                                    */
{
  char **c;
  c = p;
  while (( **c != ' ') && ( **c != TAB) &&  **c != '\0')
    {
      if (( **c >= 0) &&  (**c <= 9)) /*[SDG] 2020-06-03 */
	*c = *c + 1;
      else
	return 0;
    }
  return 1;
/* could use isdigit(**c) */
/* is zero a positive number? is V:0 ok? [SS] */
}



int
readnumf (num)
     char *num;
/* read integer from string without advancing character pointer */
{
  int t;
  char *p;

  p = num;
  if (!isdigit (*p))
    {
      event_error ("Missing Number");
    };
  t = 0;
  while (((int) *p >= '0') && ((int) *p <= '9'))
    {
      t = t * 10 + (int) *p - '0';
      p = p + 1;
    };
  return (t);
}

int
readsnumf (s)
     char *s;
/* reads signed integer from string without advancing character pointer */
{
  char *p;

  p = s;
  if (*p == '-')
    {
      p = p + 1;
      skipspace (&p);
      return (-readnumf (p));
    }
  else
    {
      return (readnumf (p));
    }
}

int
readnump (p)
     char **p;
/* read integer from string and advance character pointer */
{
  int t;

  t = 0;
  while (((int) **p >= '0') && ((int) **p <= '9'))
    {
      t = t * 10 + (int) **p - '0';
      *p = *p + 1;
    };
  return (t);
}

int
readsnump (p)
     char **p;
/* reads signed integer from string and advance character pointer */
{
  if (**p == '-')
    {
      *p = *p + 1;
      skipspace (p);
      return (-readnump (p));
    }
  else
    {
      return (readnump (p));
    }
}

void
readsig (a, b, sig)
     int *a, *b;
     char **sig;
/* read time signature (meter) from M: field */
{
  int t;
  char c; /* [SS] 2015-08-19 */

  /* [SS] 2012-08-08  cut time (C| or c|) is 2/2 not 4/4 */
  if ((*(*sig + 1) == '|') && ((**sig == 'C') || (**sig == 'c')))
    {
      *a = 2;
      *b = 2;
      return;
    }

  if ((**sig == 'C') || (**sig == 'c'))
    {
      *a = 4;
      *b = 4;
      return;
    };
  *a = readnump (sig);

  /* [SS] 2015-08-19 */
  while ((int) **sig == '+') {
    *sig = *sig + 1;
    c = readnump (sig);
    *a = *a + c;
    }

  if ((int) **sig != '/')
    {
      event_error ("Missing / ");
    }
  else
    {
      *sig = *sig + 1;
    };
  *b = readnump (sig);
  if ((*a == 0) || (*b == 0))
    {
      event_error ("Expecting fraction in form A/B");
    }
  else
    {
      t = *b;
      while (t > 1)
	{
	  if (t % 2 != 0)
	    {
	      event_error ("divisor must be a power of 2");
	      t = 1;
	      *b = 0;
	    }
	  else
	    {
	      t = t / 2;
	    };
	};
    };
}

void
readlen (a, b, p)
     int *a, *b;
     char **p;
/* read length part of a note and advance character pointer */
{
  int t;

  *a = readnump (p);
  if (*a == 0)
    {
      *a = 1;
    };
  *b = 1;
  if (**p == '/')
    {
      *p = *p + 1;
      *b = readnump (p);
      if (*b == 0)
	{
	  *b = 2;
	  while (**p == '/')
	    {
	      *b = *b * 2;
	      *p = *p + 1;
	    };
	};
    };
  t = *b;
  while (t > 1)
    {
      if (t % 2 != 0)
	{
	  event_warning ("divisor not a power of 2");
	  t = 1;
	}
      else
	{
	  t = t / 2;
	};
    };
}

void
read_microtone_value (a, b, p)
     int *a, *b;
     char **p;
/* read length part of a note and advance character pointer */
{
  int t;

  *a = readnump (p);
  if (*a == 0)
    {
      *a = 1;
    };
  *b = 1;
  if (**p == '/')
    {
      *p = *p + 1;
      *b = readnump (p);
      if (*b == 0)
	{
	  *b = 2;
	  while (**p == '/')
	    {
	      *b = *b * 2;
	      *p = *p + 1;
	    };
	};
    } else {
	*b = 0; /* [SS] 2020-06-23 */
	/* To signal that the microtone value is not a fraction */
    }	;
  t = *b;
  while (t > 1)
    {
      if (t % 2 != 0)
	{
	  /*event_warning("divisor not a power of 2"); */
	  t = 1;
	}
      else
	{
	  t = t / 2;
	};
    };
}

int
ismicrotone (p, dir)
     char **p;
     int dir;
{
  int a, b;
  char *chp; /* [HL] 2020-06-20 */
  chp = *p;
  read_microtone_value (&a, &b, p);
  /* readlen_nocheck advances past microtone indication if present */
  if (chp != *p) /* [HL] 2020-06-20 */
    {
      /* printf("event_microtone a = %d b = %d\n",a,b); */
      event_microtone (dir, a, b);
      return 1;
    }
  setmicrotone.num = 0;
  setmicrotone.denom = 0;
  return 0;
}




int
isclef (s, gotoctave, octave, strict)
     char *s;
     int *gotoctave, *octave;
     int strict;
/* part of K: parsing - looks for a clef in K: field                 */
/* format is K:string where string is treble, bass, baritone, tenor, */
/* alto, mezzo, soprano or K:clef=arbitrary                          */
{
  int gotclef;
  s = s;
  gotclef = 0;
  if (strncmp (s, "bass", 4) == 0)
    {
      gotclef = 1;
      *octave = 0; /* [SS] 2020-05-06 */
    };
  if (strncmp (s, "treble", 6) == 0)
    {
      gotclef = 1;
      /* [SS] 2020-05-06 */
      /*if (fileprogram == ABC2MIDI && *gotoctave != 1 && *octave != 1)*/
      if (fileprogram == ABC2MIDI)
        {
        /* [SS] 2015-07-02  2019-01-20*/
	/* event_warning ("clef= is overriding octave= setting"); */
        *gotoctave = 1;		/* [SS] 2011-12-19 */
        *octave = 0; /* [SS] 2020-05-06 */
        }
    };
  if (strncmp (s, "treble+8", 8) == 0)
    {
      gotclef = 1;
      /* [SS] 2020-05-06 */
      /*if (fileprogram == ABC2MIDI && *gotoctave != 1 && *octave != 1)*/
      if (fileprogram == ABC2MIDI)
        {
	/* event_warning ("clef= is overriding octave= setting"); */
        /* [SS] 2015-07-02 2019-01-20 */
        *gotoctave = 1;
        *octave = 1;
        }
    };
  if (strncmp (s, "treble-8", 8) == 0)
    {
      gotclef = 1;
      /* [SS] 2020-05-06 */
      /*if (fileprogram == ABC2MIDI && *gotoctave == 1 && *octave != -1)*/
      if (fileprogram == ABC2MIDI)
        {
	/* event_warning ("clef= is overriding octave= setting"); */
        *gotoctave = 1;
        *octave = -1;
        }
    };
  if (strncmp (s, "baritone", 8) == 0)
    {
      gotclef = 1;
      *octave = 0; /* [SS] 2020-05-06 */
    };
  if (strncmp (s, "tenor", 5) == 0)
    {
      gotclef = 1;
      *octave=0;  /* [SS] 2020-05-06 */
    };
  if (strncmp (s, "tenor-8", 7) == 0)
    {
      gotclef = 1;
      /* [SS] 2020-05-06 */
      /*if (fileprogram == ABC2MIDI && *gotoctave == 1 && *octave != -1) {*/
      if (fileprogram == ABC2MIDI) {
	/*event_warning ("clef= is overriding octave= setting");*/
        *gotoctave = 1;
        *octave = -1;
        }
    };
  if (strncmp (s, "alto", 4) == 0)
    {
      gotclef = 1;
      *octave=0;  /* [SS] 2020-05-06 */
    };
  if (strncmp (s, "mezzo", 5) == 0)
    {
      gotclef = 1;
      *octave=0;  /* [SS] 2020-05-06 */
    };
  if (strncmp (s, "soprano", 7) == 0)
    {
      gotclef = 1;
      *octave=0;  /* [SS] 2020-05-06 */
    };
/*
 * only clef=F or clef=f is allowed, or else
 * we get a conflict with the key signature
 * indication K:F
*/

  if (strncmp (s, "f", 1) == 0 && strict == 0)
    {
      gotclef = 1;
      *octave=0;  /* [SS] 2020-05-06 */
    }
  if (strncmp (s, "F", 1) == 0 && strict == 0)
    {
      gotclef = 1;
      *octave=0;  /* [SS] 2020-05-06 */
    }
  if (strncmp (s, "g", 1) == 0 && strict == 0)
    {
      gotclef = 1;
      *octave=0;  /* [SS] 2020-05-06 */
    }
  if (strncmp (s, "G", 1) == 0 && strict == 0)
    {
      gotclef = 1;
      *octave=0;  /* [SS] 2020-05-06 */
    }
  if (strncmp (s, "perc", 1) == 0 && strict == 0)
    {
      gotclef = 1;
      *octave=0;  /* [SS] 2020-05-06 */
    }				/* [SS] 2011-04-17 */

  if (!strict && !gotclef)
    {
      gotclef = 1;
      event_warning ("cannot recognize clef indication");
    }

  return (gotclef);
}



char *
readword (word, s)
/* part of parsekey, extracts word from input line */
/* besides the space, the symbols _, ^, and = are used */
/* as separators in order to handle key signature modifiers. */
/* [SS] 2010-05-24 */
     char word[];
     char *s;
{
  char *p;
  int i;

  p = s;
  i = 0;
  /* [SS] 2015-04-08 */
  while ((*p != '\0') && (*p != ' ') && (*p != '\t') && ((i == 0) ||
							 ((*p != '='))))
    {
    if (i >1 && *p == '^') break; /* allow for double sharps and flats */
    if (i >1 && *p == '_') break;
      if (i < 29)
	{
	  word[i] = *p;
	  i = i + 1;
	};
      p = p + 1;
    };
  word[i] = '\0';
  return (p);
}

void
lcase (s)
/* convert word to lower case */
     char *s;
{
  char *p;

  p = s;
  while (*p != '\0')
    {
      if (isupper (*p))
	{
	  *p = *p + 'a' - 'A';
	};
      p = p + 1;
    };
}


void
init_voicecode ()
{
  int i;
  for (i = 0; i < 24; i++) /* [SS} 2015-03-15 */
    voicecode[i][0] = 0;
  voicecodes = 0;
}

void
print_voicecodes ()
{
  int i;
  if (voicecodes == 0)
    return;
  printf ("voice mapping:\n");
  for (i = 0; i < voicecodes; i++)
    {
      if (i % 4 == 3)
	printf ("\n");
      printf ("%s  %d   ", voicecode[i], i + 1);
    }
  printf ("\n");
}

int
interpret_voicestring (char *s)
{
/* if V: is followed  by a string instead of a number
 * we check to see if we have encountered this string
 * before. We assign the number associated with this
 * string and add it to voicecode if it was not encountered
 * before. If more than 16 distinct strings were encountered
 * we report an error -1.
*/
  int i;
  char code[32];
  char msg[80];			/* [PHDM] 2012-11-22 */
  char *c;
  c = readword (code, s);

/* [PHDM] 2012-11-22 */
  if (*c != '\0' && *c != ' ' && *c != ']')
    {
      sprintf (msg, "invalid character `%c' in Voice ID", *c);
      event_error (msg);
    }
/* [PHDM] 2012-11-22 */

  if (code[0] == '\0')
    return 0;
  if (voicecodes == 0)
    {
      strcpy (voicecode[voicecodes], code);
      voicecodes++;
      return voicecodes;
    }
  for (i = 0; i < voicecodes; i++)
    if (stringcmp (code, voicecode[i]) == 0)
      return (i + 1);
  if ((voicecodes + 1) > 23) /* [SS] 2015-03-16 */
    return -1;
  strcpy (voicecode[voicecodes], code);
  voicecodes++;
  return voicecodes;
}

/* The following three functions parseclefs, parsetranspose,
 * parseoctave are used to parse the K: field which not
 * only specifies the key signature but also other descriptors
 * used for producing a midi file or postscript file.
 *
 * The char* word contains the particular token that
 * is being interpreted. If the token can be understood,
 * other parameters are extracted from char ** s and
 * s is advanced to point to the next token.
 */

int
parseclef (s, word, gotclef, clefstr, gotoctave, octave)
     char **s;
     char *word;
     int *gotclef;
     char *clefstr;
     int *gotoctave, *octave;
/* extracts string clef= something */
{
  int successful;
  skipspace (s);
  *s = readword (word, *s);
  successful = 0;
  if (casecmp (word, "clef") == 0)
    {
      skipspace (s);
      if (**s != '=')
	{
	  event_error ("clef must be followed by '='");
	}
      else
	{
	  *s = *s + 1;
	  skipspace (s);
	  *s = readword (clefstr, *s);
	  if (isclef (clefstr, gotoctave, octave, 0))
	    {
	      *gotclef = 1;
	    };
	};
      successful = 1;
    }
  else if (isclef (word, gotoctave, octave, 1))
    {
      *gotclef = 1;
      strcpy (clefstr, word);
      successful = 1;
    };
  return successful;
}


int
parsetranspose (s, word, gottranspose, transpose)
/* parses string transpose= number */
     char **s;
     char *word;
     int *gottranspose;
     int *transpose;
{
  if (casecmp (word, "transpose") != 0)
    return 0;
  skipspace (s);
  if (**s != '=')
    {
      event_error ("transpose must be followed by '='");
    }
  else
    {
      *s = *s + 1;
      skipspace (s);
      *transpose = readsnump (s);
      *gottranspose = 1;
    };
  return 1;
};


int
parseoctave (s, word, gotoctave, octave)
/* parses string octave= number */
     char **s;
     char *word;
     int *gotoctave;
     int *octave;
{
  if (casecmp (word, "octave") != 0)
    return 0;
  skipspace (s);
  if (**s != '=')
    {
      event_error ("octave must be followed by '='");
    }
  else
    {
      *s = *s + 1;
      skipspace (s);
      *octave = readsnump (s);
      *gotoctave = 1;
    };
  return 1;
};


int
parsename (s, word, gotname, namestring, maxsize)
/* parses string name= "string" in V: command 
   for compatability of abc2abc with abcm2ps
*/
     char **s;
     char *word;
     int *gotname;
     char namestring[];
     int maxsize;
{
  int i;
  i = 0;
  if (casecmp (word, "name") != 0)
    return 0;
  skipspace (s);
  if (**s != '=')
    {
      event_error ("name must be followed by '='");
    }
  else
    {
      *s = *s + 1;
      skipspace (s);
      if (**s == '"')		/* string enclosed in double quotes */
	{
	  namestring[i] = (char) **s;
	  *s = *s + 1;
	  i++;
	  while (i < maxsize && **s != '"' && **s != '\0')
	    {
	      namestring[i] = (char) **s;
	      *s = *s + 1;
	      i++;
	    }
	  namestring[i] = (char) **s;	/* copy double quotes */
	  i++;
	  namestring[i] = '\0';
	}
      else			/* string not enclosed in double quotes */
	{
	  while (i < maxsize && **s != ' ' && **s != '\0')
	    {
	      namestring[i] = (char) **s;
	      *s = *s + 1;
	      i++;
	    }
	  namestring[i] = '\0';
	}
      *gotname = 1;
    }
  return 1;
};

int
parsesname (s, word, gotname, namestring, maxsize)
/* parses string name= "string" in V: command 
   for compatability of abc2abc with abcm2ps
*/
     char **s;
     char *word;
     int *gotname;
     char namestring[];
     int maxsize;
{
  int i;
  i = 0;
  if (casecmp (word, "sname") != 0)
    return 0;
  skipspace (s);
  if (**s != '=')
    {
      event_error ("name must be followed by '='");
    }
  else
    {
      *s = *s + 1;
      skipspace (s);
      if (**s == '"')		/* string enclosed in double quotes */
	{
	  namestring[i] = (char) **s;
	  *s = *s + 1;
	  i++;
	  while (i < maxsize && **s != '"' && **s != '\0')
	    {
	      namestring[i] = (char) **s;
	      *s = *s + 1;
	      i++;
	    }
	  namestring[i] = (char) **s;	/* copy double quotes */
	  i++;
	  namestring[i] = '\0';
	}
      else			/* string not enclosed in double quotes */
	{
	  while (i < maxsize && **s != ' ' && **s != '\0')
	    {
	      namestring[i] = (char) **s;
	      *s = *s + 1;
	      i++;
	    }
	  namestring[i] = '\0';
	}
      *gotname = 1;
    }
  return 1;
};

int
parsemiddle (s, word, gotmiddle, middlestring, maxsize)
/* parse string middle=X in V: command
 for abcm2ps compatibility
*/
     char **s;
     char *word;
     int *gotmiddle;
     char middlestring[];
     int maxsize;
{
  int i;
  i = 0;
  if (casecmp (word, "middle") != 0)
    return 0;
  skipspace (s);
  if (**s != '=')
    {
      event_error ("middle must be followed by '='");
    }
  else
    {
      *s = *s + 1;
      skipspace (s);
/* we really ought to check the we have a proper note name; for now, just copy non-space
characters */
      while (i < maxsize && **s != ' ' && **s != '\0')
	{
	  middlestring[i] = (char) **s;
	  *s = *s + 1;
	  ++i;
	}
      middlestring[i] = '\0';
      *gotmiddle = 1;
    }
  return 1;
}

int
parseother (s, word, gotother, other, maxsize)	/* [SS] 2011-04-18 */
/* parses any left overs in V: command (eg. stafflines=1) */
     char **s;
     char *word;
     int *gotother;
     char other[];
     int maxsize;
{
  if (word[0] != '\0')
    {
      if ( (int) strlen (other) < maxsize) /* [SS] 2015-10-08 added (int) */
	strncat (other, word, maxsize);
      if (**s == '=')
	{			/* [SS] 2011-04-19 */
	  *s = readword (word, *s);
	  if ( (int) strlen (other) < maxsize) /* [SS] 2015-10-08 added (int) */
	    strncat (other, word, maxsize);
	}
      strncat (other, " ", maxsize);	/* in case other codes follow */
      *gotother = 1;
      return 1;
    }
  return 0;
}


static void process_microtones (int *parsed,  char word[],
  char modmap[], int modmul[], struct fraction modmicrotone[])
{
  int a, b;                     /* for microtones [SS] 2014-01-06 */
  char c;
  int j;
  int success;

  /* shortcuts such as ^/4G instead of ^1/4G not allowed here */

	  success = sscanf (&word[1], "%d/%d%c", &a, &b, &c);
	  if (success == 3) /* [SS] 2016-04-10 */
	    {
	      *parsed = 1;
	      j = (int) c - 'A';
              if (j > 7) j = (int) c - 'a';
              if (j > 7 || j < 0) {printf("invalid j = %d\n",j); exit(-1);}
	      if (word[0] == '_') a = -a;
	      /* printf("%s fraction microtone  %d/%d for %c\n",word,a,b,c); */
	   } else {
	    success = sscanf (&word[1], "%d%c", &a, &c); /* [SS] 2020-06-25 */
            if (success == 2)
	    {
            b = 0;
            /* printf("%s integer microtone %d%c\n",word,a,c); */
	    if (temperament != 1) { /* [SS] 2020-06-25 2020-07-05 */
		    event_warning("do not use integer microtone without calling %%MIDI temperamentequal");
	            }
	    *parsed = 1;
	    }
          }
	  /* if (parsed ==1)  [SS] 2020-09-30 */
	  if (success > 0) {
            j = (int) c - 'A';
            if (j > 7) j = (int) c - 'a';
            if (j > 7 || j < 0) {printf("invalid j = %d\n",j); exit(-1);}
            if (word[0] == '_') a = -a;
	    modmap[j] = word[0];
	    modmicrotone[j].num = a;
	    modmicrotone[j].denom = b;
	    /* printf("%c microtone = %d/%d\n",modmap[j],modmicrotone[j].num,modmicrotone[j].denom); */
	  }
	} /* finished ^ = _ */

int
parsekey (str)
/* parse contents of K: field */
/* this works by picking up a strings and trying to parse them */
/* returns 1 if valid key signature found, 0 otherwise */
/* This is unfortunately a complicated function because it does alot.
 * It prepares the data:
 * sf = number of sharps or flats in key signature
 * modeindex:= major 0, minor 1, ... dorian, etc.
 * modmap: eg "^= _^   " corresponds to A# B Cb D#    
 * modmul: 2 signals double sharp or flat
 * modmicrotone: eg {2/4 0/0 0/0 2/0 0/0 -3/0 0/0} for ^2/4A ^2D _3F
 * clefstr: treble, bass, treble+8, ... 
 * octave: eg. 1,2,-1 ... 
 * transpose: 1,2 for handling certain clefs 
 * and various flags like explict, gotoctave, gottranspose, gotclef, gotkey
 * which are all sent to abc2midi (via store.c), yaps (via yapstree), abc2abc
 * via (toabc.c), using the function call event_key().
 *
 * The variables sf, modeindex, modmul, and modmicrotone control which notes
 * are sharpened or flatten in a musical measure.
 * The variable clefstr selects one of the clefs, (treble, bass, ...)
 * The variable octave allows you to shift everything up and down by an octave
 * The variable transpose allows you to automatically transpose every note.
 *
 * All of this information is extracted from the string str from the
 * K: command.
 */
     char *str;
{
  char *s;
  char word[30];
  int parsed = 0;  /* [SDG] 2020-06-03 */
  int gotclef, gotkey, gotoctave, gottranspose;
  int explict;			/* [SS] 2010-05-08 */
  int modnotes;			/* [SS] 2010-07-29 */
  int foundmode;
  int transpose, octave;
  char clefstr[30];
  char modestr[30];
  char msg[80];
  char *moveon;
  int sf = -1, minor = -1;
  char modmap[7];
  int modmul[7];
  struct fraction modmicrotone[7];
  int i, j;
  int cgotoctave, coctave;
  char *key = "FCGDAEB";
  int modeindex;
  int a, b;			/* for microtones [SS] 2014-01-06 */
  int success;
  char c;

  clefstr[0] = (char) 0;
  modestr[0] = (char) 0;
  s = str;
  transpose = 0;
  gottranspose = 0;
  octave = 0;
  gotkey = 0;
  gotoctave = 0;
  gotclef = 0;
  cgotoctave = 0;
  coctave = 0;
  modeindex = 0;
  explict = 0;
  modnotes = 0;
  nokey = nokeysig; /* [SS] 2016-03-03 */
  for (i = 0; i < 7; i++)
    {
      modmap[i] = ' ';
      modmul[i] = 1;
      modmicrotone[i].num = 0;	/* [SS] 2014-01-06 */
      modmicrotone[i].denom = 0;
    };
  word[0] = 0; /* in case of empty string [SDG] 2020-06-04 */
  while (*s != '\0')
    {
      parsed = parseclef (&s, word, &gotclef, clefstr, &cgotoctave, &coctave);
      /* parseclef also scans the s string using readword(), placing */
      /* the next token  into the char array word[].                   */
      if (!parsed)
	parsed = parsetranspose (&s, word, &gottranspose, &transpose);

      if (!parsed)
	parsed = parseoctave (&s, word, &gotoctave, &octave);

      if ((parsed == 0) && (casecmp (word, "Hp") == 0))
	{
	  sf = 2;
	  minor = 0;
	  gotkey = 1;
	  parsed = 1;
	};

      if ((parsed == 0) && (casecmp (word, "none") == 0))
	{
	  gotkey = 1;
	  parsed = 1;
	  nokey = 1;
	  minor = 0;
	  sf = 0;
	}

      if (casecmp (word, "exp") == 0)
	{
	  explict = 1;
	  parsed = 1;
	}

/* if K: not 'none', 'Hp' or 'exp' then look for key signature
 * like Cmaj Amin Ddor ...                                   
 * The key signature is expressed by sf which indicates the
 * number of sharps (if positive) or flats (if negative)       */
 
      if ((parsed == 0) && ((word[0] >= 'A') && (word[0] <= 'G')))
	{
	  gotkey = 1;
	  parsed = 1;
	  /* parse key itself */
	  sf = strchr (key, word[0]) - key - 1;
	  j = 1;
	  /* deal with sharp/flat */
	  if (word[1] == '#')
	    {
	      sf += 7;
	      j = 2;
	    }
	  else
	    {
	      if (word[1] == 'b')
		{
		  sf -= 7;
		  j = 2;
		};
	    }
	  minor = 0;
	  foundmode = 0;
	  if ((int) strlen (word) == j)
	    {
	      /* look at next word for mode */
	      skipspace (&s);
	      moveon = readword (modestr, s);
	      lcase (modestr);
	      for (i = 0; i < 10; i++)
		{
		  if (strncmp (modestr, mode[i], 3) == 0)
		    {
		      foundmode = 1;
		      sf = sf + modeshift[i];
		      minor = modeminor[i];
		      modeindex = i;
		    };
		};
	      if (foundmode)
		{
		  s = moveon;
		};
	    }
	  else
	    {
	      strcpy (modestr, &word[j]);
	      lcase (modestr);
	      for (i = 0; i < 10; i++)
		{
		  if (strncmp (modestr, mode[i], 3) == 0)
		    {
		      foundmode = 1;
		      sf = sf + modeshift[i];
		      minor = modeminor[i];
		      modeindex = i;
		    };
		};
	      if (!foundmode)
		{
		  sprintf (msg, "Unknown mode '%s'", &word[j]);
		  event_error (msg);
		  modeindex = 0;
		};
	    };
	};
      if (gotkey)
	{
	  if (sf > 7)
	    {
	      event_warning ("Unusual key representation");
	      sf = sf - 12;
	    };
	  if (sf < -7)
	    {
	      event_warning ("Unusual key representation");
	      sf = sf + 12;
	    };
	};


      /* look for key signature modifiers
       * For example K:D _B
       * which will include a Bb in the D major key signature
       *                                                      */

      if ((word[0] == '^') || (word[0] == '_') || (word[0] == '='))
	{
	  modnotes = 1;
	  if ((strlen (word) == 2) && (word[1] >= 'a') && (word[1] <= 'g'))
	    {
	      j = (int) word[1] - 'a';
	      modmap[j] = word[0];
	      modmul[j] = 1;
	      parsed = 1;
	    }
	  else
	    {			/*double sharp or double flat */
	      if ((strlen (word) == 3) && (word[0] != '=')
		  && (word[0] == word[1]) && (word[2] >= 'a')
		  && (word[2] <= 'g'))
		{
		  j = (int) word[2] - 'a';
		  modmap[j] = word[0];
		  modmul[j] = 2;
		  parsed = 1;
		};
	    };
	};

/*   if (explict)  for compatibility with abcm2ps 2010-05-08  2010-05-20 */
      if ((word[0] == '^') || (word[0] == '_') || (word[0] == '='))
	{
	  modnotes = 1;
	  if ((strlen (word) == 2) && (word[1] >= 'A') && (word[1] <= 'G'))
	    {
	      j = (int) word[1] - 'A';
	      modmap[j] = word[0];
	      modmul[j] = 1;
	      parsed = 1;
	    }
	  else if		/*double sharp or double flat */
	    ((strlen (word) == 3) && (word[0] != '=') && (word[0] == word[1])
	     && (word[2] >= 'A') && (word[2] <= 'G'))
	    {
	      j = (int) word[2] - 'A';
	      modmap[j] = word[0];
	      modmul[j] = 2;
	      parsed = 1;
	    };
   }
	  /* microtone? */
	  /* shortcuts such as ^/4G instead of ^1/4G not allowed here */
	  /* parsed =0; [SS] 2020-09-30 */
   process_microtones (&parsed,  word,
        modmap, modmul, modmicrotone);
   }

  if ((parsed == 0) && (strlen (word) > 0))
    {
      sprintf (msg, "Ignoring string '%s' in K: field", word);
      event_warning (msg);
    };
  if (cgotoctave)
    {
      gotoctave = 1;
      octave = coctave;
    }
  if (modnotes & !gotkey)
    {				/*[SS] 2010-07-29 for explicit key signature */
      sf = 0;
      /*gotkey = 1; [SS] 2010-07-29 */
      explict = 1;		/* [SS] 2010-07-29 */
    }
  event_key (sf, str, modeindex, modmap, modmul, modmicrotone, gotkey,
	     gotclef, clefstr, octave, transpose, gotoctave, gottranspose,
	     explict);
  return (gotkey);
}


void
parsevoice (s)
     char *s;
{
  int num;			/* voice number */
  struct voice_params vparams;
  char word[64]; /* 2017-10-11 */
  int parsed;
  int coctave, cgotoctave;

  vparams.transpose = 0;
  vparams.gottranspose = 0;
  vparams.octave = 0;
  vparams.gotoctave = 0;
  vparams.gotclef = 0;
  cgotoctave = 0;
  coctave = 0;
  vparams.gotname = 0;
  vparams.gotsname = 0;
  vparams.gotmiddle = 0;
  vparams.gotother = 0;		/* [SS] 2011-04-18 */
  vparams.other[0] = '\0';	/* [SS] 2011-04-18 */

  skipspace (&s);
  if (isnumberp (&s) == 1)
    {
      num = readnump (&s);
    }
  else
    {
      num = interpret_voicestring (s);
      if (num == 0)
	event_error ("No voice number or string in V: field");
      if (num == -1)
	{
	  event_error ("More than 16 voices encountered in V: fields");
	  num = 0;
	}
      skiptospace (&s);
    };
  skipspace (&s);
  while (*s != '\0')
    {
      parsed =
	parseclef (&s, word, &vparams.gotclef, vparams.clefname, &cgotoctave,
		   &coctave);
      if (!parsed)
	parsed =
	  parsetranspose (&s, word, &vparams.gottranspose,
			  &vparams.transpose);
      if (!parsed)
	parsed = parseoctave (&s, word, &vparams.gotoctave, &vparams.octave);
      if (!parsed)
	parsed =
	  parsename (&s, word, &vparams.gotname, vparams.namestring,
		     V_STRLEN);
      if (!parsed)
	parsed =
	  parsesname (&s, word, &vparams.gotsname, vparams.snamestring,
		      V_STRLEN);
      if (!parsed)
	parsed =
	  parsemiddle (&s, word, &vparams.gotmiddle, vparams.middlestring,
		       V_STRLEN);
      if (!parsed)
	parsed = parseother (&s, word, &vparams.gotother, vparams.other, V_STRLEN);	/* [SS] 2011-04-18 */
    }
  /* [SS] 2015-05-13 allow octave= to change the clef= octave setting */
  /* cgotoctave may be set to 1 by a clef=. vparams.gotoctave is set  */
  /* by octave= */

  if (cgotoctave && vparams.gotoctave == 0)
    {
      vparams.gotoctave = 1;
      vparams.octave = coctave;
    }
  event_voice (num, s, &vparams);

/*
if (gottranspose) printf("transpose = %d\n", vparams.transpose);
 if (gotoctave) printf("octave= %d\n", vparams.octave);
 if (gotclef) printf("clef= %s\n", vparams.clefstr);
if (gotname) printf("parsevoice: name= %s\n", vparams.namestring);
if(gotmiddle) printf("parsevoice: middle= %s\n", vparams.middlestring);
*/
}


void
parsenote (s)
     char **s;
/* parse abc note and advance character pointer */
{
  int decorators[DECSIZE];
  int i, t;
  int mult;
  char accidental, note;
  int octave, n, m;
  char msg[80];

  mult = 1;
  microtone = 0;
  accidental = ' ';
  note = ' ';
  for (i = 0; i < DECSIZE; i++)
    {
      decorators[i] = decorators_passback[i];
      if (!inchordflag)
	decorators_passback[i] = 0;	/* [SS] 2012-03-30 */
    }
  while (strchr (decorations, **s) != NULL)
    {
      t = strchr (decorations, **s) - decorations;
      decorators[t] = 1;
      *s = *s + 1;
    };
  /*check for decorated chord */
  if (**s == '[')
    {
      lineposition = *s - linestart;	/* [SS] 2011-07-18 */
      if (fileprogram == YAPS)
	event_warning ("decorations applied to chord");
      for (i = 0; i < DECSIZE; i++)
	chorddecorators[i] = decorators[i];
      event_chordon (chorddecorators);
      if (fileprogram == ABC2ABC)
	for (i = 0; i < DECSIZE; i++)
	  decorators[i] = 0;
      parserinchord = 1;
      *s = *s + 1;
      skipspace (s);
    };
  if (parserinchord)
    {
      /* inherit decorators */
      if (fileprogram != ABC2ABC)
	for (i = 0; i < DECSIZE; i++)
	  {
	    decorators[i] = decorators[i] | chorddecorators[i];
	  };
    };

/* [SS] 2011-12-08 to catch fermata H followed by a rest */

  if (**s == 'z')
    {
      *s = *s + 1;
      readlen (&n, &m, s);
      event_rest (decorators, n, m, 0);
      return;
    }
  if (**s == 'x')
    {
      *s = *s + 1;
      readlen (&n, &m, s);
      event_rest (decorators, n, m, 1);
      return;
    }

  /* read accidental */
  switch (**s)
    {
    case '_':
      accidental = **s;
      *s = *s + 1;
      if (**s == '_')
	{
	  *s = *s + 1;
	  mult = 2;
	};
      microtone = ismicrotone (s, -1);
      if (microtone)
	{
	  if (mult == 2)
	    mult = 1;
	  else
	    accidental = ' ';
	}
      break;
    case '^':
      accidental = **s;
      *s = *s + 1;
      if (**s == '^')
	{
	  *s = *s + 1;
	  mult = 2;
	};
      microtone = ismicrotone (s, 1);
      if (microtone)
	{
	  if (mult == 2)
	    mult = 1;
	  else
	    accidental = ' ';
	}

      break;
    case '=':
      accidental = **s;
      *s = *s + 1;
      /* if ((**s == '^') || (**s == '_')) {
         accidental = **s;
         }; */
      if (**s == '^')
	{
	  accidental = **s;
	  *s = *s + 1;
	  microtone = ismicrotone (s, 1);
	  if (microtone == 0)
	    accidental = '^';
	}
      else if (**s == '_')
	{
	  accidental = **s;
	  *s = *s + 1;
	  microtone = ismicrotone (s, -1);
	  if (microtone == 0)
	    accidental = '_';
	}
      break;
    default:
      microtone = ismicrotone (s, 1);		/* [SS] 2014-01-19 */
      break;
    };
  if ((**s >= 'a') && (**s <= 'g'))
    {
      note = **s;
      octave = 1;
      *s = *s + 1;
      while ((**s == '\'') || (**s == ','))
	{
	  if (**s == '\'')
	    {
	      octave = octave + 1;
	      *s = *s + 1;
	    };
	  if (**s == ',')
	    {
	      sprintf (msg, "Bad pitch specifier , after note %c", note);
	      event_error (msg);
	      octave = octave - 1;
	      *s = *s + 1;
	    };
	};
    }
  else
    {
      octave = 0;
      if ((**s >= 'A') && (**s <= 'G'))
	{
	  note = **s + 'a' - 'A';
	  *s = *s + 1;
	  while ((**s == '\'') || (**s == ','))
	    {
	      if (**s == ',')
		{
		  octave = octave - 1;
		  *s = *s + 1;
		};
	      if (**s == '\'')
		{
		  sprintf (msg, "Bad pitch specifier ' after note %c",
			   note + 'A' - 'a');
		  event_error (msg);
		  octave = octave + 1;
		  *s = *s + 1;
		};
	    };
	};
    };
  if (note == ' ')
    {
      event_error ("Malformed note : expecting a-g or A-G");
    }
  else
    {
      readlen (&n, &m, s);
      event_note (decorators, accidental, mult, note, octave, n, m);
      if (!microtone)
	event_normal_tone ();	/* [SS] 2014-01-09 */
    };
}

char *
getrep (p, out)
     char *p;
     char *out;
/* look for number or list following [ | or :| */
{
  char *q;
  int digits;
  int done;
  int count;

  q = p;
  count = 0;
  done = 0;
  digits = 0;
  while (!done)
    {
      if (isdigit (*q))
	{
	  out[count] = *q;
	  count = count + 1;
	  q = q + 1;
	  digits = digits + 1;
	  /* [SS] 2013-04-21 */
	  if (count > 50)
	    {
	      event_error ("malformed repeat");
	      break;
	    }
	}
      else
	{
	  if (((*q == '-') || (*q == ',')) && (digits > 0)
	      && (isdigit (*(q + 1))))
	    {
	      out[count] = *q;
	      count = count + 1;
	      q = q + 1;
	      digits = 0;
	      /* [SS] 2013-04-21 */
	      if (count > 50)
		{
		  event_error ("malformed repeat");
		  break;
		}
	    }
	  else
	    {
	      done = 1;
	    };
	};
    };
  out[count] = '\0';
  return (q);
}

int
checkend (s)
     char *s;
/* returns 1 if we are at the end of the line 0 otherwise */
/* used when we encounter '\' '*' or other special line end characters */
{
  char *p;
  int atend;

  p = s;
  skipspace (&p);
  if (*p == '\0')
    {
      atend = 1;
    }
  else
    {
      atend = 0;
    };
  return (atend);
}

void
readstr (out, in, limit)
     char out[];
     char **in;
     int limit;
/* copy across alpha string */
{
  int i;

  i = 0;
  while ((isalpha (**in)) && (i < limit - 1))
    {
      out[i] = **in;
      i = i + 1;
      *in = *in + 1;
    };
  out[i] = '\0';
}

/* [SS] 2015-06-01 required for parse_mididef() in store.c */
/* Just like readstr but also allows anything except white space */
int readaln (out, in, limit)
     char out[];
     char **in;
     int limit;
/* copy across alphanumeric string */
{
  int i;

  i = 0;
  while ((!isspace (**in)) && (**in) != 0 && (i < limit - 1))
    {
      out[i] = **in;
      i = i + 1;
      *in = *in + 1;
    };
  out[i] = '\0';
  return i;
}

void
parse_precomment (s)
     char *s;
/* handles a comment field */
{
  char package[40];
  char *p;
  int success;

  success = sscanf (s, "%%%%abc-version %3s", abcversion); /* [SS] 2014-08-11 */
  if (*s == '%')
    {
      p = s + 1;
      readstr (package, &p, 40);
      event_specific (package, p);
    }
  else
    {
      event_comment (s);
    };
}

/* [SS] 2017-12-10 */
FILE * parse_abc_include (s)
  char *s;
{
  char includefilename[80];
  FILE *includehandle;
  int success;
  success = sscanf (s, "%%%%abc-include %80s", includefilename); /* [SS] 2014-08-11 */
  if (success == 1) {
    /* printf("opening include file %s\n",includefilename); */
    includehandle = fopen(includefilename,"r");
    if (includehandle == NULL)
    {
      printf ("Failed to open include file %s\n", includefilename);
    };
    return includehandle;
    }
  return NULL; 
}


void
parse_tempo (place)
     char *place;
/* parse tempo descriptor i.e. Q: field */
{
  char *p;
  int a, b;
  int n;
  int relative;
  char *pre_string;
  char *post_string;

  relative = 0;
  p = place;
  pre_string = NULL;
  if (*p == '"')
    {
      p = p + 1;
      pre_string = p;
      while ((*p != '"') && (*p != '\0'))
	{
	  p = p + 1;
	};
      if (*p == '\0')
	{
	  event_error ("Missing closing double quote");
	}
      else
	{
	  *p = '\0';
	  p = p + 1;
	  place = p;
	};
    };
  while ((*p != '\0') && (*p != '='))
    p = p + 1;
  if (*p == '=')
    {
      p = place;
      skipspace (&p);
      if (((*p >= 'A') && (*p <= 'G')) || ((*p >= 'a') && (*p <= 'g')))
	{
	  relative = 1;
	  p = p + 1;
	};
      readlen (&a, &b, &p);
      skipspace (&p);
      if (*p != '=')
	{
	  event_error ("Expecting = in tempo");
	};
      p = p + 1;
    }
  else
    {
      a = 1;			/* [SS] 2013-01-27 */
      /*a = 0;  [SS] 2013-01-27 */
      b = 4;
      p = place;
    };
  skipspace (&p);
  n = readnump (&p);
  post_string = NULL;
  if (*p == '"')
    {
      p = p + 1;
      post_string = p;
      while ((*p != '"') && (*p != '\0'))
	{
	  p = p + 1;
	};
      if (*p == '\0')
	{
	  event_error ("Missing closing double quote");
	}
      else
	{
	  *p = '\0';
	  p = p + 1;
	};
    };
  event_tempo (n, a, b, relative, pre_string, post_string);
}

void appendfield(char *); /* links with store.c and yapstree.c */

void append_fieldcmd (key, s)  /* [SS] 2014-08-15 */
char key;
char *s;
{
appendfield(s);
} 

void
preparse_words (s)
     char *s;
/* takes a line of lyrics (w: field) and strips off */
/* any continuation character */
{
  int continuation;
  int l;

  /* printf("Parsing %s\n", s); */
  /* strip off any trailing spaces */
  l = strlen (s) - 1;
  while ((l >= 0) && (*(s + l) == ' '))
    {
      *(s + l) = '\0';
      l = l - 1;
    };
  if (*(s + l) != '\\')
    {
      continuation = 0;
    }
  else
    {
      /* [SS] 2014-08-14 */
      event_warning ("\\n continuation no longer supported in w: line");
      continuation = 1;
      /* remove continuation character */
      *(s + l) = '\0';
      l = l - 1;
      while ((l >= 0) && (*(s + l) == ' '))
	{
	  *(s + l) = '\0';
	  l = l - 1;
	};
    };
  event_words (s, continuation);
}

void
init_abbreviations ()
/* initialize mapping of H-Z to strings */
{
  int i;

  /* for (i = 0; i < 'Z' - 'H'; i++) [SS] 2016-09-25 */
  for (i = 0; i < 'z' - 'A'; i++) /* [SS] 2016-09-25 */
    {
      abbreviation[i] = NULL;
    };
}

void
record_abbreviation (char symbol, char *string)
/* update record of abbreviations when a U: field is encountered */
{
  int index;

  /* if ((symbol < 'H') || (symbol > 'Z')) [SS] 2016-09-20 */
     if ((symbol < 'A') || (symbol > 'z'))
    {
      return;
    };
  index = symbol - 'A';
  if (abbreviation[index] != NULL)
    {
      free (abbreviation[index]);
    };
  abbreviation[index] = addstring (string);
}

char *
lookup_abbreviation (char symbol)
/* return string which s abbreviates */
{
  /* if ((symbol < 'H') || (symbol > 'Z'))  [SS] 2016-09-25 */
  if ((symbol < 'A') || (symbol > 'z'))
    {
      return (NULL);
    }
  else
    {
      return (abbreviation[symbol - 'A']); /* [SS] 2016-09-20 */
    };
}

void
free_abbreviations ()
/* free up any space taken by abbreviations */
{
  int i;

  for (i = 0; i < SIZE_ABBREVIATIONS; i++)
    {
      if (abbreviation[i] != NULL)
	{
	  free (abbreviation[i]);
	};
    };
}

void
parsefield (key, field)
     char key;
     char *field;
/* top-level routine handling all lines containing a field */
{
  char *comment;
  char *place;
  char *xplace;
  int iscomment;
  int foundkey;

  if (key == 'X')
    {
      int x;

      xplace = field;
      skipspace (&xplace);
      x = readnumf (xplace);
      if (inhead)
	{
	  event_error ("second X: field in header");
	};
      event_refno (x);
      ignore_line =0; /* [SS] 2017-04-12 */
      init_voicecode ();	/* [SS] 2011-01-01 */
      inhead = 1;
      inbody = 0;
      parserinchord = 0;
      return;
    };

  if (parsing == 0)
    return;

  /*if ((inbody) && (strchr ("EIKLMPQTVdswW", key) == NULL)) [SS] 2014-08-15 */
  if ((inbody) && (strchr ("EIKLMPQTVdrswW+", key) == NULL)) /* [SS] 2015-05-11 */
    {
      event_error ("Field not allowed in tune body");
    };
  comment = field;
  iscomment = 0;
  while ((*comment != '\0') && (*comment != '%'))
    {
      comment = comment + 1;
    };
  if (*comment == '%')
    {
      iscomment = 1;
      *comment = '\0';
      comment = comment + 1;
    };
  place = field;
  skipspace (&place);
  switch (key)
    {
    case 'K':
      foundkey = parsekey (place);
      if (inhead || inbody)
	{
	  if (foundkey)
	    {
	      inbody = 1;
	      inhead = 0;
	    }
	  else
	    {
	      if (inhead)
		{
		  event_error ("First K: field must specify key signature");
		};
	    };
	}
      else
	{
	  event_error ("No X: field preceding K:");
	};
      break;
    case 'M':
      {
	int num, denom;

	/* strncpy (timesigstring, place, 16);   [SS] 2011-08-19 */
	snprintf(timesigstring,sizeof(timesigstring),"%s",place); /* [SEG] 2020-06-07 */
	if (strncmp (place, "none", 4) == 0)
        /* converts 'M: none'  to 'M: 4/4' otherwise a warning 
	 * is returned if not a fraction [SS] */
	  {
	    event_timesig (4, 4, 0);
	  }
	else
	  {
	    readsig (&num, &denom, &place);
	    if ((*place == 's') || (*place == 'l'))
	      {
		event_error ("s and l in M: field not supported");
	      };
	    if ((num != 0) && (denom != 0))
	      {
		/* [code contributed by Larry Myerscough 2015-11-5]
		 * Specify checkbars = 1 for numeric time signature
		 * or checkbars = 2 for 'common' time signature to
		 * remain faithful to style of input abc file.
		 */
		event_timesig (num, denom, 1 + ((*place == 'C') || (*place == 'c')));
	      };
	  };
	break;
      };
    case 'L':
      {
	int num, denom;

	readsig (&num, &denom, &place);
	if (num != 1)
	  {
	    event_error ("Default length must be 1/X");
	  }
	else
	  {
	    if (denom > 0)
	      {
		event_length (denom);
	      }
	    else
	      {
		event_error ("invalid denominator");
	      };
	  };
	break;
      };
    case 'P':
      event_part (place);
      break;
    case 'I':
      event_info (place);
      break;
    case 'V':
      parsevoice (place);
      break;
    case 'Q':
      parse_tempo (place);
      break;
    case 'U':
      {
	char symbol;
	char container;
	char *expansion;

	skipspace (&place);
	/* if ((*place >= 'H') && (*place <= 'Z')) [SS] 2016-09-20 */
	if ((*place >= 'A') && (*place <= 'z'))  /* [SS] 2016-09-20 */
	  {
	    symbol = *place;
	    place = place + 1;
	    skipspace (&place);
	    if (*place == '=')
	      {
		place = place + 1;
		skipspace (&place);
		if (*place == '!')
		  {
		    place = place + 1;
		    container = '!';
		    expansion = place;
		    while ((!iscntrl (*place)) && (*place != '!'))
		      {
			place = place + 1;
		      };
		    if (*place != '!')
		      {
			event_error ("No closing ! in U: field");
		      };
		    *place = '\0';
		  }
		else
		  {
		    container = ' ';
		    expansion = place;
		    while (isalnum (*place))
		      {
			place = place + 1;
		      };
		    *place = '\0';
		  };
		if (strlen (expansion) > 0)
		  {
		    record_abbreviation (symbol, expansion);
		    event_abbreviation (symbol, expansion, container);
		  }
		else
		  {
		    event_error ("Missing term in U: field");
		  };
	      }
	    else
	      {
		event_error ("Missing '=' U: field ignored");
	      };
	  }
	else
	  {
	    event_warning ("only 'H' - 'Z' supported in U: field");
	  };
      };
      break;
    case 'w':
      preparse_words (place);
      break;
    case 'd':
      /* decoration line in abcm2ps */
      event_field (key, place);	/* [SS] 2010-02-23 */
      break;
    case 's':
      event_field (key, place);	/* [SS] 2010-02-23 */
      break;
    case '+':
      if (lastfieldcmd == 'w') 
          append_fieldcmd (key, place); /*[SS] 2014-08-15 */
      break; /* [SS] 2014-09-07 */
    default:
      event_field (key, place);
    };
  if (iscomment)
    {
      parse_precomment (comment);
    };
  if (key == 'w') lastfieldcmd = 'w'; /* [SS] 2014-08-15 */
  else lastfieldcmd = ' ';  /* [SS[ 2014-08-15 */
}

char *
parseinlinefield (p)
     char *p;
/* parse field within abc line e.g. [K:G] */
{
  char *q;

  event_startinline ();
  q = p;
  while ((*q != ']') && (*q != '\0'))
    {
      q = q + 1;
    };
  if (*q == ']')
    {
      *q = '\0';
      parsefield (*p, p + 2);
      q = q + 1;
    }
  else
    {
      event_error ("missing closing ]");
      parsefield (*p, p + 2);
    };
  event_closeinline ();
  return (q);
}

/* this function is used by toabc.c [SS] 2011-06-10 */
void
print_inputline_nolinefeed ()
{
  if (inputline[sizeof inputline - 1] != '\0')
    {
      /*
       * We are called exclusively by toabc.c,
       * and when we are called, event_error is muted,
       * so, event_error("input line truncated") does nothing.
       * Simulate it with a plain printf. [PHDM 2012-12-01]
       */
      printf ("%%Error : input line truncated\n");
    }
  printf ("%s", inputline);
}

/* this function is used by toabc.c [SS] 2011-06-07 */
void
print_inputline ()
{
  print_inputline_nolinefeed ();
  printf ("\n");
}

void
parsemusic (field)
     char *field;
/* parse a line of abc notes */
{
  char *p;
  char c; /* [SS] 2017-04-19 */
  char *comment;
  char endchar;
  int iscomment;
  int starcount;
  int i;
  char playonrep_list[80];
  int decorators[DECSIZE];

  for (i = 0; i < DECSIZE; i++)
    decorators[i] = 0;		/* [SS] 2012-03-30 */

  event_startmusicline ();
  endchar = ' ';
  comment = field;
  iscomment = 0;
  while ((*comment != '\0') && (*comment != '%'))
    {
      comment = comment + 1;
    };
  if (*comment == '%')
    {
      iscomment = 1;
      *comment = '\0';
      comment = comment + 1;
    };

  p = field;
  skipspace (&p);
  while (*p != '\0')
    {
      lineposition = p - linestart;	/* [SS] 2011-07-18 */

      if (*p == '.' && *(p+1) == '(') {  /* [SS] 2015-04-28 dotted slur */
          p = p+1;
          event_sluron (1);
          p = p+1;
          }

      if (((*p >= 'a') && (*p <= 'g')) || ((*p >= 'A') && (*p <= 'G')) ||
	  (strchr ("_^=", *p) != NULL) || (strchr (decorations, *p) != NULL))
	{
	  parsenote (&p);
	}
      else
	{
          c = *p; /* [SS] 2017-04-19 */
	  switch (*p)
	    {
	    case '"':
	      {
		struct vstring gchord;

		p = p + 1;
		initvstring (&gchord);
		while ((*p != '"') && (*p != '\0'))
		  {
		    addch (*p, &gchord);
		    p = p + 1;
		  };
		if (*p == '\0')
		  {
		    event_error ("Guitar chord name not properly closed");
		  }
		else
		  {
		    p = p + 1;
		  };
		event_gchord (gchord.st);
		freevstring (&gchord);
		break;
	      };
	    case '|':
	      p = p + 1;
	      switch (*p)
		{
		case ':':
		  event_bar (BAR_REP, "");
		  p = p + 1;
		  break;
		case '|':
		  event_bar (DOUBLE_BAR, "");
		  p = p + 1;
		  break;
		case ']':
		  event_bar (THIN_THICK, "");
		  p = p + 1;
		  break;
		default:
		  p = getrep (p, playonrep_list);
		  event_bar (SINGLE_BAR, playonrep_list);
		};
	      break;
	    case ':':
	      p = p + 1;
	      switch (*p)
		{
		case ':':
		  event_bar (DOUBLE_REP, "");
		  p = p + 1;
		  break;
		case '|':
		  p = p + 1;
		  p = getrep (p, playonrep_list);
		  event_bar (REP_BAR, playonrep_list);
		  if (*p == ']')
		    p = p + 1;	/* [SS] 2013-10-31 */
		  break;
                /*  [JM] 2018-02-22 dotted bar line ... this is legal */
		default:
                /* [SS] 2018-02-08 introducing DOTTED_BAR */
		  event_bar (DOTTED_BAR,"");
		};
	      break;
	    case ' ':
	      event_space ();
	      skipspace (&p);
	      break;
	    case TAB:
	      event_space ();
	      skipspace (&p);
	      break;
	    case '(':
	      p = p + 1;
	      {
		int t, q, r;

		t = 0;
		q = 0;
		r = 0;
		t = readnump (&p);
		if ((t != 0) && (*p == ':'))
		  {
		    p = p + 1;
		    q = readnump (&p);
		    if (*p == ':')
		      {
			p = p + 1;
			r = readnump (&p);
		      };
		  };
		if (t == 0)
		  {
                    if (*p == '&') {
                       p = p+1;
                       event_start_extended_overlay(); /* [SS] 2015-03-23 */
                       }
                    else
		       event_sluron (1);
		  }
		else  /* t != 0 */
		  {
		    event_tuple (t, q, r);
		  };
	      };
	      break;
	    case ')':
	      p = p + 1;
	      event_sluroff (0);
	      break;
	    case '{':
	      p = p + 1;
	      event_graceon ();
	      ingrace = 1;
	      break;
	    case '}':
	      p = p + 1;
	      event_graceoff ();
	      ingrace = 0;
	      break;
	    case '[':
	      p = p + 1;
	      switch (*p)
		{
		case '|':
		  p = p + 1;
		  event_bar (THICK_THIN, "");
		  if (*p == ':') {  /* [SS] 2015-04-13 [SDG] 2020-06-04 */
		      event_bar (BAR_REP, "");
		      p = p + 1;
		      }
		  break;
		default:
		  if (isdigit (*p))
		    {
		      p = getrep (p, playonrep_list);
		      event_playonrep (playonrep_list);
		    }
		  else
		    {
		      if (isalpha (*p) && (*(p + 1) == ':'))
			{
			  p = parseinlinefield (p);
			}
		      else
			{
			  lineposition = p - linestart;	/* [SS] 2011-07-18 */
			  /* [SS] 2012-03-30 */
			  for (i = 0; i < DECSIZE; i++)
			    chorddecorators[i] =
			      decorators[i] | decorators_passback[i];
			  event_chordon (chorddecorators);
			  parserinchord = 1;
			};
		    };
		  break;
		};
	      break;
	    case ']':
	      p = p + 1;
	      /*readlen (&chord_n, &chord_m, &p); [SS] 2019-06-06 */
	      /*if (!inchordflag && *p == '|') {  [SS] 2018-12-21 not THICK_THIN  bar line*/
	      if (!parserinchord && *p == '|') { /* [SS] 2019-06-06 not THICK_THIN  bar line*/
                p = p + 1; /* skip over | */
	        event_bar (THIN_THICK, "");}
              else {
	        readlen (&chord_n, &chord_m, &p); /* [SS] 2019-06-06 */
	        event_chordoff (chord_n, chord_m);
	        parserinchord = 0;
	        }
	      for (i = 0; i < DECSIZE; i++)
		{
		  chorddecorators[i] = 0;
		  decorators_passback[i] = 0;	/* [SS] 2012-03-30 */
		}
	      break;
/*  hidden rest  */
	    case 'x':
	      {
		int n, m;

		p = p + 1;
		readlen (&n, &m, &p);
/* in order to handle a fermata applied to a rest we must
 * pass decorators to event_rest.
 */
		for (i = 0; i < DECSIZE; i++)
		  {
		    decorators[i] = decorators_passback[i];
		    decorators_passback[i] = 0;
		  }
		event_rest (decorators, n, m, 1);
                decorators[FERMATA] = 0;  /* [SS] 2014-11-17 */
		break;
	      };
/*  regular rest */
	    case 'z':
	      {
		int n, m;

		p = p + 1;
		readlen (&n, &m, &p);
/* in order to handle a fermata applied to a rest we must
 * pass decorators to event_rest.
 */
		for (i = 0; i < DECSIZE; i++)
		  {
		    decorators[i] = decorators_passback[i];
		    decorators_passback[i] = 0;
		  }
		event_rest (decorators, n, m, 0);
                decorators[FERMATA] = 0;  /* [SS] 2014-11-17 */
		break;
	      };
	    case 'y':		/* used by Barfly and abcm2ps to put space */
/* I'm sure I've seen somewhere that /something/ allows a length
 * specifier with y to enlarge the space length. Allow it anyway; it's
 * harmless.
 */
	      {
		int n, m;

		p = p + 1;
		readlen (&n, &m, &p);
		event_spacing (n, m);
		break;
	      };
/* full bar rest */
	    case 'Z':
	    case 'X':		/* [SS] 2012-11-15 */

	      {
		int n, m;

		p = p + 1;
		readlen (&n, &m, &p);
		if (m != 1)
		  {
		    event_error
		      ("X or Z must be followed by a whole integer");
		  };
		event_mrest (n, m, c);
                decorators[FERMATA] = 0;  /* [SS] 2014-11-17 */
		break;
	      };
	    case '>':
	      {
		int n;

		n = 0;
		while (*p == '>')
		  {
		    n = n + 1;
		    p = p + 1;
		  };
		if (n > 3)
		  {
		    event_error ("Too many >'s");
		  }
		else
		  {
		    event_broken (GT, n);
		  };
		break;
	      };
	    case '<':
	      {
		int n;

		n = 0;
		while (*p == '<')
		  {
		    n = n + 1;
		    p = p + 1;
		  };
		if (n > 3)
		  {
		    event_error ("Too many <'s");
		  }
		else
		  {
		    event_broken (LT, n);
		  };
		break;
	      };
	    case 's':
	      if (slur == 0)
		{
		  slur = 1;
		}
	      else
		{
		  slur = slur - 1;
		};
	      event_slur (slur);
	      p = p + 1;
	      break;
	    case '-':
	      event_tie ();
	      p = p + 1;
	      break;
	    case '\\':
	      p = p + 1;
	      if (checkend (p))
		{
		  event_lineend ('\\', 1);
		  endchar = '\\';
		}
	      else
		{
		  event_error ("'\\' in middle of line ignored");
		};
	      break;
	    case '+':
	      if (oldchordconvention)
		{
		  lineposition = p - linestart;	/* [SS] 2011-07-18 */
		  event_chord ();
		  parserinchord = 1 - parserinchord;
		  if (parserinchord == 0)
		    {
		      for (i = 0; i < DECSIZE; i++)
			chorddecorators[i] = 0;
		    };
		  p = p + 1;
		  break;
		}
	      /* otherwise we fall through into the next case statement */
	    case '!':
	      {
		struct vstring instruction;
		char *s;
		char endcode;

		endcode = *p;
		p = p + 1;
		s = p;
		initvstring (&instruction);
		while ((*p != endcode) && (*p != '\0'))
		  {
		    addch (*p, &instruction);
		    p = p + 1;
		  };
		if (*p != endcode)
		  {
		    p = s;
		    if (checkend (s))
		      {
			event_lineend ('!', 1);
			endchar = '!';
		      }
		    else
		      {
			event_error ("'!' or '+' in middle of line ignored");
		      };
		  }
		else
		  {
		    event_instruction (instruction.st);
		    p = p + 1;
		  };
		freevstring (&instruction);
	      }
	      break;
	    case '*':
	      p = p + 1;
	      starcount = 1;
	      while (*p == '*')
		{
		  p = p + 1;
		  starcount = starcount + 1;
		};
	      if (checkend (p))
		{
		  event_lineend ('*', starcount);
		  endchar = '*';
		}
	      else
		{
		  event_error ("*'s in middle of line ignored");
		};
	      break;
	    case '/':
	      p = p + 1;
	      if (ingrace)
		event_acciaccatura ();
	      else
		event_error ("stray / not in grace sequence");
	      break;
	    case '&':
	      p = p + 1;
              if (*p == ')') {
                 p = p + 1;
                 event_stop_extended_overlay(); /* [SS] 2015-03-23 */
                 break;
                 }
              else
	        event_split_voice ();
	      break;


	    default:
	      {
		char msg[40];

		if ((*p >= 'A') && (*p <= 'z')) /* [SS] 2016-09-20 */
		  {
		    event_reserved (*p);
		  }
		else if (fileprogram == ABC2MIDI && *p == '$') ; /* ignore [SS] 2019-12-9 */
	       	/* $ sometimes used as a score linebreak character */
		else
		  {
		    sprintf (msg, "Unrecognized character: %c", *p);
		    event_error (msg);
		  };
	      };
	      p = p + 1;
	    };
	};
    };
  event_endmusicline (endchar);
  if (iscomment)
    {
      parse_precomment (comment);
    };
}

void
parseline (line)
     char *line;
/* top-level routine for handling a line in abc file */
{
  char *p, *q;

  /* [SS] 2020-01-03 */
  if (strcmp(line,"%%begintext") == 0) {
	  ignore_line = 1;
          }
  if (strcmp(line,"%%endtext") == 0) {
	  ignore_line = 0;
          }
  if ((strncmp(line,"%%temperament",12) == 0) && fileprogram == ABC2MIDI) {
	  event_temperament(line);
          }

  handle_abc2midi_parser (line);  /* [SS] 2020-03-25 */
  if (ignore_line == 1 && fileprogram == ABC2MIDI) return;

  /* handle_abc2midi_parser (line);   [SS] 2017-04-12  2020-03-25 */
 if (ignore_line == 1) { /* [JM] 2018-02-22 */
        /* JM 20180219 Do a flush of the blocked lines in case of MidiOff
           and abc2abc */
        if (fileprogram == ABC2ABC)
                printf ("%s",line);
        return; /* [SS] 2017-04-12 */
        }


  /*printf("%d parsing : %s\n", lineno, line);*/ 
  strncpy (inputline, line, sizeof inputline);	/* [SS] 2011-06-07 [PHDM] 2012-11-27 */

  p = line;
  linestart = p;		/* [SS] 2011-07-18 */
  ingrace = 0;
  skipspace (&p);
  if (strlen (p) == 0)
    {
      event_blankline ();
      inhead = 0;
      inbody = 0;
      return;
    };
  if ((int) *p == '\\')
    {
      if (parsing)
	{
	  event_tex (p);
	};
      return;
    };
  if ((int) *p == '%')
    {
      parse_precomment (p + 1);
      if (!parsing)
	event_linebreak ();
      return;
    };
  /*if (strchr ("ABCDEFGHIKLMNOPQRSTUVdwsWXZ", *p) != NULL) [SS] 2014-08-15 */
  if (strchr ("ABCDEFGHIKLMNOPQRSTUVdwsWXZ+", *p) != NULL)
    {
      q = p + 1;
      skipspace (&q);
      if ((int) *q == ':')
	{
	  if (*(line + 1) != ':')
	    {
	      event_warning ("whitespace in field declaration");
	    };
	  if ((*(q + 1) == ':') || (*(q + 1) == '|'))
	    {
	      event_warning ("Potentially ambiguous line - either a :| repeat or a field command -- cannot distinguish.");
/*    [SS] 2013-03-20 */
/*     };             */
/*      parsefield(*p,q+1); */

/*    [SS} 2013-03-20 start */
/*    malformed field command try processing it as a music line */
	      if (inbody && *p != 'w') /* [SS] 2017-10-23 make exception for w: field*/
		{
		  if (parsing)
		    parsemusic (p);
		}
	      else if (inbody) preparse_words (p); /* [SS] 2017-10-23 */
	      else {
		  if (parsing)
		    event_text (p);
		};
	    }
	  else
	    parsefield (*p, q + 1);	/* not field command malformed */
/*    [SS] 2013-03-20  end */

	}
      else
	{
	  if (inbody)
	    {
	      if (parsing)
		parsemusic (p);
	    }
	  else
	    {
	      if (parsing)
		event_text (p);
	    };
	};
    }
  else
    {
      if (inbody)
	{
	  if (parsing)
	    parsemusic (p);
	}
      else
	{
	  if (parsing)
	    event_text (p);
	};
    };
}

void
parsefile (name)
     char *name;
/* top-level routine for parsing file */
/* [SS] 2017-12-10 In order to allow including the directive
   "%%abc-include includefile.abc" to insert the includedfile.abc,
   we switch the file handle fp to link to the includefile.abc and switch
   back to the original file handle when we reach the end of file
   of includefile.abc. Thus we keep track of the original handle
   using fp_last.
*/
{
  FILE *fp;
  FILE *fp_last,*fp_include; /* [SS] 2017-12-10 */
  int reading;
  int fileline;
  int last_position = 0; /* [SDG] 2020-06-03 */
  struct vstring line;
  /* char line[MAXLINE]; */
  int t;
  int lastch, done_eol;
  int err;

  fp_last = NULL; /* [SS] 2017-12-10 */

  /* printf("parsefile called %s\n", name); */
  /* The following code permits abc2midi to read abc from stdin */
  if ((strcmp (name, "stdin") == 0) || (strcmp (name, "-") == 0))
    {
      fp = stdin;
    }
  else
    {
      fp = fopen (name, "r");
    };
  if (fp == NULL)
    {
      printf ("Failed to open file %s\n", name);
      exit (1);
    };
  inhead = 0;
  inbody = 0;
  parseroff ();
  reading = 1;
  line.limit = 4;
  initvstring (&line);
  fileline = 1;
  done_eol = 0;
  lastch = '\0';
  while (reading)
    {
      t = getc (fp);
      if (t == EOF)
	{
	  reading = 0;
	  if (line.len > 0)
	    {
	      parseline (line.st);
	      fileline = fileline + 1;
	      lineno = fileline;
	      if (parsing)
		event_linebreak ();
	    };
          if (fp_last != NULL) {  /* [SS] 2017-12-10 */
            fclose(fp);
            fp = fp_last;
            err = fseek(fp,last_position,SEEK_SET);
            /*printf("fseek return err = %d\n",err);*/
            reading = 1;
            fp_last = NULL;
            }
	}
      else
	{
	  /* recognize  \n  or  \r  or  \r\n  or  \n\r  as end of line */
	  /* should work for DOS, unix and Mac files */
	  if ((t != '\n') && (t != '\r'))
	    {
	      addch ((char) t, &line);
	      done_eol = 0;
	    }
	  else
	    {
	      if ((done_eol) && (((t == '\n') && (lastch == '\r')) ||
				 ((t == '\r') && (lastch == '\n'))))
		{
		  done_eol = 0;
		  /* skip this character */
		}
	      else
		{
                 /* reached end of line */
                 fp_include = parse_abc_include (line.st);/* [SS] 2017-12-10 */
                 if (fp_include == NULL) { 
		    parseline (line.st);
		    clearvstring (&line);
                    if (fp_last == NULL) {
		      fileline = fileline + 1;
		      lineno = fileline;
                      }
		    if (parsing)
		      event_linebreak ();
		    done_eol = 1;
                    } else {
                    if (fp_last == NULL) {
                      last_position = ftell(fp);
                      /*printf("last position = %d\n",last_position);*/
                      fp_last = fp;
                      fp = fp_include;
		      if (parsing)
		        event_linebreak ();
		      done_eol = 1;
		      clearvstring (&line);
                      } else {
                      event_error ("Not allowed to recurse include file");
                      }
                    }
		};
	    };
	  lastch = t;
	};
    };
  fclose (fp);
  event_eof ();
  freevstring (&line);
  if (parsing_started == 0)
    event_error ("No tune processed. Possible missing X: field");
}


int
parsetune (FILE * fp)
/* top-level routine for parsing file */
{
  struct vstring line;
  /* char line[MAXLINE]; */
  int t;
  int lastch, done_eol;

  inhead = 0;
  inbody = 0;
  parseroff ();
  intune = 1;
  line.limit = 4;
  initvstring (&line);
  done_eol = 0;
  lastch = '\0';
  do
    {
      t = getc (fp);
      if (t == EOF)
	{
	  if (line.len > 0)
	    {
	      printf ("%s\n", line.st);
	      parseline (line.st);
	      fileline_number = fileline_number + 1;
	      lineno = fileline_number;
	      event_linebreak ();
	    };
	  break;
	}
      else
	{
	  /* recognize  \n  or  \r  or  \r\n  or  \n\r  as end of line */
	  /* should work for DOS, unix and Mac files */
	  if ((t != '\n') && (t != '\r'))
	    {
	      addch ((char) t, &line);
	      done_eol = 0;
	    }
	  else
	    {
	      if ((done_eol) && (((t == '\n') && (lastch == '\r')) ||
				 ((t == '\r') && (lastch == '\n'))))
		{
		  done_eol = 0;
		  /* skip this character */
		}
	      else
		{
		  /* reached end of line */
		  parseline (line.st);
		  clearvstring (&line);
		  fileline_number = fileline_number + 1;
		  lineno = fileline_number;
		  event_linebreak ();
		  done_eol = 1;
		};
	    };
	  lastch = t;
	};
    }
  while (intune);
  freevstring (&line);
  return t;
}

/*
int getline ()
{
  return (lineno);
}
*/

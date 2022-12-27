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
#include "music_utils.h"
#include <stdio.h>
#include <stdlib.h>
/* [JM] 2018-02-22 to handle strncasecmp() */
#include <string.h>
#include <limits.h>

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
#define strcasecmp _stricmp
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

int note2midi (char** s, char* word); /*[SS] 2022-12-27 added word */
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

int num_voices = 0;  /* [JA] 2020-10-12 */
int repcheck = 1; /* enables/ disables repeat checking */
/* abc2midi disables repeat checking because it does its own thing */
voice_context_t voicecode[MAX_VOICES];
timesig_details_t master_timesig;  /* [JA] 2020-12-10 */
cleftype_t master_clef;
int has_timesig;
int master_unitlen; /* L: field value is 1/unitlen */
int voicenum; /* current voice number */
int has_voice_fields = 0;

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

/* tables mode and modeshift moved to music_utils.c */

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
#ifdef NO_SNPRINTF
   sprintf(p, "%s%s",s1,s2); /* [SS] 2020-11-01 */
#else
   snprintf(p,len, "%s%s",s1,s2);
#endif
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
/* [JA] 2021-05-25 */
  while (((int) *p >= '0') && ((int) *p <= '9') && (t < (INT_MAX-9)/10))
    {
      t = t * 10 + (int) *p - '0';
      p = p + 1;
    };
  if (t >= (INT_MAX-9)/10) {  /* [JA] 2021-05-25 */
    event_error ("Number too big");
  }
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
  /* [JA] 2021-05-25 */
  while (((int) **p >= '0') && ((int) **p <= '9') && (t < (INT_MAX-9)/10))
  {
    t = t * 10 + (int) **p - '0';
    *p = *p + 1;
  }
  /* advance over any spurious extra digits [JA] 2021-05-25 */
  while (isdigit(**p)) {
    *p = *p + 1;
  }
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

/* [JA] 2020-12-10 */
int check_power_of_two(int denom)
{
  int t;
  char error_message[80];

  t = denom; 
  while (t > 1) {
    if (t % 2 != 0) {
      snprintf(error_message, 80, "%d b is not a power of 2", denom);
      event_error (error_message);
      return -1;
    } else {
      t = t / 2;
    }
  }
  return denom;
}
/* [JA] 2020-12-10 */
/* read the numerator of a time signature in M: field
 *
 * abc standard 2.2 allows M:(a + b + c + ...)/d
 * This indicates how note lenths within a bar are to be grouped.
 * abc standard also allows a+b+c/d to mean the same thing but this 
 * goes against the convention that division takes precendence
 * over addition i.e. a+b+c/d normally means a + b + (c/d).
 */
static int read_complex_has_timesig(char **place, timesig_details_t *timesig)
{
  int n;
  int total;
  int count;
  int has_bracket = 0;

  if (**place == '(') {
    *place = *place + 1;
    has_bracket = 1;
    skipspace(place);
  }
  count = 0;
  total = 0;
  skipspace(place);
  while ((**place != '\0') && (isdigit(**place))) {
    n = readnump(place);
    timesig->complex_values[count] = n;
    total = total + n;
    count = count + 1;
    if (count > 8) {
      event_error("Too many parts to complex time (maximum 8)");
      return 0;
    }
    skipspace(place);
    if (**place == '+') {
      *place = *place + 1;
      skipspace(place);
    }
  }
  if (**place == ')') {
    *place = *place + 1; /* advance over ')' */
    skipspace(place);
    if (!has_bracket) {
      event_warning("Missing ( in complex time");
    }
    has_bracket = 0;
  }
  if (has_bracket) {
    event_warning("Missing ) in complex time");
  }
  /* we have reached the end of the numerator */
  timesig->num_values = count;
  timesig->num = total;
  if (timesig->num_values == 1)
  {
    timesig->type = TIMESIG_NORMAL;
  } else {
    timesig->type = TIMESIG_COMPLEX;
  }
  return 1;
}

/* read time signature (meter) from M: field */
void readsig (char **sig, timesig_details_t *timesig)
/* upgraded [JA] 2020-12-10 */
{
  int valid_num;

  if ((strncmp (*sig, "none", 4) == 0) ||
      (strncmp (*sig, "None", 4) == 0)) {
    timesig->num = 4;
    timesig->denom = 4;
    timesig->type = TIMESIG_FREE_METER;
    return;
  }
  /* [SS] 2012-08-08  cut time (C| or c|) is 2/2 not 4/4 */
  if (((**sig == 'C') || (**sig == 'c')) &&
      (*(*sig + 1) == '|')) {
    timesig->num = 2;
    timesig->denom = 2;
    timesig->type = TIMESIG_CUT;
    return;
  }
  if ((**sig == 'C') || (**sig == 'c')) {
    timesig->num = 4;
    timesig->denom = 4;
    timesig->type = TIMESIG_COMMON;
    return;
  }
  valid_num = read_complex_has_timesig(sig, timesig);
  if (!valid_num) {
    /* An error message will have been generated by read_complex_has_timesig */
    timesig->num = 4;
    timesig->denom = 4;
    timesig->type = TIMESIG_FREE_METER;
    return;
  }

  if ((int)**sig != '/') {
    event_warning ("No / found, assuming denominator of 1");
    timesig->denom = 1;
  } else {
    *sig = *sig + 1;
    skipspace(sig);
    if (!isdigit(**sig)) {
      event_warning ("Number not found for M: denominator");
    }
    timesig->denom = readnump (sig);
  }
  if ((timesig->num == 0) || (timesig->denom == 0)) {
    event_error ("Expecting fraction in form A/B");
  } else {
    timesig->denom =  check_power_of_two(timesig->denom);
  }
}


void readlen (int *a, int *b, char **p)
/* read length part of a note and advance character pointer */
{
  int t;

  *a = readnump (p);
  if (*a == 0) {
    *a = 1;
  }
  *b = 1;
  if (**p == '/') {
    *p = *p + 1;
    *b = readnump (p);
    if (*b == 0) {
      *b = 2;
      /* [JA] 2021-05-19 prevent infinite loop */
      /* limit the number of '/'s we support */
      while ((**p == '/') && (*b < 1024)) {
        *b = *b * 2;
        *p = *p + 1;
      }
      if (*b >= 1024) {
        event_warning ("Exceeded maximum note denominator");
      }
    }
  }
  *b = check_power_of_two(*b);
}


/* [JA] 2020-12-10 */
static void read_L_unitlen(int *num, int *denom, char **place)
{
  if (!isdigit(**place)) {
    event_warning("No digit at the start of L: field");
  }
  *num = readnump (place);
  if (*num == 0) {
    *num = 1;
  }
  if ((int)**place != '/') {
    event_error ("Missing / ");
    *denom = 1;
  } else {
    *place = *place + 1;
    skipspace(place);
    *denom = readnump (place);
  }
  if ((*num == 0) || (*denom == 0)) {
    event_error ("Expecting fraction in form A/B");
  } else {
    *denom =  check_power_of_two(*denom);
  }
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

/* part of K: parsing - looks for a clef in K: field                 */
/* format is K:string where string is treble, bass, baritone, tenor, */
/* alto, mezzo, soprano or K:clef=arbitrary                          */
/* revised by James Allwright  [JA] 2020-10-18 */
int isclef (char *s, cleftype_t * new_clef,
            int *gotoctave, int *octave, int expect_clef)
{
  int gotclef;

  gotclef = 0;
  new_clef->octave_offset = 0;
  gotclef = get_standard_clef (s, new_clef);
  if (!gotclef && expect_clef) {
    /* do we have a clef in letter format ? e.g. C1, F3, G3 */
    gotclef = get_extended_clef_details (s, new_clef);
    if (new_clef->basic_clef == basic_clef_none) {
      event_warning ("Found clef=none, but a clef is required. Ignoring");
      gotclef = 0;
    }
  }
  if (expect_clef && !gotclef) {
    char error_message[80];
    
#ifdef NO_SNPRINTF
    sprintf (error_message, "clef %s not recognized", s);
#else
    snprintf (error_message, 80, "clef %s not recognized", s);
#endif
    event_warning (error_message);
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

/* initialize a timesig structure to default values */
void init_timesig(timesig_details_t *timesig)
{
  timesig->type = TIMESIG_FREE_METER;
  timesig->num = 4;
  timesig->denom = 4;
  timesig->complex_values[0] = 4;
  timesig->num_values = 1;
}

/* [JA] 2020-10-12 */
void init_voice_contexts (void)
{
  int i;
  cleftype_t default_clef;  /* [JA] 2020-11-01 */

  /* we use treble clef when no clef is explicitly specified */
  get_standard_clef ("treble", &default_clef); /* default to treble clef */
  for (i = 0; i < MAX_VOICES; i++) {      /* [SS} 2015-03-15 */
    voicecode[i].label[0] = '\0';
    voicecode[i].expect_repeat = 0;
    voicecode[i].repeat_count = 0;
    copy_clef(&voicecode[i].clef, &default_clef); /* [JA] 2020-11-01 */
  }
}

/* copy one timesig_details_t struct to another  [JA] 2020-12-10 */
void copy_timesig(timesig_details_t *destination, timesig_details_t *source)
{
  int i;

  destination->type = source->type;
  destination->num = source->num;
  destination->denom = source->denom;
  for (i = 0; i < 8; i++)
  {
    destination->complex_values[i] = source->complex_values[i];
  }
  destination->num_values = source->num_values;
}

/* [JA] 2020-10-12 */
/* called at the start of each tune */
static void reset_parser_status (void)
{
  cleftype_t default_clef;

  init_timesig(&master_timesig);
  get_standard_clef ("treble", &master_clef); /* default to treble clef */
  has_timesig = 0;
  master_unitlen = -1;
  voicenum = 1;
  has_voice_fields = 0;
  num_voices = 1;
  parserinchord = 0;
  ingrace = 0;
  slur = 0;
  init_voice_contexts ();
}


void
print_voicecodes ()
{
  int i;
  if (num_voices == 0)
    return;
  printf ("voice mapping:\n");
  for (i = 0; i < num_voices; i++)
    {
      if (i % 4 == 3)
	printf ("\n");
      printf ("%s  %d   ", voicecode[i].label, i + 1);
    }
  printf ("\n");
}

/* [JA] 2020-10-12 */
int interpret_voice_label (char *s, int num, int *is_new)
/* We expect a numeric value indicating the voice number.
 * The assumption is that these will ocuur in the order in which voices
 * appear, so that we have V:1, V:2, ... V:N if there are N voices.
 * The abc standard 2.2 allows strings instead of these numbers to
 * represent voices.
 * This function should be called with either
 * an empty string and a valid num or
 * a valid string and num set to 0.
 *
 * If num is non-zero, we check that it is valid and return it.
 * If the number is one above the number of voices currently in use,
 * we allocate a new voice.
 *
 * If num is zero and the string is not empty, we check if string
 * has been assigned to one of the existing voices. If not, we
 * allocate a new voice and assign the string to it.
 *
 * If we exceed MAX_VOICES voices, we report an error.
 *
 * we return a voice number in the range 1 - num_voices
*/
{
  int i;
  char code[32];
  char msg[80];                 /* [PHDM] 2012-11-22 */
  char *c;
  c = readword (code, s);

  if (num > 0)
  {
    if (num > num_voices + 1)
    {
      char error_message[80];

#ifdef NO_SNPRINT 
      sprintf(error_message, "V:%d out of sequence, treating as V:%d",
               num, num_voices); /* [SS] 2020-10-01 */
#else
      snprintf(error_message, 80, "V:%d out of sequence, treating as V:%d",
               num, num_voices);
#endif
      event_warning(error_message);
      num = num_voices + 1;
    }
    /* declaring a new voice */
    if (num == num_voices + 1)
    {
      *is_new = 1;
      if (num_voices >= MAX_VOICES)
      {
        event_warning("Number of available voices exceeded");
        return 1;
      }
      num_voices = num_voices + 1;
      voicecode[num_voices - 1].label[0] = '\0';
    } else {
      /* we are using a previously declared voice */
      *is_new = 0;
    }
    return num;
  }
/* [PHDM] 2012-11-22 */
  if (*c != '\0' && *c != ' ' && *c != ']') {
    sprintf (msg, "invalid character `%c' in Voice ID", *c);
    event_error (msg);
  }
/* [PHDM] 2012-11-22 */

  if (code[0] == '\0')
  {
    event_warning("Empty V: field, treating as V:1");
    return 1;
  }
  /* Has supplied label been used for one of the existing voices ? */
  if (has_voice_fields)
  {
    for (i = 0; i < num_voices; i++) {
      if (strcmp (code, voicecode[i].label) == 0) {
        return i + 1;
      }
    }
  }
  /* if we have got here, we have a new voice */
  if ((num_voices + 1) > MAX_VOICES) {/* [SS] 2015-03-16 */
    event_warning("Number of available voices exceeded");
    return 1;
  }
  /* First V: field is a special case. We are already on voice 1,
   * so we don't increment the number of voices, but we will set
   * status->has_voice_fields on returning from this function.
   */
  if (has_voice_fields)
  {
    *is_new = 1;
    num_voices++;
  } else {
    *is_new = 0; /* we have already started as voice 1 */
  }
  strncpy (voicecode[num_voices - 1].label, code, 31);
  return num_voices;
}

/* The following four functions parseclefs, parsetranspose,
 * parsesound, parseoctave are used to parse the K: field which not
 * only specifies the key signature but also other descriptors
 * used for producing a midi file or postscript file.
 *
 * The char* word contains the particular token that
 * is being interpreted. If the token can be understood,
 * other parameters are extracted from char ** s and
 * s is advanced to point to the next token.
 */


int
parseclef (s, word, gotclef, clefstr, newclef, gotoctave, octave)
     char **s;
     char *word;
     int *gotclef;
     char *clefstr;  /* [JA] 2020-10-19 */
     cleftype_t * newclef;
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
	  if (isclef (clefstr, newclef, gotoctave, octave, 1))
	    {
	      *gotclef = 1;
	    };
	};
      successful = 1;
    }
  else if (isclef (word, newclef, gotoctave, octave, 0))
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

/* [SS] 2021-10-11 */
int
parsesound (s, word, gottranspose, transpose)
/* parses string sound = 
                 shift =
                 instrument = note1note2 or note1/note2
  for parsekey() or parsevoice()
  and returns the transpose value
*/
     char **s;
     char *word;
     int *gottranspose;
     int *transpose;
  {   
  int p1,p2;
  if (casecmp(word,"sound") != 0
     && casecmp(word,"shift") != 0
     && casecmp(word,"instrument") != 0
     )
    return 0;
  skipspace (s);
  if (**s != '=')
    {
      event_error ("sound must be followed by '='");
     return 0;
     } else {
      *s = *s + 1;
      skipspace (s);
      p1 = note2midi (s,word);
      /*printf("p1 midi note = %d\n",p1);*/ 
      p2 = note2midi (s,word);
      /*printf("p2 midi note = %d\n",p2);*/ 

      if (p2 == p1) {
          p2 = 72;  /* [SS] 2022.12.21 */
          } 
      if (casecmp(word,"instrument") == 0) { /*2022.12.27 */
          *transpose = p1 - p2; /* [SS] 2022.02.18 2022.04.27 */
       } else {
          *transpose = p2 - p1; /* [SS] 2022.02.18 2022.04.27 */
       }
       *gottranspose = 1;
     }
  return 1;
  }

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

/* Function added JA 20 May 2022*/
/* parse a string contained in quotes.
 * strings may contain the close quote character because
 * x umlaut is encoded as \"x, which complicates parsing.
 * this is an unfortunate feature of the abc syntax.
 *
 * on entry, start points to the opening double quote.
 * returns pointer to last character before closing quote.
 */
char *umlaut_get_buffer(char *start, char *buffer, int bufferlen)
{
  char *p;
  int last_ch;
  int i;

  p = start;
  i = 0;
  while ((*p != '\0') &&
         !((*p == '"') && (last_ch != '\\'))) {
    if (i < bufferlen - 2) {
      buffer[i] = *p;
      i = i + 1;
    }
    last_ch = *p;
    p = p + 1;
  }
  buffer[i] = '\0';
  return p;
}

/* Function added JA 20 May 2022*/
/* construct string using vstring */
char *umlaut_build_string(char *start, struct vstring *gchord)
{
  int lastch = ' ';
  char *p;

  p = start;
  //initvstring (&gchord);
  /* need to allow umlaut sequence in guitar chords.
   * e.g. \"a, \"u
   */
  while (!((*p == '\0') ||
          ((*p == '"') && (lastch != '\\')))) {
    addch (*p, gchord);
    lastch = *p;
    p = p + 1;
  }
  //printf("umlaut_build_string has >%s<\n", gchord->st);
  return p;
}

/* Function modified JA 20 May 2022 */
int
parsename (s, gotname, namestring, maxsize)
/* parses string name= "string" in V: command 
   for compatability of abc2abc with abcm2ps
*/
     char **s;
     int *gotname;
     char namestring[];
     int maxsize;
{
  int i;

  i = 0;
  skipspace (s);
  if (**s != '=') {
    event_error ("name must be followed by '='");
  } else {
    *s = *s + 1;
    skipspace (s);
    if (**s == '"') {           /* string enclosed in double quotes */
      namestring[i] = (char)**s;
      *s = *s + 1;
      *s = umlaut_get_buffer(*s, &namestring[1], maxsize-1);
      *s = *s + 1; /* skip over closing double quote */
      strcat(namestring, "\"");
    } else {                    /* string not enclosed in double quotes */
      while (i < maxsize && **s != ' ' && **s != '\0') {
        namestring[i] = (char)**s;
        *s = *s + 1;
        i++;
      }
      namestring[i] = '\0';
    }
    *gotname = 1;
  }
  return 1;
}

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
        if (j > 7) {
          j = (int) c - 'a';
        }
        if (j > 7 || j < 0) {
          event_error ("Not a valid microtone");
          return;
        }
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
      if (j > 7) {
        j = (int) c - 'a';
      }
      if (j > 7 || j < 0) {
        event_error ("Not a valid microtone");
        return;
      }
      if (word[0] == '_') a = -a;
      modmap[j] = word[0];
	    modmicrotone[j].num = a;
	    modmicrotone[j].denom = b;
	    /* printf("%c microtone = %d/%d\n",modmap[j],modmicrotone[j].num,modmicrotone[j].denom); */
	  }
} /* finished ^ = _ */

static void set_voice_from_master(int voice_num)
{
  voice_context_t *current_voice;

  current_voice = &voicecode[voice_num - 1];
  copy_timesig(&current_voice->timesig, &master_timesig);
  copy_clef(&current_voice->clef, &master_clef);
  current_voice->unitlen = master_unitlen;
}

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
  cleftype_t newclef;
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
  init_new_clef (&newclef);
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
      parsed = parseclef (&s, word, &gotclef, clefstr, &newclef, &cgotoctave, &coctave);
      if (parsed) {  /* [JA] 2021-05-21 changed (gotclef) to (parsed) */
        /* make clef an attribute of current voice */
        if (inhead) {
          copy_clef (&master_clef, &newclef);
        }
        if (inbody){
          copy_clef (&voicecode[voicenum - 1].clef, &newclef);
        }
      }
      /* parseclef also scans the s string using readword(), placing */
      /* the next token  into the char array word[].                   */
      if (!parsed)
	parsed = parsetranspose (&s, word, &gottranspose, &transpose);

      if (!parsed)
	parsed = parseoctave (&s, word, &gotoctave, &octave);

      if (!parsed)
        parsed = parsesound (&s, word, &gottranspose, &transpose);

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
	     gotclef, clefstr, &newclef, octave, transpose, gotoctave, gottranspose,
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
  int is_new = 0;

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
  num = 0;
  if (isdigit(*s)) {  /* [JA] 2020-10-12 */
    num = readnump (&s);
  }
  num = interpret_voice_label (s, num, &is_new);
  if (is_new) {
    /* declaring voice for the first time.
     * initialize with values set in the tune header */
    set_voice_from_master(num);
  }
  has_voice_fields = 1;
  skiptospace (&s);
  voicenum = num;
  skipspace (&s);
  while (*s != '\0') {
    parsed =
      parseclef (&s, word, &vparams.gotclef, vparams.clefname,
                 &vparams.new_clef, &cgotoctave, &coctave);
      /* printf("vparams.clefname =%s\n",vparams.clefname); */
      /* do not create a tablature voice [SS] 2022.07.31 */
      if (strcmp(vparams.clefname,"tab") == 0) return; 
      if (vparams.gotclef) {
        /* make clef an attribute of current voice */
        copy_clef (&voicecode[num - 1].clef, &vparams.new_clef);
      }
      if (!parsed)
        parsed =
          parsetranspose (&s, word, &vparams.gottranspose,
            &vparams.transpose);
      if (!parsed)
        parsed = parseoctave (&s, word, &vparams.gotoctave, &vparams.octave);
      if (!parsed)
        parsed = parsesound (&s, word, &vparams.gottranspose, &vparams.transpose);
      /* Code changed JA 20 May 2022 */
      if ((!parsed) && (strcasecmp (word, "name") == 0)) {
        parsed =
          parsename (&s, &vparams.gotname, vparams.namestring,
            V_STRLEN);
      }
      if ((!parsed) && (strcasecmp (word, "sname") == 0)) {
        parsed =
          parsename (&s, &vparams.gotsname, vparams.snamestring,
            V_STRLEN);
      }
      if (!parsed)
        parsed =
          parsemiddle (&s, word, &vparams.gotmiddle, vparams.middlestring,
            V_STRLEN);
      if (!parsed)
        parsed = parseother (&s, word, &vparams.gotother, vparams.other, V_STRLEN);  /* [SS] 2011-04-18 */
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
      event_note (decorators, &voicecode[voicenum - 1].clef, accidental, mult, note, octave, n, m);
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
  success = sscanf (s, "%%%%abc-include %79s", includefilename); /* [SS] 2014-08-11 */
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

/* Function mofied for umlaut handling JA 20 May 2022 */
void
parse_tempo (place)
     char *place;
/* parse tempo descriptor i.e. Q: field */
{
  char *p;
  char *after_pre;
  int a, b;
  int n;
  int relative;
  char *pre_string;
  char *post_string;
  struct vstring pre;
  struct vstring post;

  relative = 0;
  p = place;
  pre_string = NULL;
  initvstring (&pre);
  if (*p == '"') {
    p = p + 1; /* skip over opening double quote */
    p = umlaut_build_string(p, &pre);
    pre_string = pre.st;
    if (*p == '\0') {
      event_error ("Missing closing double quote");
    } else {
      p = p + 1; /* skip over closing double quote */
      place = p;
    }
  }
  after_pre = p;
  /* do we have an "=" ? */
  while ((*p != '\0') && (*p != '='))
    p = p + 1;
  if (*p == '=') {
    p = place;
    skipspace (&p);
    if (((*p >= 'A') && (*p <= 'G')) || ((*p >= 'a') && (*p <= 'g'))) {
      relative = 1;
      p = p + 1;
    }
    readlen (&a, &b, &p);
    skipspace (&p);
    if (*p != '=') {
      event_error ("Expecting = in tempo");
    }
    p = p + 1;
  } else {
    /* no "=" found - default to 1/4 note */
    a = 1;                      /* [SS] 2013-01-27 */
    b = 4;
    p = after_pre;
  }
  skipspace (&p);
  n = readnump (&p); /* read notes per minute value */
  post_string = NULL;
  initvstring (&post);
  skipspace (&p);
  if (*p == '"') {
    p = p + 1; /* skip over opening double quote */
    p = umlaut_build_string(p, &post);
    post_string = post.st;
    if (*p == '\0') {
      event_error ("Missing closing double quote");
    } else {
      p = p + 1; /* skip over closing double quote */
    }
  }
  event_tempo (n, a, b, relative, pre_string, post_string);
  freevstring (&pre);
  freevstring (&post);
}

void appendfield(char *); /* links with store.c and yapstree.c */

void append_fieldcmd (key, s)  /* [SS] 2014-08-15 */
char key;
char *s;
{
appendfield(s);
}

/* [JA] 2022-06-14 */
/* Second level of processing for a line of lyrics, takes either
 * w: with any + at the start removed or +: .
 * It then strips off any continuation character
 * append is set to
 *   PLUS_FIELD for +:
 *   W_PLUS_FIELD for w: + <lyrics>
 *   0 for regular w: <lyrics>
 */
void preparse2_words(char *field, int append)
{
  int continuation;
  int l;
  char *s;

  s = field;
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
  event_words (s, append, continuation);
}

/* first level of w: field processing
 * takes a line of lyrics (w: field)  and handles
 * any + after w:
 */
void preparse1_words (char *field)
{
  int append;
  char *s;

  /* look for '+' at the start of word field */
  s = field;
  append = 0;
  skipspace(&s);
  if (*s == '+') {
    append = W_PLUS_FIELD;
    s = s + 1;
    skipspace(&s);
  }
  preparse2_words(s, append);
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

/* function to resolve unit note length when the
 * L: field is missing in the header [JA] 2020-12-10
 *
 * From the abc standard 2.2:
 * If there is no L: field defined, a unit note length is set by default,
 * based on the meter field M:. This default is calculated by computing
 * the meter as a decimal: if it is less than 0.75 the default unit note
 * length is a sixteenth note; if it is 0.75 or greater, it is an eighth
 * note. For example, 2/4 = 0.5, so, the default unit note length is a
 * sixteenth note, while for 4/4 = 1.0, or 6/8 = 0.75, or 3/4= 0.75,
 * it is an eighth note. For M:C (4/4), M:C| (2/2) and M:none (free meter),
 * the default unit note length is 1/8.
 *
 */
static void resolve_unitlen()
{
  if (master_unitlen == -1)
  {
    if (has_timesig == 0)
    {
      event_default_length(8);
      master_unitlen = 8;
    }
    else
    {
      if (((4 * master_timesig.num)/master_timesig.denom) >= 3)
      {
        event_default_length(8);
        master_unitlen = 8;
      }
      else
      {
        event_default_length(16);
        master_unitlen = 16;
      }
    }
  }
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
      if (inbody)
      {
       /* [JA] 2020-10-14 */
        event_error ("Missing blank line before new tune");
      }
      event_refno (x);
      ignore_line =0; /* [SS] 2017-04-12 */
      reset_parser_status(); /* [JA] 2020-10-12 */
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
      if (inhead)
      {
        /* First K: is the end of the header and the start of the body.
         * make sure we set up default for unit length
         * if L: fields was missing in the header.
         */
        resolve_unitlen();
      }
      foundkey = parsekey (place); /* [JA] 2021.05.21 parsekey called before set_voice_from_master(1) */
      if (inhead) {
        /* set voice parameters using values from header */
        set_voice_from_master(1);
      }
      if (inhead || inbody) {
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
        timesig_details_t timesig;

	/* strncpy (timesigstring, place, 16);   [SS] 2011-08-19 */
#ifdef NO_SNPRINT 
	sprintf(timesigstring,"%s",place); /* [SEG] 2020-06-07 */
#else
	snprintf(timesigstring,sizeof(timesigstring),"%s",place); /* [SEG] 2020-06-07 */
#endif
        readsig (&place, &timesig);
        if ((*place == 's') || (*place == 'l')) {
          event_error ("s and l in M: field not supported");
        };
        if ((timesig.num == 0) || (timesig.denom == 0)) {
          event_warning ("Invalid time signature ignored");
        } else {
          if (inhead) {
            /* copy timesig into master_timesig */
            copy_timesig(&master_timesig, &timesig);
          } 
          if (inbody) { 
            /* update timesig in current voice */
            voice_context_t *current_voice;

            current_voice = &voicecode[voicenum - 1];
            copy_timesig(&current_voice->timesig, &timesig);
          }
          event_timesig (&timesig);
          has_timesig = 1;
        }
      }
	    break;
    case 'L':
      {
	int num, denom;

  read_L_unitlen(&num, &denom, &place);
	if (num != 1)
	  {
	    event_error ("Default length must be 1/X");
	  }
	else
	  {
	    if (denom > 0)
	      {
		event_length (denom);
            if (inhead)
            {
              master_unitlen = denom;
            }
            if (inbody) {
              voice_context_t *current_voice;

              current_voice = &voicecode[voicenum - 1];
              current_voice->unitlen = denom;
            }
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
      preparse1_words (place);
      break;
    case '+':
      /* implement +: field as meaning w: + <lyrics> */
      preparse2_words(place, PLUS_FIELD);
      break;
    case 'd':
      /* decoration line in abcm2ps */
      event_field (key, place);	/* [SS] 2010-02-23 */
      break;
    case 's':
      event_field (key, place);	/* [SS] 2010-02-23 */
      break;
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

/* [JA] 2020-10-12 */
/* Look for problems in the use of repeats */
static void check_bar_repeats (int bar_type, char *replist)
{
  voice_context_t *cv = &voicecode[voicenum];
  char error_message[140];

  switch (bar_type) {
    case SINGLE_BAR:
      break;
    case DOUBLE_BAR:
      break;
    case THIN_THICK:
      break;
    case THICK_THIN:
      break;
    case BAR_REP:
      if (cv->expect_repeat) {
        event_warning ("Expecting repeat, found |:");
      };
      cv->expect_repeat = 1;
      cv->repeat_count = cv->repeat_count + 1;
      break;
    case REP_BAR:
      if (!cv->expect_repeat) {

        if (cv->repeat_count == 0)
        {
#ifdef NO_SNPRINT 
          sprintf(error_message, "Missing repeat at start ? Unexpected :|%s found", replist);
#else
          snprintf(error_message, 80, "Missing repeat at start ? Unexpected :|%s found", replist);
#endif
          event_warning (error_message);
        }
        else
        {
#ifdef NO_SNPRINT 
          sprintf(error_message,  "Unexpected :|%s found", replist);
#else
          snprintf(error_message, 80, "Unexpected :|%s found", replist);
#endif

          event_warning (error_message);
        }
      };
      cv->expect_repeat = 0;
      cv->repeat_count = cv->repeat_count + 1;
      break;
    case BAR1:
      if (!cv->expect_repeat) {
        if (cv->repeat_count == 0)
        {
          event_warning ("Missing repeat at start ? found |1");
        }
        else
        {
          event_warning ("found |1 in non-repeat section");
        }
      };
      break;
    case REP_BAR2:
      if (!cv->expect_repeat) {
        if (cv->repeat_count == 0)
        {
          event_warning ("Missing repeat at start ? found :|2");
        }
        else
        {
          event_warning ("No repeat expected, found :|2");
        }
      };
      cv->expect_repeat = 0;
      cv->repeat_count = cv->repeat_count + 1;
      break;
    case DOUBLE_REP:
      if (!cv->expect_repeat) {
        if (cv->repeat_count == 0)
        {
          event_warning ("Missing repeat at start ? found ::");
        }
        else
        {
          event_warning ("No repeat expected, found ::");
        }
      };
      cv->expect_repeat = 1;
      cv->repeat_count = cv->repeat_count + 1;
      break;
  };
}

/* [JA] 2020-10-12 */
static void check_and_call_bar(int bar_type, char *replist)
{
  if (repcheck)
  {
    check_bar_repeats (bar_type, replist);
  }
  event_bar (bar_type, replist);
}


/* [SS] 2021-10-11   */
static int pitch2midi(note, accidental, mult, octave )
/* computes MIDI pitch for note.
*/
char note, accidental;
int mult, octave;
{
  int p;
  char acc;
  int mul, noteno;
  int pitch;

  static int scale[7] = {0, 2, 4, 5, 7, 9, 11};

  static const char *anoctave = "cdefgab";
  acc = accidental;
  mul = mult;
  noteno = (int)note - 'a';
 
  p = (int) ((long) strchr(anoctave, note) - (long) anoctave);
  if (p < 0 || p > 6) {    /* [SS] 2022-12-22 */
	  printf("illegal note %c\n",note);
	  return 72;
  }
  p = scale[p];
  if (acc == '^') p = p + mul;
  if (acc == '_') p = p - mul;
  p = p + 12*octave + 60;
return p; 
}



/* [SS] 2021-10-11   */
int
note2midi (char** s, char *word)
{
/* for implementing sound=, shift= and instrument= key signature options */

/* check for accidentals */
char note, accidental;
char msg[80];
int octave;
int mult;
int pitch;
/*printf("note2midi: %c\n",**s);*/
if (**s == '\0') return 72;
note = **s;
mult = 1;
accidental = ' ';
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
      break;
    case '^':
      accidental = **s;
      *s = *s + 1;
      if (**s == '^')
        {
          *s = *s + 1;
          mult = 2;
        };
      break;
    case '=':
      accidental = **s;
      *s = *s + 1;
      break;
    default:
      break;
    };

  if ((**s >= 'a') && (**s <= 'g'))
    {
      note = **s;
      octave = 1;
      *s = *s + 1;
      while ((**s == '\'') || (**s == ',') || (**s == '/'))
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
          if (**s == '/')
             {
             /* skip / which occurs in instrument = command */
              *s = *s + 1;
/*  printf("note = %c  accidental = %c mult = %d octave= %d \n",note,accidental,mult,octave); */
	      if (casecmp(word,"instrument") != 0) { /* [SS] 2022-12-27 */
                 event_warning("score and shift directives do not expect an embedded '/'");
	          }

              pitch = pitch2midi(note, accidental, mult, octave);
              return pitch;
              }
        };
    }
  else
    {
      octave = 0;
      if ((**s >= 'A') && (**s <= 'G'))
        {
          note = **s + 'a' - 'A';
          *s = *s + 1;
          while ((**s == '\'') || (**s == ',') || (**s == '/'))
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
          if (**s == '/')
             {
             /* skip / which occurs in instrument = command */
              *s = *s + 1;
/*  printf("note = %c  accidental = %c mult = %d octave= %d \n",note,accidental,mult,octave); */
              pitch = pitch2midi(note, accidental, mult, octave);
              return pitch;
            };
        };
    };
 /* printf("note = %c  accidental = %c mult = %d octave= %d \n",note,accidental,mult,octave); */
  } 
  if (note == ' ')
    {
      event_error ("Malformed note : expecting a-g or A-G");
    }
  pitch = pitch2midi(note, accidental, mult, octave);
  return pitch;
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

      if (*p == '.' && *(p+1) == '|') { /* [SS] 2022-08.01 dotted bar */
         p = p +1;
         /* ignore dotted bar */
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

		initvstring (&gchord);
    /* modified JA 20 May 2022 */
    /* need to allow umlaut sequence in "guitar chords" which
     * are being used for arbitrary text e.g. "_last time"
     */
    p = umlaut_build_string(p+1, &gchord);
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
		  check_and_call_bar (BAR_REP, "");
		  p = p + 1;
		  break;
		case '|':
		  check_and_call_bar (DOUBLE_BAR, "");
		  p = p + 1;
		  break;
		case ']':
		  check_and_call_bar (THIN_THICK, "");
		  p = p + 1;
		  break;
		default:
		  p = getrep (p, playonrep_list);
		  check_and_call_bar (SINGLE_BAR, playonrep_list);
		};
	      break;
	    case ':':
	      p = p + 1;
	      switch (*p)
		{
		case ':':
		  check_and_call_bar (DOUBLE_REP, "");
		  p = p + 1;
		  break;
		case '|':
		  p = p + 1;
		  p = getrep (p, playonrep_list);
		  check_and_call_bar (REP_BAR, playonrep_list);
		  if (*p == ']')
		    p = p + 1;	/* [SS] 2013-10-31 */
		  break;
                /*  [JM] 2018-02-22 dotted bar line ... this is legal */
		default:
                /* [SS] 2018-02-08 introducing DOTTED_BAR */
		  check_and_call_bar (DOTTED_BAR,"");
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
		  check_and_call_bar (THICK_THIN, "");
		  if (*p == ':') {  /* [SS] 2015-04-13 [SDG] 2020-06-04 */
		      check_and_call_bar (BAR_REP, "");
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
	        check_and_call_bar (THIN_THICK, "");}
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
      case '$':
        p = p + 1;
        event_score_linebreak ('$');  /* [JA] 2020-10-07 */
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

  /* [SS] 2020-01-03 2021-02-21 */
  if (strstr(line,"%%begintext") != NULL) {
	  ignore_line = 1;
          }
  if (strstr(line,"%%endtext") != NULL) {
	  ignore_line = 0;
          }
  /* [SS] 2021-05-09 */
  if (strcmp(line,"%%beginps") == 0) {
	  ignore_line = 1;
          }
  if (strcmp(line,"%%endps") == 0) {
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
	      else if (inbody) preparse1_words (p); /* [SS] 2017-10-23 */
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

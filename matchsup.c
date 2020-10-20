/*
  matchsup.c  (abcmatch support functions)
  Seymour Shlien

  


 * originally store.c
 * abc2midi - program to convert abc files to MIDI files.
 * Copyright (C) 1999 James Allwright
 * e-mail: J.R.Allwright@westminster.ac.uk

store.c was adapted for the purpose of analyzing the contents
of abc tunes. The code is much the same except many functions
related to the generation of midi files has been trimmed off.
In particular: links to the code in genmidi.c and queue.c are gone.
All the gchord stuff is removed. There are options to suppress
all warning and error messages. Certain variables are no longer
extern's (parts, partno, bar_num, global_transpose etc.).
Output file generation stuff is gone. A new variable xrefno is
added to pass the reference number to abcmatch. Event_init has
been moved to the main program abcmatch.c and maxnotes has
been raised to 3000 since there seems to be a problem with
the autoextend procedure.  Some DEBUG statements were
added to addfeature - (they can probably be removed). Support
for text features and comments are gone. All the karaoke stuff
is gone. Tempo stuff, slurs, grace notes, decorations, hornpipe
indications  are all ignored. Event_note was changed to ignore
trills and rolls.  When a [chord] is included, only the highest
note is extracted from the chord. For debugging, a new function
print_feature_list was introduced.etc.

Essentially for comparing abc files, we want to ignore repeats,
grace notes, staccato indications and midi indications. We are
only interested in comparing the tunes. The main output 
which is used is the feature[],pitch,num[],denom[] list which
is used by abcmatch to create a new representation of the abc
file.

Some abc files with interleaved voices will not be treated
propertly with this program. In general parts, repeats and
voice indications are ignored.
 

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


#define XTEN1 1

#include "abc.h"
#include "parseabc.h"
#include "parser2.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef  WIN32
#include <string.h>  /* [gjg] 2012-02-01  Visual C++ 2010 Express compatability */
#endif

#ifdef __MWERKS__
#define __MACINTOSH__ 1
#endif /* __MWERKS__ */

#ifdef __MACINTOSH__
int setOutFileCreator(char *fileName,unsigned long theType,
                      unsigned long theCreator);
#endif /* __MACINTOSH__ */
/* define USE_INDEX if your C libraries have index() instead of strchr() */
#ifdef USE_INDEX
#define strchr index
#endif

#ifdef ANSILIBS
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#else
extern char* strchr();
extern void reduce();
#endif

#define MAXLINE 500
#define INITTEXTS 20
#define INITWORDS 20
#define MAXCHANS 16


/*#define DEBUG*/


/* global variables grouped roughly by function */

FILE *fp;

programname fileprogram = ABCMATCH;


/* parsing stage */
int tuplecount, tfact_num, tfact_denom, tnote_num, tnote_denom;
int specialtuple;
int gracenotes;
int headerpartlabel;
int dotune, pastheader;
int hornpipe, last_num, last_denom;
int timesigset;
int retain_accidentals;
int ratio_a, ratio_b;
int foundtitle; /* flag for capturing on the first title */

struct voicecontext {
  /* maps of accidentals for each stave line */
  char basemap[7], workmap[7];
  int  basemul[7], workmul[7];
  int default_length;
  int voiceno;
  int indexno;
  int hasgchords;
  int haswords;
  int inslur;
  int ingrace;
  int octaveshift;
  /* chord handling */
  int inchord, chordcount;
  int chord_num, chord_denom;
  /* details of last 2 notes/chords to apply length-modifiers to */
  int laststart, lastend, thisstart, thisend;
  /* broken rythm handling */
  int brokentype, brokenmult, brokenpending;
  int broken_stack[7];
  struct voicecontext* next;
};
struct voicecontext global;
struct voicecontext* v;
struct voicecontext* head;
int voicecount;

/* storage structure for strings */
int maxtexts = INITTEXTS;
char** atext;
int ntexts = 0;

int note_unit_length=8;

/* general purpose storage structure */
int maxnotes;
int *pitch, *num, *denom;
featuretype *feature;
int *pitchline;
int notes;

int verbose = 0;
int nowarn=1;
int noerror=1;
int xmatch;
int sf, mi;

/* Part handling */
struct vstring part;
int parts, partno, partlabel;
int part_start[26], part_count[26];

int voicesused;

/* Tempo handling (Q: field) */
int time_num, time_denom;
long tempo;
int tempo_num, tempo_denom;
int relative_tempo, Qtempo;
extern int division;
extern int div_factor;

/* output file generation */
int check;
int ntracks;

/* bar length checking */
int bar_num, bar_denom;
int barchecking;
int beat;

/* generating MIDI output */
int middle_c;
extern int channels[MAXCHANS + 3];

int global_transpose;

int additive;
int gfact_num, gfact_denom;

/* karaoke handling */
int karaoke, wcount;
char** words;
int maxwords = INITWORDS;
int xrefno;

extern int intune; /* signals to parsetune that tune is finished */
extern char titlename[48]; /* stores title of tune */
extern char keysignature[16];

/* Many of these functions have been retained in order to link with parseabc.
As I have been forced to also modifiy parseabc, now called abcparse, these
functions can also be removed eventually.
 */

char *featname[] = {
"SINGLE_BAR", "DOUBLE_BAR", "BAR_REP", "REP_BAR",
"PLAY_ON_REP", "REP1", "REP2", "BAR1",
"REP_BAR2", "DOUBLE_REP", "THICK_THIN", "THIN_THICK",
"PART", "TEMPO", "TIME", "KEY",
"REST", "TUPLE", "NOTE", "NONOTE",
"OLDTIE", "TEXT", "SLUR_ON", "SLUR_OFF",
"TIE", "CLOSE_TIE", "TITLE", "CHANNEL",
"TRANSPOSE", "RTRANSPOSE", "GTRANSPOSE", "GRACEON",
"GRACEOFF", "SETGRACE", "SETC", "SETTRIM", "GCHORD",
"GCHORDON", "GCHORDOFF", "VOICE", "CHORDON",
"CHORDOFF", "CHORDOFFEX", "DRUMON", "DRUMOFF",
"DRONEON", "DRONEOFF", "SLUR_TIE", "TNOTE",
"LT", "GT", "DYNAMIC", "LINENUM",
"MUSICLINE", "MUSICSTOP", "WORDLINE", "WORDSTOP",
"INSTRUCTION", "NOBEAM", "CHORDNOTE", "CLEF",
"PRINTLINE", "NEWPAGE", "LEFT_TEXT", "CENTRE_TEXT",
"VSKIP", "COPYRIGHT", "COMPOSER", "ARPEGGIO",
"SPLITVOICE", "META", "PEDAL_ON", "PEDAL_OFF", "EFFECT"
};



void event_info (place)
char * place;
{
}

void event_gchord (chord)
char * chord;
{
}

void event_slur (t)
int t;
{
}


void event_instruction (s)
char *s;
{
}


void event_reserved (p)
char p;
{
}

int bar_num, bar_denom, barno, barsize;
int b_num,b_denom;

void reduce(a, b)
/* elimate common factors in fraction a/b */
int *a, *b;
{
  int sign;
  int t, n, m;

  if (*a < 0) {
    sign = -1;
    *a = -*a;
  } else {
    sign = 1;
  };
  /* find HCF using Euclid's algorithm */
  if (*a > *b) {
    n = *a;
    m = *b;
  } else {
    n = *b;
    m = *a;
  };
  while (m != 0) {
    t = n % m;
    n = m;
    m = t;
  };
  *a = (*a/n)*sign;
  *b = *b/n;
}

void addunits(a, b)
/* add a/b to the count of units in the bar */
int a, b;
{
  bar_num = bar_num*(b*b_denom) + (a*b_num)*bar_denom;
  bar_denom = bar_denom * (b*b_denom);
  reduce(&bar_num, &bar_denom);
}


void set_meter(n, m)
/* set up variables associated with meter */
int n, m;
{
  /* set up barsize */
  barsize = n;
  if (barsize % 3 == 0) {
    beat = 3;
  } else {
    if (barsize % 2 == 0) {
      beat = 2;
    } else {
      beat = barsize;
    };
  };
  /* correction factor to make sure we count in the right units */
  if (m > 4) {
    b_num = m/4;
    b_denom = 1;
  } else {
   b_num = 1;
   b_denom = 4/m;
  };
}

int dummydecorator[DECSIZE]; /* used in event_chord */


static struct voicecontext* newvoice(n)
/* allocate and initialize the data for a new voice */
int n;
{
  struct voicecontext *s;
  int i;

  s = (struct voicecontext*) checkmalloc(sizeof(struct voicecontext));
  voicecount = voicecount + 1;
  s->voiceno = n;
  s->indexno = voicecount;
  s->default_length = global.default_length;
  s->hasgchords = 0;
  s->haswords = 0;
  s->inslur = 0;
  s->ingrace = 0;
  s->inchord = 0;
  s->chordcount = 0;
  s->laststart = -1;
  s->lastend = -1;
  s->thisstart = -1;
  s->thisend = -1;
  s->brokenpending = -1;
  s->next = NULL;
  for (i=0; i<7; i++) {
    s->basemap[i] = global.basemap[i];
    s->basemul[i] = global.basemul[i];
    s->workmap[i] = global.workmap[i];
    s->workmul[i] = global.workmul[i];
  };
  s->octaveshift = global.octaveshift;
  return(s);
}

static struct voicecontext* getvoicecontext(n)
/* find the data structure for a given voice number */
int n;
{
  struct voicecontext *p;
  struct voicecontext *q;

  p = head;
  q = NULL;
  while ((p != NULL) && (p->voiceno != n)) {
    q = p;
    p = p->next;
  };
  if (p == NULL) {
    p = newvoice(n);
    if (q != NULL) {
      q->next = p;
    };
  };
  if (head == NULL) {
    head = p;
  };
  return(p);
}

static void clearvoicecontexts()
/* free up all the memory allocated to voices */
{
  struct voicecontext *p;
  struct voicecontext *q;

  p = head;
  while (p != NULL) {
    q = p->next;
    free(p);
    p = q;
  };
  head = NULL;
}




void event_text(s)
/* text found in abc file */
char *s;
{
}


void event_specific (package, s)
char *package, *s;
{
}


void event_x_reserved(p)
/* reserved character H-Z found in abc file */
char p;
{
}

void event_abbreviation(symbol, string, container)
/* abbreviation encountered - this is handled within the parser */
char symbol;
char *string;
char container;
{
}


void event_acciaccatura()
{
/* does nothing here but outputs a / in abc2abc */
return;
}

/* [SS] 2015-03-23 */
void event_start_extended_overlay()
{
event_error("extended overlay not implemented in abcmatch");
}

void event_stop_extended_overlay()
{
event_error("extended overlay not implemented in abcmatch");
}


void event_split_voice()
{
}



void event_tex(s)
/* TeX command found - ignore it */
char *s;
{
}

void event_fatal_error(s)
/* print error message and halt */
char *s;
{
  event_error(s);
  exit(1);
}

void event_error(s)
/* generic error handler */
char *s;
{
if (noerror) return;
#ifdef NOFTELL
  extern int nullpass;

  if (nullpass != 1) {
    printf("Error in line %d : %s\n", lineno, s);
  };
#else
  printf("Error in line %d : %s\n", lineno, s);
#endif
}

void event_warning(s)
/* generic warning handler - for flagging possible errors */
char *s;
{
if (nowarn) return;
#ifdef NOFTELL
  extern int nullpass;

  if (nullpass != 1) {
    printf("Warning in line %d : %s\n", lineno, s);
  };
#else
  printf("Warning in line %d : %s\n", lineno, s);
#endif
}

static int autoextend(maxnotes)
/* increase the number of abc elements the program can cope with */
int maxnotes;
{
  int newlimit;
  int *ptr;
  featuretype *fptr;
  int i;

  if (verbose) {
    event_warning("Extending note capacity");
  };
  newlimit = maxnotes*2;
  fptr = (featuretype*) checkmalloc(newlimit*sizeof(featuretype));
  for(i=0;i<maxnotes;i++){
    fptr[i] = feature[i];
  };
  free(feature);
  feature = fptr;
  ptr = checkmalloc(newlimit*sizeof(int));
  for(i=0;i<maxnotes;i++){
    ptr[i] = pitch[i];
  };
  free(pitch);
  pitch = ptr;
  ptr = checkmalloc(newlimit*sizeof(int));
  for(i=0;i<maxnotes;i++){
    ptr[i] = pitchline[i];
  };
  free(pitchline);
  pitchline = ptr;
  ptr = checkmalloc(newlimit*sizeof(int));
  for(i=0;i<maxnotes;i++){
    ptr[i] = num[i];
  };
  free(num);
  num = ptr;
  ptr = checkmalloc(newlimit*sizeof(int));
  for(i=0;i<maxnotes;i++){
    ptr[i] = denom[i];
  };
  free(denom);
  denom = ptr;
  return(newlimit);
}


static void addfeature(f, p, n, d)
/* place feature in internal table */
int f, p, n, d;
{
  feature[notes] = f;
  pitch[notes] = p;
  num[notes] = n;
  denom[notes] = d;
  if ((f == NOTE) || (f == REST) || (f == CHORDOFF)) {
    reduce(&num[notes], &denom[notes]);
  };
#ifdef DEBUG
  if ((f == NOTE) || (f == REST)) {
    float  fract = (float) num[notes]/ (float) denom[notes];  /* [gjg] 2012-02-01 */
    int  length = fract * 12.0 +0.1;  /* [gjg] 2012-02-01 */
    printf("%2d %2d %2d %2d\n",pitch[notes],num[notes],denom[notes],length);
    }
  if (f == TIME) printf("time signature = %d/%d\n",n,d);
#endif 
  notes = notes + 1;
  if (notes >= maxnotes) {
    maxnotes = autoextend(maxnotes);
  };
}

void event_linebreak()
/* reached end of line in abc */
{
  addfeature(LINENUM, lineno, 0, 0);
}

/* a score linebreak character has been encountered */
void event_score_linebreak(char ch)
{
}

void event_startmusicline()
/* starting to parse line of abc music */
{
  addfeature(MUSICLINE, 0, 0, 0);
}

void event_endmusicline(endchar)
/* finished parsing line of abc music */
char endchar;
{
  addfeature(MUSICSTOP, 0, 0, 0);
}

static void textfeature(type, s)
/* called while parsing abc - stores an item which requires an */
/* associared string */
int type;
char* s;
{
}

void event_comment(s)
/* comment found in abc */
char *s;
{
}


void event_startinline()
/* start of in-line field in abc music line */
{
}

void event_closeinline()
/* end of in-line field in abc music line */
{
}

void event_field(k, f)
/* Handles R: T: and any other field not handled elsewhere */
char k;
char *f;
{
  if (dotune) {
    switch (k) {
    case 'R':
      {
        char* p;

        p = f;
        skipspace(&p);
/******** if ((strncmp(p, "Hornpipe", 8) == 0) ||
            (strncmp(p, "hornpipe", 8) == 0)) {
          hornpipe = 1;
        };
********/
      };
      break;
    case 'T':
     if (foundtitle == 0)  strncpy(titlename,f,46);
     foundtitle = 1; /* [SS] 2014-01-01 */
      break;
    default:
      {
      };
    };
  };
}

void event_words(p, continuation)
/* handles a w: field in the abc */
char* p;
int continuation;
{
}

/* [SS] 2014-08-16 */
void appendfield (morewords)
char *morewords;
{
printf("appendfield not implemented here\n");
}


static void checkbreak()
/* check that we are in not in chord, grace notes or tuple */
/* called at voice change */
{
  if (tuplecount != 0) {
    event_error("Previous voice has an unfinished tuple");
    tuplecount = 0;
  };
  if (v->inchord != 0) {
    event_error("Previous voice has incomplete chord");
    event_chordoff(1,1);
  };
  if (v->ingrace != 0) {
    event_error("Previous voice has unfinished grace notes");
    v->ingrace = 0;
  };
}


static void read_spec(spec, part)
/* converts a P: field to a list of part labels */
/* e.g. P:A(AB)3(CD)2 becomes P:AABABABCDCD */
/* A '+' indicates 'additive' behaviour (a part may include repeats).  */
/* A '-' indicates 'non-additive' behaviour (repeat marks in the music */
/* are ignored and only repeats implied by the part order statement    */
/* are played).  */
char spec[];
struct vstring* part;
{
}

void event_part(s)
/* handles a P: field in the abc */
char* s;
{
  char* p;

  if (dotune) {
    p = s;
    skipspace(&p);
    if (pastheader) {
      if (((int)*p < 'A') || ((int)*p > 'Z')) {
        event_error("Part must be one of A-Z");
        return;
      };
      if ((headerpartlabel == 1) && (part.st[0] == *p)) {
        /* P: field in header is not a label */
        headerpartlabel = 0;
        /* remove speculative part label */
        feature[part_start[(int)*p - (int)'A']] = NONOTE;
      } else {
        if (part_start[(int)*p - (int)'A'] != -1) {
          event_error("Part defined more than once");
        };
      };
      part_start[(int)*p - (int)'A'] = notes;
      addfeature(PART, (int)*p, 0, 0);
      checkbreak();
      v = getvoicecontext(1);
    } else {
      parts = 0;
      read_spec(p, &part);
      if (parts == 1) {
        /* might be a label not a specificaton */
        headerpartlabel = 1;
      };
    };
  };
}

void event_octave(int, int);

void event_voice(n, s, vp)
/* handles a V: field in the abc */
int n;
char *s;
struct voice_params *vp;
{
  if (pastheader || XTEN1) {
    voicesused = 1;
    if (pastheader)  checkbreak();
    v = getvoicecontext(n);
    addfeature(VOICE, v->indexno, 0, 0);
    if (vp->gotclef) {
      event_octave(vp->new_clef.octave_offset, 1);
    }
    if (vp->gotoctave) {
      event_octave(vp->octave,1);
    };
    if (vp->gottranspose) {
      addfeature(TRANSPOSE, vp->transpose, 0, 0);
    };
  } else {
    event_warning("V: in header ignored");
  };
}


void event_length(n)
/* handles an L: field in the abc */
int n;
{
  note_unit_length = 8;
  if (pastheader) {
    v->default_length = n;
  } else {
    global.default_length = n;
  };
}

static void tempounits(t_num, t_denom)
/* interprets Q: once default length is known */
int *t_num, *t_denom;
{
}

void event_tempo(n, a, b, rel, pre, post)
/* handles a Q: field e.g. Q: a/b = n  or  Q: Ca/b = n */
/* strings before and after are ignored */
int n;
int a, b, rel;
char *pre;
char *post;
{
}


void event_timesig(n, m, dochecking)
/* handles an M: field  M:n/m */
int n, m, dochecking;
{
  if (dotune) {
    if (pastheader) {
      addfeature(TIME, dochecking, n, m);
   } else { 
      time_num = n;
      time_denom = m;
      timesigset = 1;
      barchecking = dochecking;
    };
  };
}

void event_octave(num, local)
/* used internally by other routines when octave=N is encountered */
/* in I: or K: fields */
int num;
int local;
{
  if (dotune) {
    if (pastheader || local) {
      v->octaveshift = num;
    } else {
      global.octaveshift = num;
    };
  };
}

void event_info_key(key, value)
char* key;
char* value;
{
  int num;

  if (strcmp(key, "octave")==0) {
    num = readsnumf(value);
    event_octave(num,0);
  };
}

static void stack_broken(v)
struct voicecontext* v;
{
  v->broken_stack[0] = v->laststart;
  v->broken_stack[1] = v->lastend;
  v->broken_stack[2] = v->thisstart;
  v->broken_stack[3] = v->thisend;
  v->broken_stack[4] = v->brokentype;
  v->broken_stack[5] = v->brokenmult;
  v->broken_stack[6] = v->brokenpending;
  v->laststart = -1;
  v->lastend = -1;
  v->thisstart = -1;
  v->thisend = -1;
  v->brokenpending = -1;
}

static void restore_broken(v)
struct voicecontext* v;
{
  if (v->brokenpending != -1) {
    event_error("Unresolved broken rhythm in grace notes");
  };
  v->laststart = v->broken_stack[0];
  v->lastend = v->broken_stack[1];
  v->thisstart = v->broken_stack[2];
  v->thisend = v->broken_stack[3];
  v->brokentype = v->broken_stack[4];
  v->brokenmult = v->broken_stack[5];
  v->brokenpending = v->broken_stack[6];
}

void event_graceon()
/* a { in the abc */
{
  if (gracenotes) {
    event_error("Nested grace notes not allowed");
  } else {
    if (v->inchord) {
      event_error("Grace notes not allowed in chord");
    } else {
      gracenotes = 1;
      addfeature(GRACEON, 0, 0, 0);
      v->ingrace = 1;
      stack_broken(v);
    };
  };
}

void event_graceoff()
/* a } in the abc */
{
  if (!gracenotes) {
    event_error("} without matching {");
  } else {
    gracenotes = 0;
    addfeature(GRACEOFF, 0, 0, 0);
    v->ingrace = 0;
    restore_broken(v);
  };
}

void event_rep1()
/* [1 in the abc */
{
  addfeature(PLAY_ON_REP, 0, 0, 1);
/*
  if ((notes == 0) || (feature[notes-1] != SINGLE_BAR)) {
    event_error("[1 must follow a single bar");
  } else {
    feature[notes-1] = BAR1;
  };
*/
}

void event_rep2()
/* [2 in the abc */
{
  addfeature(PLAY_ON_REP, 0, 0, 2);
/*
  if ((notes == 0) || (feature[notes-1] != REP_BAR)) {
    event_error("[2 must follow a :| ");
  } else {
    feature[notes-1] = REP_BAR2;
  };
*/
}

void event_playonrep(s)
char* s;
/* [X in the abc, where X is a list of numbers */
{
  int num, converted;
  char seps[2];

  converted = sscanf(s, "%d%1[,-]", &num, seps);
  if (converted == 0) {
    event_error("corrupted variant ending");
  } else {
    if ((converted == 1) && (num != 0)) {
      addfeature(PLAY_ON_REP, 0, 0, num);
    } else {
      textfeature(PLAY_ON_REP, s);
    };
  };
}

static void slurtotie()
/* converts a pair of identical slurred notes to tied notes */
{
}

void event_sluron(t)
/* called when ( is encountered in the abc */
int t;
{
  if (t == 1) {
    addfeature(SLUR_ON, 0, 0, 0);
    v->inslur = 1;
  };
}

void event_sluroff(t)
/* called when ) is encountered */
int t;
{
  if (t == 0) {
    slurtotie();
    addfeature(SLUR_OFF, 0, 0, 0);
    v->inslur = 0;
  };
}

void event_tie()
/* a tie - has been encountered in the abc */
{
  addfeature(TIE, 0, 0, 0);
}

void event_space()
/* space character in the abc is ignored by abc2midi */
{
  /* ignore */
  /* printf("Space event\n"); */
}

void event_lineend(ch, n)
/* called when \ or ! or * or ** is encountered at the end of a line */
char ch;
int n;
{
  /* ignore */
}

void event_broken(type, mult)
/* handles > >> >>> < << <<< in the abc */
int type, mult;
{
  if (v->inchord) {
    event_error("Broken rhythm not allowed in chord");
  } else {
    if (v->ingrace) {
      event_error("Broken rhythm not allowed in grace notes");
    } else {
      v->brokentype = type;
      v->brokenmult = mult;
      v->brokenpending = 0;
    };
  };
}

void event_tuple(n, q, r)
/* handles triplets (3 and general tuplets (n:q:r in the abc */
int n, q, r;
{
  if (tuplecount > 0) {
    event_error("nested tuples");
  } else {
    if (r == 0) {
      specialtuple = 0;
      tuplecount = n;
    } else {
      specialtuple = 1;
      tuplecount = r;
    };
    if (q != 0) {
      tfact_num = q;
      tfact_denom = n;
    } else {
      if ((n < 2) || (n > 9)) {
        event_error("Only tuples (2 - (9 allowed");
        tfact_num = 1;
        tfact_denom = 1;
        tuplecount = 0;
      } else {
        /* deduce tfact_num using standard abc rules */
        if ((n == 2) || (n == 4) || (n == 8)) tfact_num = 3;
        if ((n == 3) || (n == 6)) tfact_num = 2;
        if ((n == 5) || (n == 7) || (n == 9)) {
          if ((time_num % 3) == 0) {
            tfact_num = 3;
          } else {
            tfact_num = 2;
          };
        };
        tfact_denom = n;
      };
    };
    tnote_num = 0;
    tnote_denom = 0;
  };
}

void event_chord()
/* a + has been encountered in the abc */
{
  if (v->inchord) {
    event_chordoff(1,1);
  } else {
    event_chordon(dummydecorator);
  };
}

void event_ignore () { };  /* [SS] 2018-12-21 */


static void lenmul(n, a, b)
/* multiply note length by a/b */
int n, a, b;
{
  if ((feature[n] == NOTE) || (feature[n] == REST) || 
      (feature[n] == CHORDOFF)) {
    num[n] = num[n] * a;
    denom[n] = denom[n] * b;
    reduce(&num[n], &denom[n]);
  };
}


static void brokenadjust()
/* adjust lengths of broken notes */
{
  int num1, num2, denom12;
  int j;
  int failed;

  switch(v->brokenmult) {
    case 1:
      num1 = ratio_b;
      num2 = ratio_a;
      break;
    case 2:
      num1 = 7;
      num2 = 1;
      break;
    case 3:
      num1 = 15;
      num2 = 1;
      break;
    default:
      num1=num2=1; /* [SDG] 2020-06-03 */
  };
  denom12 = (num1 + num2)/2;
  if (v->brokentype == LT) {
    j = num1;
    num1 = num2;
    num2 = j;
  };
  failed = 0;
  if ((v->laststart == -1) || (v->lastend == -1) || 
      (v->thisstart == -1) || (v->thisend == -1)) {
    failed = 1;
  } else {
    /* check for same length notes */
    if ((num[v->laststart]*denom[v->thisstart]) != 
             (num[v->thisstart]*denom[v->laststart])) {
      failed = 1;
    };
  };
  if (failed) {
    event_error("Cannot apply broken rhythm");
  } else {
/*
    printf("Adjusting %d to %d and %d to %d\n",
           v->laststart, v->lastend, v->thisstart, v->thisend);
*/
    for (j=v->laststart; j<=v->lastend; j++) {
      lenmul(j, num1, denom12);
    };
    for (j=v->thisstart; j<=v->thisend; j++) {
      lenmul(j, num2, denom12);
    };
  };
}

static void marknotestart()
/* voice data structure keeps a record of last few notes encountered */
/* in order to process broken rhythm. This is called at the start of */
/* a note or chord */
{
  v->laststart = v->thisstart;
  v->lastend = v->thisend;
  v->thisstart = notes-1;
}

static void marknoteend()
/* voice data structure keeps a record of last few notes encountered */
/* in order to process broken rhythm. This is called at the end of */
/* a note or chord */
{
  v->thisend = notes-1;
  if (v->brokenpending != -1) {
    v->brokenpending = v->brokenpending + 1;
    if (v->brokenpending == 1) {
      brokenadjust();
      v->brokenpending = -1;
    };
  };
}

static void marknote()
/* when handling a single note, not a chord, marknotestart() and */
/* marknoteend() can be called together */
{
  marknotestart();
  marknoteend();
}

/* just a stub to ignore 'y' */
void event_spacing(n, m)
int n,m;
{
}

void event_rest(decorators,n,m,type)
/* rest of n/m in the abc */
int n, m,type;
int decorators[DECSIZE];
{
  int num, denom;

  num = n;
  denom = m;
  if (v == NULL) {
    event_fatal_error("Internal error : no voice allocated");
  };
  if (v->inchord) v->chordcount = v->chordcount + 1;
  if (tuplecount > 0) {
    num = num * tfact_num;
    denom = denom * tfact_denom;
    if (tnote_num == 0) {
      tnote_num = num;
      tnote_denom = denom;
    } else {
      if (tnote_num * denom != num * tnote_denom) {
        if (!specialtuple) {
          event_warning("Different length notes in tuple");
        };
      };
    };
    if ((!gracenotes) && (!v->inchord)) {
      tuplecount = tuplecount - 1;
    };
  };
  if (v->chordcount == 1) {
    v->chord_num = num*4;
    v->chord_denom = denom*(v->default_length);
  };
  if ((!v->ingrace) && ((!v->inchord)||(v->chordcount==1))) {
    addunits(num, denom*(v->default_length));
  };
  last_num = 3; /* hornpiping (>) cannot follow rest */
  addfeature(REST, 0, num*4, denom*(v->default_length));
  if (!v->inchord ) {
    marknote();
  };
}

void event_mrest(n,m,c)
/* multiple bar rest of n/m in the abc */
/* we check for m == 1 in the parser */
int n, m;
char c; /* [SS] 2017-04-19 to distinguish X from Z in abc2abc */
{
  int i;
  int decorators[DECSIZE];
  decorators[FERMATA]=0;
/* it is not legal to pass a fermata to a multirest */

  for (i=0; i<n; i++) {
    event_rest(decorators,time_num*(v->default_length), time_denom,0);
    if (i != n-1) {
      event_bar(SINGLE_BAR, "");
    };
  };
}



void event_chordon(int chorddecorators[])
/* handles a chord start [ in the abc */
/* the array chorddecorators is needed in toabc.c and yapstree.c */
/* but is not relevant here.                                    */

{
  if (v->inchord) {
    event_error("Attempt to nest chords");
  } else {
    addfeature(CHORDON, 0, 0, 0);
    v->inchord = 1;
    v->chordcount = 0;
    v->chord_num = 0;
    v->chord_denom = 1;
    marknotestart();
  };
}

void event_chordoff(int chord_n, int chord_m)
/* handles a chord close ] in the abc */
{
  if (!v->inchord) {
    event_error("Chord already finished");
  } else {

    if(chord_m == 1 && chord_n == 1) /* chord length not set outside [] */
      addfeature(CHORDOFF, 0, v->chord_num, v->chord_denom);
    else
      addfeature(CHORDOFFEX, 0, chord_n*4, chord_m*v->default_length);

    v->inchord = 0;
    v->chordcount = 0;
    marknoteend();
    if (tuplecount > 0) --tuplecount;
  };
}


void event_finger(p)
/* a 1, 2, 3, 4 or 5 has been found in a guitar chord field */
char *p;
{
  /* does nothing */
}

static int pitchof(note, accidental, mult, octave, propogate_accs)
/* finds MIDI pitch value for note */
/* if propogate_accs is 1, apply any accidental to all instances of  */
/* that note in the bar. If propogate_accs is 0, accidental does not */
/* apply to other notes */
char note, accidental;
int mult, octave;
int propogate_accs;
{
  int p;
  char acc;
  int mul, noteno;
  static int scale[7] = {0, 2, 4, 5, 7, 9, 11};
  char *anoctave = "cdefgab";

  p = (int) ((long) strchr(anoctave, note) - (long) anoctave);
  p = scale[p];
  acc = accidental;
  mul = mult;
  noteno = (int)note - 'a';
  if (acc == ' ') {
    acc = v->workmap[noteno];
    mul = v->workmul[noteno];
  } else {
    if ((retain_accidentals) && (propogate_accs)) {
      v->workmap[noteno] = acc;
      v->workmul[noteno] = mul;
    };
  };
  if (acc == '^') p = p + mul;
  if (acc == '_') p = p - mul;
  return p + 12*octave + middle_c;
}



static void hornp(num, denom)
/* If we have used R:hornpipe, this routine modifies the rhythm by */
/* applying appropriate broken rhythm                              */
int num, denom;
{
  if ((hornpipe) && (notes > 0) && (feature[notes-1] != GT)) {
    if ((num*last_denom == last_num*denom) && (num == 1) &&
        (denom*time_num == 32)) {
      if (((time_num == 4) && (bar_denom == 8)) ||
          ((time_num == 2) && (bar_denom == 16))) {
           /* addfeature(GT, 1, 0, 0); */
           v->brokentype = GT;
           v->brokenmult = 1;
           v->brokenpending = 0;
      };
    };
    last_num = num;
    last_denom = denom;
  };
}


void event_note(decorators, clef, accidental, mult, note, xoctave, n, m)
/* handles a note in the abc */
int decorators[DECSIZE];
cleftype_t *clef;
char accidental;
int mult;
char note;
int xoctave, n, m;
{
  int pitch;
  int pitch_noacc;
  int num, denom;
  int octave;

  if (v == NULL) {
    event_fatal_error("Internal error - no voice allocated");
  };
  octave = xoctave + v->octaveshift;
  num = n;
  denom = m;
  if (v->inchord) v->chordcount = v->chordcount + 1;
  if (tuplecount > 0) {
    num = num * tfact_num;
    denom = denom * tfact_denom;
    if (tnote_num == 0) {
      tnote_num = num;
      tnote_denom = denom;
    } else {
      if (tnote_num * denom != num * tnote_denom) {
        if (!specialtuple) {
          event_warning("Different length notes in tuple");
        };
      };
    };
    if ((!gracenotes) &&
        ((!v->inchord) || ((v->inchord) && (v->chordcount == 1)))) {
      tuplecount = tuplecount - 1;
    };
  };
  if ((!v->ingrace) && (!v->inchord)) {
    hornp(num, denom*(v->default_length));
  } else {
    last_num = 3; /* hornpiping (>) cannot follow chord or grace notes */
  };
  if ((!v->ingrace) && ((!v->inchord)||(v->chordcount==1))) {
    addunits(num, denom*(v->default_length));
  };
  pitch = pitchof(note, accidental, mult, octave, 1);
  pitch_noacc = pitchof(note,0,0,octave,0);
  if (decorators[FERMATA]) {
    num = num*2;
  };
  if (v->chordcount == 1) {
    v->chord_num = num*4;
    v->chord_denom = denom*(v->default_length);
  };

   pitchline[notes] = pitch_noacc;
   addfeature(NOTE, pitch, num*4, denom*(v->default_length));
   marknote();
}

void event_microtone(int dir, int a, int b)
{
}

void event_temperament(char *line) {
}


void event_normal_tone()
{
}



char *get_accidental(place, accidental)
/* read in accidental - used by event_handle_gchord() */
char *place; /* place in string being parsed */
char *accidental; /* pointer to char variable */
{
  char *p;

  p = place;
  *accidental = '=';
  if (*p == '#') {
    *accidental = '^';
    p = p + 1;
  };
  if (*p == 'b') {
    *accidental = '_';
    p = p + 1;
  };
  return(p);
}

void event_handle_gchord(s)
/* handler for the guitar chords */
char* s;
{
}

void event_handle_instruction(s)
/* handler for ! ! instructions */
/* does ppp pp p mp mf f ff fff */
/* also does !drum! and !nodrum! */
char* s;
{
}

static void setmap(sf, map, mult)
/* work out accidentals to be applied to each note */
int sf; /* number of sharps in key signature -7 to +7 */
char map[7];
int mult[7];
{
  int j;

  for (j=0; j<7; j++) {
    map[j] = '=';
    mult[j] = 1;
  };
  if (sf >= 1) map['f'-'a'] = '^';
  if (sf >= 2) map['c'-'a'] = '^';
  if (sf >= 3) map['g'-'a'] = '^';
  if (sf >= 4) map['d'-'a'] = '^';
  if (sf >= 5) map['a'-'a'] = '^';
  if (sf >= 6) map['e'-'a'] = '^';
  if (sf >= 7) map['b'-'a'] = '^';
  if (sf <= -1) map['b'-'a'] = '_';
  if (sf <= -2) map['e'-'a'] = '_';
  if (sf <= -3) map['a'-'a'] = '_';
  if (sf <= -4) map['d'-'a'] = '_';
  if (sf <= -5) map['g'-'a'] = '_';
  if (sf <= -6) map['c'-'a'] = '_';
  if (sf <= -7) map['f'-'a'] = '_';
}

static void altermap(v, modmap, modmul)
/* apply modifiers to a set of accidentals */
struct voicecontext* v;
char modmap[7];
int modmul[7];
{
  int i;

  for (i=0; i<7; i++) {
    if (modmap[i] != ' ') {
      v->basemap[i] = modmap[i];
      v->basemul[i] = modmul[i];
    };
  };
}

static void copymap(v)
/* sets up working map at the start of each bar */
struct voicecontext* v;
{
  int j;

  for (j=0; j<7; j++) {
    v->workmap[j] = v->basemap[j];
    v->workmul[j] = v->basemul[j];
  };
}

/* workaround for problems with PCC compiler */
/* data may be written to an internal buffer */

int myputc(c)
char c;
{
  return (putc(c,fp));
}

static void addfract(xnum, xdenom, a, b)
/* add a/b to the count of units in the bar */
int *xnum;
int *xdenom;
int a, b;
{
  *xnum = (*xnum)*b + a*(*xdenom);
  *xdenom = (*xdenom) * b;
  reduce(xnum, xdenom);
}


static void dotie(j, xinchord,voiceno)
/* called in preprocessing stage to handle ties */
/* we need the voiceno in case a tie is broken by a */
/* voice switch.                                    */
int j, xinchord,voiceno;
{
  int tienote, place;
  int tietodo, done;
  int lastnote, lasttie;
  int inchord;
  int tied_num, tied_denom;
  int localvoiceno;
  int samechord;

  /* find note to be tied */
  samechord = 0;
  if (xinchord) samechord = 1;
  tienote = j;
  localvoiceno = voiceno;
  while ((tienote > 0) && (feature[tienote] != NOTE) &&
         (feature[tienote] != REST)) {
    tienote = tienote - 1;
  };
  if (feature[tienote] != NOTE) {
    event_error("Cannot find note before tie");
  } else {
    inchord = xinchord;
    /* change NOTE + TIE to TNOTE + REST */
    feature[tienote] = TNOTE;
    feature[j] = REST;
    num[j] = num[tienote];
    denom[j] = denom[tienote];
    place = j;
    tietodo = 1;
    lasttie = j;
    tied_num = num[tienote];
    tied_denom = denom[tienote];
    lastnote = -1;
    done = 0;
    while ((place < notes) && (tied_num >=0) && (done == 0)) {
     /* printf("%d %s   %d %d/%d ",place,featname[feature[place]],pitch[place],num[place],denom[place]); */
      switch (feature[place]) {
        case NOTE:
          if(localvoiceno != voiceno) break;
          lastnote = place;
          if ((tied_num == 0) && (tietodo == 0)) {
            done = 1;
          };
          if ((pitchline[place] == pitchline[tienote])
             && (tietodo == 1) && (samechord == 0)) {
            /* tie in note */
            if (tied_num != 0) {
              event_error("Time mismatch at tie");
            };
            tietodo = 0;
	    pitch[place] = pitch[tienote]; /* in case accidentals did not
					      propagate                   */
            /* add time to tied time */
            addfract(&tied_num, &tied_denom, num[place], denom[place]);
            /* add time to tied note */
            addfract(&num[tienote], &denom[tienote], num[place], denom[place]);
            /* change note to a rest */
            feature[place] = REST;
            /* get rid of tie */
            if (lasttie != j) {
              feature[lasttie] = OLDTIE;
            };
          };
          if (inchord == 0) {
            /* subtract time from tied time */
            addfract(&tied_num, &tied_denom, -num[place], denom[place]);
          };
          break;
        case REST:
          if(localvoiceno != voiceno) break;
          if ((tied_num == 0) && (tietodo == 0)) {
            done = 1;
          };
          if (inchord == 0) {
            /* subtract time from tied time */
            addfract(&tied_num, &tied_denom, -num[place], denom[place]);
          };
          break;
        case TIE:
          if(localvoiceno != voiceno) break;
          if (lastnote == -1) {
            event_error("Bad tie: possibly two ties in a row");
          } else {
            if (pitch[lastnote] == pitch[tienote] && samechord == 0) {
              lasttie = place;
              tietodo = 1;
              if (inchord) samechord = 1;
            };
          };
          break;
        case CHORDON:
          if(localvoiceno != voiceno) break;
          inchord = 1;
          break;
        case CHORDOFF:
        case CHORDOFFEX:
          samechord = 0;
          if(localvoiceno != voiceno) break;
          inchord = 0;
          /* subtract time from tied time */
          addfract(&tied_num, &tied_denom, -num[place], denom[place]);
          break;
        case VOICE:
          localvoiceno = pitch[place];
        default:
          break;
      };
      /*printf("tied_num = %d done = %d inchord = %d\n",tied_num, done, inchord); */
      place = place + 1;
    };
    if (tietodo == 1) {
      event_error("Could not find note to be tied");
    };
  };
/* printf("dotie finished\n"); */
}

static void tiefix()
/* connect up tied notes and cleans up the */
/* note lengths in the chords (eg [ace]3 ) */
{
  int j;
  int inchord;
  int chord_num=-1, chord_denom=1; /*[SDG] 2020-06-03 */
  int chord_start,chord_end;
  int voiceno;

  j = 0;
  inchord = 0;
  voiceno = 1;
  while (j<notes) {
    switch (feature[j]) {
    case CHORDON:
      inchord = 1;
      chord_num = -1;
      chord_start = j;
      j = j + 1;
      break;
    case CHORDOFFEX:
      inchord = 0;
      chord_end=j;
      j = j + 1;
      break;
    case CHORDOFF:
      if (!((!inchord) || (chord_num == -1))) {
        num[j] = chord_num; 
        denom[j] = chord_denom; 
      };
      inchord = 0;
      chord_end=j;
      j = j + 1;
      break;
    case NOTE:
      if ((inchord) && (chord_num == -1)) {
        chord_num = num[j];
        chord_denom = denom[j];
        /* use note length in num,denom as chord length */
        /* if chord length is not set outside []        */
      };
      j = j + 1;
      break;
    case REST:
      if ((inchord) && (chord_num == -1)) {
        chord_num = num[j];
        chord_denom = denom[j];
      };
      j = j + 1;
      break;
    case TIE:
      /*dotie(j, inchord,voiceno); [SS] 2013-04-04 */
      j = j + 1;
      break;
    case LINENUM:
      lineno = pitch[j];
      j = j + 1;
      break;
    case VOICE:
      voiceno = pitch[j];
    default:
      j = j + 1;
      break;
    };
  };
}


static void zerobar()
/* start a new count of beats in the bar */
{
  bar_num = 0;
  bar_denom = 1;
}

void event_bar(type, replist)
/* handles bar lines of various types in the abc */
int type;
char* replist;
{
  int newtype;
#ifdef DEBUG
  printf("bar line\n");
#endif
  newtype = type;
  if ((type == THIN_THICK) || (type == THICK_THIN)) {
    newtype = DOUBLE_BAR;
  };
  if (type == BAR1) {
    newtype = SINGLE_BAR;
  };
  if (type == REP_BAR2) {
    newtype = REP_BAR;
  };
  addfeature(newtype, 0, 0, 0);
  copymap(v);
  zerobar();
  if (strlen(replist) > 0) {
    event_playonrep(replist);
  };
/*
  if (type == BAR1) {
    addfeature(PLAY_ON_REP, 0, 0, 1);
  };
  if (type == REP_BAR2) {
    addfeature(PLAY_ON_REP, 0, 0, 2);
  };
*/
}




void startfile()
/* called at the beginning of an abc tune by event_refno */
/* This sets up all the default values */
{
  int j;

  if (verbose) {
    printf("scanning tune\n");
  };
  /* set up defaults */
  sf = 0;
  mi = 0;
  setmap(0, global.basemap, global.basemul);
  copymap(&global);
  global.octaveshift = 0;
  voicecount = 0;
  head = NULL;
  v = NULL;
  time_num = 4;
  time_denom = 4;
  timesigset = 0;
  barchecking = 1;
  global.default_length = -1;
  event_tempo(120, 1, 4, 0,NULL, NULL);
  notes = 0;
  ntexts = 0;
  gfact_num = 1;
  gfact_denom = 3;
  global_transpose = 0;
  hornpipe = 0;
  karaoke = 0;
  retain_accidentals = 1;
  ratio_a = 2;
  ratio_b = 4;
  wcount = 0;
  parts = -1;
  middle_c = 60;
  for (j=0; j<26; j++) {
    part_start[j] = -1;
  };
  headerpartlabel = 0;
  additive = 1;
  initvstring(&part);
 }


static void headerprocess()
/* called after the K: field has been reached, signifying the end of */
/* the header and the start of the tune */
{
  int t_num, t_denom;

  if (headerpartlabel == 1) {
    part_start[(int)part.st[0] - (int)'A'] = notes;
    addfeature(PART, part.st[0], 0, 0);
  };
  addfeature(DOUBLE_BAR, 0, 0, 0);
  pastheader = 1;

  gracenotes = 0; /* not in a grace notes section */
  if (!timesigset) {
    event_warning("No M: in header, using default");
  };
  /* calculate time for a default length note */
  if (global.default_length == -1) {
    if (((float) time_num)/time_denom < 0.75) {
      global.default_length = 16;
    } else {
      global.default_length = 8;
    };
  };
  bar_num = 0;
  bar_denom = 1;
  set_meter(time_num, time_denom); 
  if (hornpipe) {
    if ((time_denom != 4) || ((time_num != 2) && (time_num != 4))) {
      event_error("Hornpipe must be in 2/4 or 4/4 time");
      hornpipe = 0;
    };
  };

  tempounits(&t_num, &t_denom);
  /* make tempo in terms of 1/4 notes */
/*  tempo = (long) 60*1000000*t_denom/(Qtempo*4*t_num); */
/*  div_factor = division; */
  voicesused = 0;
}

void event_key(sharps, s, modeindex, modmap, modmul, modmicrotone,
          gotkey, gotclef, clefname, clef,
          octave, transpose, gotoctave, gottranspose, explict)
/* handles a K: field */
int sharps; /* sharps is number of sharps in key signature */
char *s; /* original string following K: */
int modeindex; /* 0 major, 1,2,3 minor, 4 locrian, etc.  */
char modmap[7]; /* array of accidentals to be applied */
int  modmul[7]; /* array giving multiplicity of each accent (1 or 2) */
struct fraction modmicrotone[7]; /* [SS] 2014-01-06 */
int gotkey, gotclef;
char* clefname;
cleftype_t *clef;  /* [JA] 2020-10-19 */
int octave, transpose, gotoctave, gottranspose;
int explict;
{
  int minor;
  strncpy(keysignature,s,16);
  if (modeindex >0 && modeindex <4) minor = 1;
  if ((dotune) && gotkey) {
    if (pastheader) {
      setmap(sharps, v->basemap, v->basemul);
      altermap(v, modmap, modmul);
      copymap(v);
      addfeature(KEY, sharps, 0, minor);
      if (gottranspose) {
        addfeature(TRANSPOSE, transpose, 0, 0);
      };
    } else {
      if (gottranspose) {
        global_transpose = transpose;
      };
      setmap(sharps, global.basemap, global.basemul);
      altermap(&global, modmap, modmul);
      copymap(&global);
      sf = sharps;
      mi = minor;
      headerprocess();
      v = newvoice(1);
      head = v;
    };
    if (gotclef) {
      event_octave(clef->octave_offset, 0);
    }
    if (gotoctave) {
      event_octave(octave,0);
    };
  };
}



void print_feature_list ()
{
int i,length;
float fract;
printf("feature list \n");
for (i=0;i<notes;i++) {
  printf("%d %d %s %d %d %d",i,feature[i],featname[feature[i]],pitch[i],num[i],denom[i]);
  if (feature[i] == NOTE || feature[i] == TNOTE || feature[i] == REST) {
    fract = (float) num[i]/ (float) denom[i];
    length = (int) (fract * 12.0 +0.1);
    printf(" %d",length);
    }
  printf("\n");
  }
}

static void finishfile()
/* end of tune has been reached - write out MIDI file */
{
  extern int nullputc();
  int i;


  clearvoicecontexts();
  if (!pastheader) {
    event_error("No valid K: field found at start of tune");
  } else {


    if (parts > -1) {
      addfeature(PART, ' ', 0, 0);
    };
    if (headerpartlabel == 1) {
      event_error("P: field in header should go after K: field");
    };

    tiefix();


    };
    for (i=0; i<ntexts; i++) {
      free(atext[i]);
    };
    for (i=0; i<wcount; i++) {
      free(words[i]);
    };
    freevstring(&part);
}



void event_blankline()
/* blank line found in abc signifies the end of a tune */
{
  if (dotune) {
    intune = 0;
    finishfile();
    parseroff();
    dotune = 0;
  };
}

void event_refno(n)
/* handles an X: field (which indicates the start of a tune) */
int n;
{
  foundtitle = 0; /* [SS] 2014-01-10 */
  if (dotune) {
    finishfile();
    parseroff();
    dotune = 0;
    intune = 0; /* [SS] 2013-03-20 */
  };
  if (verbose) {
    printf("Reference X: %d\n", n);
  };
  if ((n == xmatch) || (xmatch == 0) || (xmatch == -1)) {
    parseron();
    dotune = 1;
    pastheader = 0;
    xrefno = n;
    startfile();
    return;
  };
}

void event_eof()
/* end of abc file encountered */
{
  if (dotune) {
    dotune = 0;
    parseroff();
    finishfile();
  };
  if (verbose) {
    printf("End of File reached\n");
  };
}


void free_feature_representation ()
{
  free(pitch);
  free(pitchline);
  free(num);
  free(denom);
  free(feature);
  free(words);
}



void dumpfeat (int from, int to)
{
int i,j;
for (i=from;i<=to;i++)
  {
  j = feature[i];
  if (j<0 || j>73) printf("illegal feature[%d] = %d\n",i,j); /* [SS] 2012-11-25 */
  else printf("%d %s   %d %d %d \n",i,featname[j],pitch[i],num[i],denom[i]);
  /* [SDG] 2020-06-03 removed extra %d */
  }
}



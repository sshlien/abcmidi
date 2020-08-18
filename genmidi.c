/*
 * abc2midi - program to convert abc files to MIDI files.
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

/* genmidi.c  
 * This is the code for generating MIDI output from the stored music held
 * in arrays feature, num, denom, pitch. The top-level routine is
 * writetrack(). This file is part of abc2midi.
 *
 * 14th January 1999
 * James Allwright
 *
 */
/* for Microsoft Visual C++ Ver 6 and higher */
#ifdef _MSC_VER
#define ANSILIBS
#endif

#include "abc.h"
#include "parseabc.h"
#include "queues.h"
#include "genmidi.h"
#include "midifile.h"
#include <stdio.h>
#ifdef ANSILIBS
#include <string.h>
#include <ctype.h>
#endif
/* define USE_INDEX if your C libraries have index() instead of strchr() */
#ifdef USE_INDEX
#define strchr index
#endif
#include <stdlib.h>
#define MAX(A, B) ((A) > (B) ? (A) : (B))

void   single_note_tuning_change(int midikey, float midipitch);
void addfract(int *xnum, int *xdenom, int a, int b);
/* [SS] 2011-08-17 */
void fdursum_at_segment(int segposnum, int segposden, int *val_num, int *val_den);
void articulated_stress_factors (int n,  int *vel); 

void write_event(int event_type, int channel, char data[], int n);

float ranfrac ()
{
return rand()/(float) RAND_MAX;
}

void setbeat();
void parse_drummap(char **s); /* [SS] 2017-12-10 no more static */

/* global variables grouped roughly by function */

extern int lineno; /* source line being parsed */
extern int lineposition; /* character position in line */

extern char** atext;

/* Named guitar chords */
extern int chordnotes[MAXCHORDNAMES][10];  /* [SS] 2012-01-29 */
extern int chordlen[MAXCHORDNAMES];

/* general purpose storage structure */
/* these 6 arrays are used to hold the tune data */
extern int *pitch, *num, *denom;
extern int *bentpitch;
extern int *decotype; /* [SS] 2012-11-25 */
extern int *charloc; /* [SS] 2014-12-25 */
extern featuretype *feature;
extern int *stressvelocity; /* [SS] 2011-08-17 */
extern int notes;
extern int barflymode; /* [SS] 2011-08-24 */
extern int stressmodel; /* [SS] 2011-08-26 */
extern int programbase; /* [SS] 2017-06-02 */

extern int verbose;
extern int quiet;
extern int sf, mi;
extern int silent; /* [SS] 2014-10-16 */

extern int retuning,bend; /* [SS] 2012-04-01 */
int drumbars;
int gchordbars;

/* Part handling */
extern struct vstring part;
int parts, partno, partlabel;
int part_start[26], part_count[26];
long introlen, lastlen, partlen[26];
int partrepno;
/* int additive;  not supported any more [SS] 2004-10-08*/
int err_num, err_denom;

extern int voicesused;
extern int dependent_voice[];

/* Tempo handling (Q: field) */
extern long tempo;
extern int time_num, time_denom; /* time sig. for the tune */
extern int mtime_num, mtime_denom; /* current time sig. when generating MIDI */
int div_factor;
int division = DIV;

long delta_time; /* time since last MIDI event */
long delta_time_track0; /* [SS] 2010-06-27 */
long tracklen, tracklen1;
long barloc[1024]; /* [SS] 2019-03-20 */

/* output file generation */
extern int ntracks;

/* bar length checking */
int bar_num, bar_denom, barno, barsize;
int b_num, b_denom;
extern int barchecking;


/* time signature after header processed */
extern int header_time_num,header_time_denom;



/* generating MIDI output */
int beat;
int loudnote, mednote, softnote;
int beataccents;
int velocity_increment = 10; /* for crescendo and decrescendo */
char beatstring[100]; 
int nbeats;
int channel, program;
#define MAXCHANS 16
int channel_in_use[MAXCHANS+3];
int current_pitchbend[MAXCHANS];
int current_program[MAXCHANS];
int  transpose;
int  global_transpose=0;
int chordchannels[10]; /* for handling in voice chords and microtones */
int nchordchannels = 0;
extern int no_more_free_channels; /* [SS] 2015-03-23 from store.c */

/* [SS] 2015-09-07 */
int single_velocity_inc;
int single_velocity;

/* karaoke handling */
extern int karaoke, wcount;
int kspace;
char* wordlineptr;
extern char** words;
int thismline, thiswline, windex, thiswfeature;
int wordlineplace;
int nowordline;
int waitforbar;
int wlineno, syllcount;
int lyricsyllables, musicsyllables;
/* the following are booleans to select features in current track */
int wordson, noteson, gchordson, temposon, drumson, droneon;
int hyphenstate;  /* [Bas Schoutsen] 2010-04-08 */

/* Generating accompaniment */
int gchords, g_started;
int basepitch, inversion, chordnum;
int gchordnotes[6],gchordnotes_size;

struct notetype {
  int base;
  int chan;
  int vel;
};
struct notetype gchord, fun;
int g_num, g_denom;
int g_next;
char gchord_seq[40];
int gchord_len[40];
int g_ptr;

int tracknumber; /* [SS] 2014-11-17 */


/* [SS] 2015-05-21 */
struct dronestruct {
   int chan;  /* MIDI channel assigned to drone */
   int event; /* stores time in MIDI pulses when last drone event occurred*/
   int prog;  /* MIDI program (instrument) to use for drone */
   int pitch1;/* MIDI pitch of first drone tone */
   int vel1;  /* MIDI velocity (loudness) of first drone tone */
   int pitch2;/* MIDI pitch of second drone tone */
   int vel2;  /* MIDI velocity (loudnress) of second drone tone */
} drone = {1, 0, 70, 45, 80, 33, 80}; /* bassoon a# */




/* Generating drum track */
int drum_num, drum_denom;
char drum_seq[40];
int drum_len[40];
int drum_velocity[40], drum_program[40];
int drum_ptr, drum_on;

int notecount=0;  /* number of notes in a chord [ABC..] */
int notedelay=10;  /* time interval in MIDI ticks between */
                   /*  start of notes in chord */
int chordattack=0;
int staticnotedelay=10;  /* introduced to handle !arpeggio! */
int staticchordattack=0;
int totalnotedelay=0; /* total time delay introduced */

int trim=1; /* to add a silent gap to note */
int trim_num = 1;
int trim_denom = 5;

/* [SS] 2015-06-16 */
int expand=0; /* overlap note past next note */
int expand_num = 0;
int expand_denom = 5;

/* channel 10 drum handling */
int drum_map[256];

int gchord_error = 0; /* [SS] 2010-07-11 */

extern struct trackstruct trackdescriptor[40]; /* trackstruct defined in genmidi.h*/


/* [SS] 2011-07-04 */
int beatmodel = 0; /* flag selecting standard or Phil's model */

/* [SS] 2012-12-12 */
int bendvelocity = 100;
int bendacceleration = 300;

/* [SS] 2014-09-09 */
int bendstate = 8192; /* also linked with queues.c */

/* [SS] 2015-09-10 2015-10-03 */
int benddata[256];
int bendnvals;
int bendtype = 1;

/* [SS] 2015-07-24 2015-10-03 */
#define MAXLAYERS 3
int controldata[MAXLAYERS][256];
int controlnvals[MAXLAYERS];
int controldefaults[128]; /* [SS] 2015-08-10 */
int nlayers = 0; /* [SS] 2015-08-20 */
int controlcombo = 0; /* [SS] 2015-08-20 */


/* for handling stress models */
int nseg;       /* number of segments */
int ngain[32];  /* gain factor for each segment */
float maxdur;   /* maximum duration */
int segnum,segden; /* segment width computed from M: and L: parameters*/
float fdur[32]; /* duration modifier for each segment */
float fdursum[32]; /* for mapping segment address into a position */

char *featname[] = {
"SINGLE_BAR", "DOUBLE_BAR","DOTTED_BAR", "BAR_REP", "REP_BAR",
"PLAY_ON_REP", "REP1", "REP2", "BAR1",
"REP_BAR2", "DOUBLE_REP", "THICK_THIN", "THIN_THICK",
"PART", "TEMPO", "TIME", "KEY",
"REST", "TUPLE", "NOTE", "NONOTE",
"OLDTIE", "TEXT", "SLUR_ON", "SLUR_OFF",
"TIE", "CLOSE_TIE", "TITLE", "CHANNEL",
"TRANSPOSE", "RTRANSPOSE", "GTRANSPOSE", "GRACEON",
"GRACEOFF", "SETGRACE", "SETC", "SETTRIM","EXPAND", "GCHORD",
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

int gtfract(anum,adenom, bnum,bdenom)
/* compare two fractions  anum/adenom > bnum/bdenom */
/* returns   (a > b)                       */
int anum,adenom,bnum,bdenom;
{
  if ((anum*bdenom) > (bnum*adenom)) {
    return(1);
  } else {
    return(0);
  };
}



void addunits(a, b)
/* add a/b to the count of units in the bar */
int a, b;
{
  bar_num = bar_num*(b*b_denom) + (a*b_num)*bar_denom;
  bar_denom = bar_denom * (b*b_denom);
  reduce(&bar_num, &bar_denom);
  /*printf("position = %d/%d\n",bar_num,bar_denom);*/
}

void configure_gchord()
/* creates a list of notes to played as chord for
 * a specific guitar chord. Most of the code figures out
 * how to order the notes when inversions are encountered.
*/
{
 int j;
 int inchord, note;
 gchordnotes_size = 0;

inchord = 0;
if (inversion != -1) {
/* try to match inversion with basepitch+chordnotes.. */
  for (j=0; j<chordlen[chordnum]; j++) {
       if ((basepitch + chordnotes[chordnum][j]) % 12 == inversion % 12) {
            inchord = j;
	    };
       };

/* do not add strange note to chord [SS] 2008-09-24 */
/*  if ((inchord == 0) && (inversion > basepitch)) {
**        inversion = inversion - 12;
**        gchordnotes[gchordnotes_size] = inversion+gchord.base;
**        gchordnotes_size++;
**        };
***/
   };
for (j=0; j<chordlen[chordnum]; j++) {
    note = basepitch + chordnotes[chordnum][j]; 
   if (j < inchord) 
    note += 12;
   gchordnotes[gchordnotes_size] = gchord.base+note;
   gchordnotes_size++;
   };
}



void set_gchords(s)
char* s;
/* set up a string which indicates how to generate accompaniment from */
/* guitar chords (i.e. "A", "G" in abc). */
/* called from dodeferred(), startfile() and setbeat() */
{
  int seq_len;
  char* p;
  int j;

  p = s;
  j = 0;
  seq_len = 0;
    while ((strchr("zcfbghijGHIJx", *p) != NULL) && (j <39)) {
    if (*p == 0) break;
    gchord_seq[j] = *p;
    p = p + 1;
    if ((*p >= '0') && (*p <= '9')) {
      gchord_len[j] = readnump(&p);
    } else {
      gchord_len[j] = 1;
    };
    seq_len = seq_len + gchord_len[j];
    j = j + 1;
  };
  if (seq_len == 0) {
    event_error("Bad gchord");
    gchord_seq[0] = 'z';
    gchord_len[0] = 1;
    seq_len = 1;
  };
  gchord_seq[j] = '\0';
  if (j == 39) {
    event_error("Sequence string too long");
  };
  /* work out unit delay in 1/4 notes*/
  g_num = mtime_num * 4*gchordbars;
  g_denom = mtime_denom * seq_len;
  reduce(&g_num, &g_denom);
/*  printf("%s  %d %d\n",s,g_num,g_denom); */
}

void set_drums(s)
char* s;
/* set up a string which indicates drum pattern */
/* called from dodeferred() */
{
  int seq_len, count, drum_hits;
  char* p;
  int i, j, place;

  p = s;
  count = 0;
  drum_hits = 0;
  seq_len = 0;
  while (((*p == 'z') || (*p == 'd')) && (count<39)) {
    if (*p == 'd') {
      drum_hits = drum_hits + 1;
    };
    drum_seq[count] = *p;
    p = p + 1;
    if ((*p >= '0') && (*p <= '9')) {
      drum_len[count] = readnump(&p);
    } else {
      drum_len[count] = 1;
    };
    seq_len = seq_len + drum_len[count];
    count = count + 1;
  };
  drum_seq[count] = '\0';
  if (seq_len == 0) {
    event_error("Bad drum sequence");
    drum_seq[0] = 'z';
    drum_len[0] = 1;
    seq_len = 1;
  };
  if (count == 39) {
    event_error("Drum sequence string too long");
  };
  /* look for program and velocity specifiers */
  for (i = 0; i<count; i++) {
    drum_program[i] = 35;
    drum_velocity[i] = 80;
  };
  skipspace(&p);
  i = 0;
  place = 0;
  while (isdigit(*p)) {
    j = readnump(&p);
    if (i < drum_hits) {
      while (drum_seq[place] != 'd') {
        place = place + 1;
      };
      if (j > 127) {
        event_error("Drum program must be in the range 0-127");
      } else {
        drum_program[place] = j;
      };
      place = place + 1;
    } else {
      if (i < 2*count) {
        if (i == drum_hits) {
          place = 0;
        };
        while (drum_seq[place] != 'd') {
          place = place + 1;
        };
        if ((j < 1) || (j > 127)) {
          event_error("Drum velocity must be in the range 1-127");
        } else {
          drum_velocity[place] = j;
        };
        place = place + 1;
      };
    };
    i = i + 1;
    skipspace(&p);
  };
  if (i > 2*drum_hits) {
    event_error("Too many data items for drum sequence");
  };
  /* work out unit delay in 1/4 notes*/
  drum_num = mtime_num * 4*drumbars;
  drum_denom = mtime_denom * seq_len;
  reduce(&drum_num, &drum_denom);
}

static void checkbar(pass)
/* check to see we have the right number of notes in the bar */
int pass;
{
  char msg[80];
  
  if (barno >= 0 || barno < 1024) barloc[barno] = tracklen;
  if (barchecking) {
    /* only generate these errors once */
    if (noteson && (partrepno == 0)) {
      /* allow zero length bars for typesetting purposes */
      if ((bar_num-barsize*(bar_denom) != 0) &&
          (bar_num != 0) && ((pass == 2) || (barno != 0))) {
        /* [SS] 2014-11-17 added tracknumber */
        sprintf(msg, "Track %d Bar %d has %d",tracknumber, barno, bar_num);
        if (bar_denom != 1) {
          sprintf(msg+strlen(msg), "/%d", bar_denom);
        };
        sprintf(msg+strlen(msg), " units instead of %d", barsize);
        if (pass == 2) {
          strcat(msg, " in repeat");
        };
        if (quiet == -1) event_warning(msg);
      };
    };
  };
  if (bar_num > 0) {
    barno = barno + 1;
  };
  bar_num = 0;
  bar_denom = 1;
  /* zero place in gchord sequence */
  if (gchordson) {
    if (gchordbars < 2) { /* [SS] 2018-06-23 */
      g_ptr = 0;
      }
    addtoQ(0, g_denom, -1, g_ptr ,0, 0); /* [SS] 2018-06-23 */
    };
  if (drumson) {
    if (drumbars < 2) { /* [SS] 2018-06-23 */
       drum_ptr = 0;
       addtoQ(0, drum_denom, -1, drum_ptr,0, 0);
       }
    addtoQ(0, drum_denom, -1, drum_ptr,0, 0); /* [SS] 2018-06-23 */
  };

}

static void softcheckbar(pass)
/* allows repeats to be in mid-bar */
int pass;
{
  if (barchecking) {
    if ((bar_num-barsize*(bar_denom) >= 0) || (barno <= 0)) {
      checkbar(pass);
    };
  };
}

static void save_state(vec, a, b, c, d, e, f)
/* save status when we go into a repeat */
int vec[6];
int a, b, c, d, e, f;
{
  vec[0] = a;
  vec[1] = b;
  vec[2] = c;
  vec[3] = d;
  vec[4] = e;
  vec[5] = f; /* [SS] 2013-11-02 */
}

static void restore_state(vec, a, b, c, d, e, f)
/* restore status when we loop back to do second repeat */
int vec[6];
int *a, *b, *c, *d, *e, *f;
{
  *a = vec[0];
  *b = vec[1];
  *c = vec[2];
  *d = vec[3];
  *e = vec[4];
  *f = vec[5]; /* [SS] 2013-11-02 */
}

/* 2015-03-16 [SS] changed channels[] to channel_in_use[] */
static int findchannel()
/* work out next available channel */
{
  int j;

  j = 0;
  while ((j<MAXCHANS) && (channel_in_use[j] != 0)) {
    j = j + 1;
  };
  if (j >= MAXCHANS && !no_more_free_channels) {
    event_error("All 16 MIDI channels used up.");
    no_more_free_channels = 1;
    j = 0;
  };
  channel_in_use[j] = 1;
  return (j);
}





static void fillvoice(partno, xtrack, voice)
/* check length of this voice at the end of a part */
/* if it is zero, extend it to the correct length */
int partno, xtrack, voice;
{
  char msg[100];
  long now;

 if (partlabel <-1 || partlabel >25) printf("genmidi.c:fillvoice partlabel %d out of range\n",partlabel);

  now = tracklen + delta_time;
  if (partlabel == -1) {
    if (xtrack == 1) {
      introlen = now;
    } else {
      if (now == 0) {
        delta_time = delta_time + introlen;
        now = introlen;
      } else {
        if (now != introlen) {
          sprintf(msg, "Time 0-%ld voice %d, has length %ld", 
                  introlen, voice, now);
          event_error(msg);
        };
      };
    };
  } else {
    if (xtrack == 1) {
      partlen[partlabel] = now - lastlen;
    } else {
      if (now - lastlen == 0) {
        delta_time = delta_time + partlen[partlabel];
        now = now + partlen[partlabel];
      } else {
        if (now - lastlen != partlen[partlabel]) {
          sprintf(msg, "Time %ld-%ld voice %d, part %c has length %ld", 
                  lastlen, lastlen+partlen[partlabel], voice, 
                  (char) (partlabel + (int) 'A'), 
                  now-lastlen);
          event_error(msg);
        };
      };
    };
  };
  lastlen = now;
}

static int findpart(j)
int j;
/* find out where next part starts and update partno */
{
  int place;

  place = j;
  partno = partno + 1;
  if (partno < parts) {
    partlabel = (int)part.st[partno] - (int)'A';
  }
  while ((partno < parts) &&
         (part_start[partlabel] == -1)) {
    if (!silent) event_error("Part not defined");
    partno = partno + 1;
    if (partno < parts) {
      partlabel = (int)part.st[partno] - (int)'A';
    }
  };
  if (partno >= parts) {
    place = notes;
  } else {
    partrepno = part_count[partlabel];
    part_count[partlabel]++;
    place = part_start[partlabel];
  };
  if (verbose) {
    if (partno < parts) {
      printf("Doing part %c number %d of %d\n", part.st[partno], partno, parts);
    };
  };
  return(place);
}

static int partbreak(xtrack, voice, place)
/* come to part label in note data - check part length, then advance to  */
/* next part if there was a P: field in the header */
int xtrack, voice, place;
{
  int newplace;

  newplace = place;
  if (dependent_voice[voice]) return newplace;
  if (xtrack > 0) {
    fillvoice(partno, xtrack, voice);
  };
  if (parts != -1) {
    /* go to next part label */
    newplace = findpart(newplace);
  };
  partlabel = (int) pitch[newplace] - (int)'A';
  return(newplace);
}

static int findvoice(initplace, voice, xtrack)
/* find where next occurrence of correct voice is */
int initplace;
int voice, xtrack;
{
  int foundvoice;
  int j;

  foundvoice = 0;
  j = initplace;
  while ((j < notes) && (foundvoice == 0)) {
    if (feature[j] == LINENUM) { /* [SS] 2019-03-14 */
      lineno = pitch[j];
      }
    if (feature[j] == PART) {
      j = partbreak(xtrack, voice, j);
      if (voice == 1) {
        foundvoice = 1;
      } else {
        j = j + 1;
      };
    } else {
      if ((feature[j] == VOICE) && (pitch[j] == voice)) {
        foundvoice = 1;
      } else {
        j = j + 1;
      };
    };
  };
  return(j);
}

static void text_data(s)
/* write text event to MIDI file */
char* s;
{   
  mf_write_meta_event(delta_time, text_event, s, strlen(s));
  tracklen = tracklen + delta_time;
  delta_time = 0L;
}

static void karaokestarttrack (track)
int track;
/* header information for karaoke track based on w: fields */
{
  int j;
  int done;
  char atitle[200];

/*
 *  Print Karaoke file headers in track 0.
 *  @KMIDI KARAOKE FILE - Karaoke midi file marker)
 */
   if (track == 0)
   {
      text_data("@KMIDI KARAOKE FILE");
   }
/*
 *  Name track 2 "Words" for the lyrics track.
 *  @LENGL - language
 *  Print @T information.
 *  1st @T line signifies title.
 *  2nd @T line signifies author.
 *  3rd @T line signifies copyright.
 */
   if (track == 2)
   {
      mf_write_meta_event(0L, sequence_name, "Words", 5);
      kspace = 0;
      text_data("@LENGL");
      strcpy(atitle, "@T");
   }
   else 
/*
 *  Write name of song as sequence name in track 0 and as track 1 name. 
 *  Print general information about the file using @I marker.
 *  Add to tracks 0 and 1 for various Karaoke (and midi) players to find.
 */
      strcpy(atitle, "@I");

  j = 0;
  done = 3;
     
  while ((j < notes) && (done > 0))
  {
     j = j+1;
     if (feature[j] == TITLE) {
        if (track != 2)
           mf_write_meta_event(0L, sequence_name, atext[pitch[j]], strlen (atext[pitch[j]]));
        strcpy(atitle+2, atext[pitch[j]]);
        text_data(atitle);
        done--;
     }
     if (feature[j] == COMPOSER) {
        strcpy(atitle+2, atext[pitch[j]]);
        text_data(atitle);
        done--;
     }     
     if (feature[j] == COPYRIGHT) {
        strcpy(atitle+2, atext[pitch[j]]);
        text_data(atitle);
        done--;
     }
  }
}

static int findwline(startline)
int startline;
/* Find next line of lyrics at or after startline. */
{
  int place;
  int done;
  int newwordline;
  int inwline, extending;
  int versecount, target;

/*   printf("findwline called with %d\n", startline); */
  done = 0;
  inwline = 0;
  nowordline = 0;
  newwordline = -1;
  target = partrepno;
  if (startline == thismline) {
    versecount = 0;
    extending = 0;
  } else {
    versecount = target;
    extending = 1;
  };
  if (thismline == -1) {
    event_error("First lyrics line must come after first music line");
  } else {
    place = startline + 1;
    /* search for corresponding word line */
    while ((place < notes) && (!done)) {
      switch (feature[place]) {
      case WORDLINE:
        inwline = 1;
        /* wait for words for this pass */
        if (versecount == target) {
          thiswfeature = place;
          newwordline = place;
          windex = pitch[place];
          wordlineplace = 0;
          done = 1;
        };
        break;
      case WORDSTOP:
        if (inwline) {
          versecount = versecount + 1;
        };
        inwline = 0;
        /* stop if we are part-way through a lyric set */
        if  (extending) {
          done = 1;
        };
        break;
      case PART:
        done = 1;
        break;
      case VOICE:
        done = 1;
        break;
      case MUSICLINE:
        done = 1;
        break;
      default:
        break;
      };
      place = place + 1;
      if (done && (newwordline == -1) && (versecount > 0) && (!extending)) {
        target = partrepno % versecount ;
        versecount = 0;
        place = startline+1;
        done = 0;
        inwline = 0;
      };
    };
    if (newwordline == -1) {
      /* remember that we couldn't find lyrics */
      nowordline = 1;
      if (lyricsyllables == 0) {
        event_warning("Line of music without lyrics");
      };
    };  
  };
  return(newwordline);
}



static int getword(place, w)
/* picks up next syllable out of w: field.
 * It strips out all the control codes ~ - _  * in the
 * words and sends each syllable out to the Karaoke track.
 * Using the place variable, it loops through each character
 * in the word until it encounters a space or next control
 * code. The syllstatus variable controls the loop. After,
 * the syllable is sent, it then positions the place variable
 * to the next syllable or control code.
 * inword   --> grabbing the characters in the syllable and
 *             putting them into syllable for output.
 * postword --> finished grabbing all characters
 * foundnext--> ready to repeat process for next syllable
 * empty    --> between syllables.
 *
 * The variable i keeps count of the number of characters
 * inserted into the syllable[] char for output to the
 * karaoke track. The kspace variables signals that a
 * space was encountered.
 */
int* place;
int w;
{
  char syllable[200];
  unsigned char c; /* [BY] 2012-10-03 */
  int i;
  int syllcount;
  enum {empty, inword, postword, foundnext, failed} syllstatus;
  /* [BY] 2012-10-03  Big5 chinese character support */
  int isBig5; /* boolean check for first byte of Big-5: 0xA140 ~ 0xF9FE */

  /*printf("GETWORD: w = %d\n",c);*/
  i = 0;
  syllcount = 0;
  if (w >= wcount) {
    syllable[i] = '\0';
    return ('\0');
  };
  if (*place == 0) {
    if ((w % 2) == 0) {
      syllable[i] = '/'; 
    } else {
      syllable[i] = '\\'; 
    };
    i = i + 1;
  };
  if (kspace) {
    syllable[i] = ' ';
    i = i + 1;
  };
  syllstatus = empty;
  c = *(words[w]+(*place));
  isBig5 = 0;  /* [BI] 2012-10-03 */
  while ((syllstatus != postword) && (syllstatus != failed)) {
  syllable[i] = c;
    /* printf("syllstatus = %d c = %c i = %d place = %d row= %d \n",syllstatus,c,i,*place,w); */
	if (isBig5) { /* [BI] 2012-10-03 */
      i = i + 1;
      *place = *place + 1;
	  isBig5 = 0;
	} else {
    switch(c) {
    case '\0':
      if (syllstatus == empty) {
        syllstatus = failed;
      } else {
        syllstatus = postword;
        kspace = 1;
      };
      break;
    case '~':
      syllable[i] = ' ';
      syllstatus = inword;
      *place = *place + 1;
      i = i + 1;
	  hyphenstate = 0; /* [Bas Schoutsen] 2010-04-08 */
      break;
    case '\\':
      if (*(words[w]+(*place+1)) == '-') {
        syllable[i] = '-';
        syllstatus = inword;
        *place = *place + 2;
        i = i + 1;
      } else {
        /* treat like plain text */
        *place = *place + 1;
        if (i>0) {
          syllstatus = inword;
          i = i + 1;
        };
      };
      break;
    case ' ':
      if (syllstatus == empty) {
        *place = *place + 1;
      } else {
        syllstatus = postword;
        *place = *place + 1;
        kspace = 1;
      };
      break;
    case '-':
	if (hyphenstate == 1) {  /* [Bas Schoutsen 2010-04-08 */
		i = i + 1; syllstatus = postword; *place = *place + 1;
		break;
		
	  }
	if (syllstatus == inword) {
        syllstatus = postword;
        *place = *place + 1;
        kspace = 0;
      } else {
        *place = *place + 1;
      };
	  hyphenstate = 1;
      break;
    case '*':
      if (syllstatus == empty) {
        syllstatus = postword;
        *place = *place + 1;
      } else {
        syllstatus = postword;
      };
	  hyphenstate = 0;
      break;
    case '_':
      if (syllstatus == empty) {
        syllstatus = postword;
        *place = *place + 1;
      } else {
        syllstatus = postword;
      };
	  hyphenstate = 0;
      break;
    case '|':
      if (syllstatus == empty) {
        syllstatus = failed;
        *place = *place + 1;
      } else {
        syllstatus = postword;
        *place = *place + 1;
        kspace = 1;
      };
      waitforbar = 1;
	  hyphenstate = 0;
      break;
    default:
      /* copying plain text character across */
      /* first character must be alphabetic */
	  hyphenstate = 0;
          /* [BI] 2012-10-03 */
	  if (c >= 0xA1) {	/* 0xA1, 161 */
		isBig5 = 1;
	  };
      if ((i>0) || isalpha(syllable[0]) || (c >= 0xA1)) {
        syllstatus = inword;
        i = i + 1;
      };
      *place = *place + 1;
      break;
    };
};
    c = *(words[w]+(*place));
  };
  syllable[i] = '\0';
  if (syllstatus == failed) {
    syllcount = 0;
  } else {
    syllcount = 1;
    if (strlen(syllable) > 0) {
      text_data(syllable);
      /*printf("TEXT DATA %s\n",syllable);*/
    };
  };
  /* now deal with anything after the syllable */
  while ((syllstatus != failed) && (syllstatus != foundnext)) {
    c = *(words[w]+(*place));
    /*printf("next character = %c\n",c);*/
    switch (c) {
    case ' ':
      *place = *place + 1;
      break;
    case '-':
      *place = *place + 1; 
      kspace = 0;
      syllcount = syllcount + 1; /* [SS] 2011-02-23 */
      break;
    case '\t':
      *place = *place + 1;
      break;
    case '_':
      *place = *place + 1;
      syllcount = syllcount + 1;
      break;
    case '|':
      if (waitforbar == 0) {
        *place = *place + 1;
        waitforbar = 1;
      } else {
        syllstatus = failed;
      };
      break;
    default:
      syllstatus = foundnext;
      break;
    };  
    /* printf("now place = %d syllcount = %d syllstatus = %d\n",*place,syllcount,syllstatus); */
  };
  return(syllcount);
}




static void write_syllable(place)
int place;
/* Write out a syllable. This routine must check that it has a line of 
 * lyrics and find one if it doesn't have one. The function is called
 * for each note encountered in feature[j] when the global variable
 * wordson is set. The function keeps count of the number of notes
 * in the music and words in the lyrics so that we can check that 
 * they match at the end of a music line. When waitforbar is set
 * by getword, the function  does nothing (allows feature[j]
 * to advance to next feature) until waitforbar is set to 0
 * (by writetrack).                                                 */
{
  musicsyllables = musicsyllables + 1;
  if (waitforbar) {
    lyricsyllables = lyricsyllables + 1;
    return;
  };
  if ((!nowordline) && (!waitforbar)) {
    if (thiswline == -1) {
      thiswline = findwline(thismline);
    };
    if (!nowordline) {
      int done;

      done = 0;
      while (!done) {
        if (syllcount == 0) {
          /* try to get fresh word */
          syllcount = getword(&wordlineplace, windex);
          if (waitforbar) {
            done = 1;
            if (syllcount == 0) {
              lyricsyllables = lyricsyllables + 1;
            };
          } else {
            if (syllcount == 0) {
              thiswline = findwline(thiswline);
              if (thiswline == -1) {
                done = 1;
              };
            };
          };
        };
        if (syllcount > 0) {
          /* still finishing off a multi-syllable item */
          syllcount = syllcount - 1;
          lyricsyllables = lyricsyllables + 1;
          done = 1;
        };
      };
    };
  };
}

int onemorenote; /* [Bas Schoutsen] 2010-04-08 */

static void checksyllables()
/* check line of lyrics matches line of music. It grabs
 * all remaining syllables in the lyric line counting
 * them as it goes along. It then checks that the number
 * of syllables matches the number of notes for that music
 * line
*/
{
  int done;
  int syllcount;
  char msg[80];

  /* first make sure all lyric syllables are read */
  done = 0;
  while (!done) {
    syllcount = getword(&wordlineplace, windex);
    if (syllcount > 0) {
      lyricsyllables = lyricsyllables + syllcount;
    } else {
      thiswline = findwline(thiswline);
      if (thiswline == -1) {
        done = 1;
      } else {
        windex = pitch[thiswline];
      };
    };
  };
  if (onemorenote == 1){  /* [Bas Schoutsen] 2010-04-08 */
	lyricsyllables = lyricsyllables + 1;
  }  
  if (lyricsyllables != musicsyllables) {
    sprintf(msg, "Verse %d mismatch;  %d syllables in music %d in lyrics",
                partrepno+1, musicsyllables, lyricsyllables);
    if (verbose) event_error(msg); /* [SS] 2012-04-15 */
  };
  if (onemorenote == 1){  /* [Bas Schoutsen] 2010-04-08 */
	onemorenote = 0;
  /*printf("onemorenote please, hyphenstate to zero\n (using lyric- instead of note-hyphen)\n"); //not the most elegant solution.. but it works */
  hyphenstate = 0;
  }
  lyricsyllables = 0;
  musicsyllables = 0;
}

static int inlist(place, passno)
int place;
int passno;
/* decide whether passno matches list/number for variant section */
/* handles representation of [X in the abc */
{
  int a, b;
  char* p;
  int found;
  char msg[100];

  /* printf("passno = %d\n", passno); */
  if (denom[place] != 0) {
    /* special case when this is variant ending for only one pass */
    if (passno == denom[place]) {
      return(1);
    } else {
      return(0);
    };
  } else {
    /* must scan list */
    p = atext[pitch[place]];
    found = 0;
    while ((found == 0) && (*p != '\0')) {
      if (!isdigit(*p)) {
        sprintf(msg, "Bad variant list : %s", atext[pitch[place]]);
        event_error(msg);
        found = 1;
      };
      a = readnump(&p);
      if (passno == a) {
        found = 1;
      };
      if (*p == '-') {
        p = p + 1;
        b = readnump(&p);
        if ((passno >= a) && (passno <= b)) {
          found = 1;
        };
      };
      if (*p == ',') {
        p = p + 1;
      };
    };
    return(found);
  };
}

void set_meter(n, m)
/* set up variables associated with meter */
int n, m;
{
  mtime_num = n;
  mtime_denom = m;
  time_num =n; 
  time_denom=m; 
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

static void write_meter(n, m)
/* write meter to MIDI file */
int n, m;
{
  int t, dd;
  char data[4];

  set_meter(n, m);

  dd = 0;
  t = m;
  while (t > 1) {
    dd = dd + 1;
    t = t/2;
  };
  data[0] = (char)n;
  data[1] = (char)dd;
  if (n%2 == 0) {
    data[2] = (char)(24*2*n/m);
  } else {
    data[2] = (char)(24*n/m);
  };
  data[3] = 8;
/*if (noteson)  [SS] 2010-04-21 2010-07-06
  mf_write_meta_event(delta_time, time_signature, data, 4); 
 [SS] 2010-04-15 
else
*/
  mf_write_meta_event(0L, time_signature, data, 4); /* [SS] 2010-04-15 2010-07-06*/
}

static void write_keysig(sf, mi)
/* Write key signature to MIDI file */
int sf, mi;
{
  char data[2];

  data[0] = (char) (0xff & sf);
  data[1] = (char) mi;
  mf_write_meta_event(0L, key_signature, data, 2);
}

static void midi_noteon(delta_time, pitch, pitchbend, chan, vel)
/* write note on event to MIDI file */
long delta_time;
int pitch, chan, vel, pitchbend;
{
  char data[2];
#ifdef NOFTELL
  extern int nullpass;
#endif
  if (channel >= MAXCHANS) {
    event_error("Channel limit exceeded");
  } else {
  if(pitchbend < 0 || pitchbend > 16383) {
      event_error("Internal error concerning pitch bend on note on.");
    }

   if(pitchbend != current_pitchbend[channel] && chan != 9) {
      data[0] = (char) (pitchbend&0x7f);
      data[1] = (char) ((pitchbend>>7)&0x7f);
      bendstate = pitchbend; /* [SS] 2014-09-09 */
      mf_write_midi_event(delta_time,pitch_wheel,chan,data,2);
      delta_time=0;
      current_pitchbend[channel] = pitchbend;
    }


    if (chan == 9) data[0] = (char) drum_map[pitch];
    else           data[0] = (char) pitch;
    data[1] = (char) vel;

    mf_write_midi_event(delta_time, note_on, chan, data, 2);
  };
}

void midi_noteoff(delta_time, pitch, chan)
/* write note off event to MIDI file */
long delta_time;
int pitch, chan;
{
  char data[2];

  if (chan == 9) data[0] = (char) drum_map[pitch];
  else           data[0] = (char) pitch;
  data[1] = (char) 0;
  if (channel >= MAXCHANS) {
    event_error("Channel limit exceeded\n");
  } else {
    mf_write_midi_event(delta_time, note_off, chan, data, 2);
  };
}

static void noteon_data(pitch, pitchbend, channel, vel)
/* write note to MIDI file and adjust delta_time */
int pitch, pitchbend, channel, vel;
{
  midi_noteon(delta_time, pitch, pitchbend, channel, vel);
  tracklen = tracklen + delta_time;
  delta_time = 0L;
}

/* [SS] 2012-04-01 */
static void midi_re_tune (int channel) {
/* changes the master coarse tuning and master fine tuning
   using Register Parameter Number (RPN) for a specific
   track. See http://home.roadrunner.com/~jgglatt/tech/midispec/rpn.htm
   or http://www.2writers.com/eddie/TutNrpn.htm for a tutorial on how
   this is done.
*/
char data[2];
data[0] = (char) (bend & 0x7f); /* least significant bits */
data[1] = (char) ((bend >>7) & 0x7f);
/* indicate that we are applying RPN fine and gross tuning using
   the following two control commands. 
   control 101 0  
   control 100 1 */
data[0] = 101; /* RPN command */
data[1] = 0;   /* type of command */
write_event(control_change, channel, data, 2);
data[0] = 100; /* RPN command */
data[1] = 1;   /* type of command */
write_event(control_change, channel, data, 2);
/* now enter the bend parameters using the control data entry
   commands for the least significant and most significant bits
*/
data[0] = 6; /* control data entry for coarse bits */
data[1] = (char) ((bend >>7) & 0x7f);
write_event(control_change, channel, data, 2);

data[0] = 38; /* control data entry for fine bits */
data[1] = (char) (bend & 0x7f); /* least significant bits */
write_event(control_change, channel, data, 2);
} 



/* [SS] 2011-07-04 */
static void note_beat(int n, int *vel) {
  /* set velocity */
  int i;
  if(beataccents == 0) 
    *vel = mednote;
  else if (nbeats > 0) {
      if ((bar_num*nbeats)%(bar_denom*barsize) != 0) {
        /* not at a defined beat boundary */
        *vel = softnote;
      } else {
        /* find place in beatstring */
        i = ((bar_num*nbeats)/(bar_denom*barsize))%nbeats;
        switch(beatstring[i]) {
        case 'f':
        case 'F':
          *vel = loudnote;
          break;
        case 'm':
        case 'M':
          *vel = mednote;
          break;
        default:
        case 'p':
        case 'P':
          *vel = softnote;
          break;
        };
      };
     } else {
      /* no beatstring - use beat algorithm */
      if (bar_num == 0) {
          *vel = loudnote;
     } else {
        if ((bar_denom == 1) && ((bar_num % beat) == 0)) {
          *vel = mednote;
     } else {
          *vel = softnote;
        };
      };
    }
  }


/* [SS] 2011-07-04  2011-08-17*/

void stress_factors (int n, int *vel) {
  if (beatmodel == 2) {
       *vel = stressvelocity[n];
       } else
  articulated_stress_factors (n, vel);
  }  


void articulated_stress_factors (int n,  int *vel)
/* computes the Phil Taylor stress factors for a note
   positioned between begnum/begden and endnum/endden.
   The segment size is resnum/resden.
   Method compute the segments that are overlapped by
   the note and average the segments parameters.
*/
{ int begnum,begden;
  int stepnum, stepden;
  int firstsegnum,firstsegden;
  int lastsegnum,lastsegden;
  int firstseg,lastseg;
  int firstsegrem,lastsegrem; /* remainders */
  int endnum,endden;
  int i;
  int gain;
  float dur;
  float segsize,segrange;
  int tnotenum,tnotedenom;

  stepnum = num[n];
  stepden = denom[n];
/* undo the b_num/b_denom application in addunits() */
/* note b_num/b_denom defined in set_meter() has nothing to do
   with L: unit length */
  begnum = bar_num*b_denom;
  begden = bar_denom*b_num;

  endnum =  begnum*stepden + begden*stepnum;
  endden =  stepden*begden;
  reduce (&endnum,&endden);
  /* determine the segment number by dividing by the segment size
   * and truncating the result.
   * firstseg = integer (begnum/begden divided by segnum/segden) */
  begnum = begnum;
  begden = begden;
  firstsegnum = begnum*segden;
  firstsegden = begden*segnum*4;
  reduce (&firstsegnum,&firstsegden);
/* note coordinates are in quarter note units so divide by 4 */
  firstseg = firstsegnum/firstsegden;
  firstsegrem = firstsegnum % firstsegden;
  /* lastseg = integer (endnum/endden divided by resnum/resden) */
  endnum = endnum;
  endden = endden;
  lastsegnum = endnum*segden;
  lastsegden = endden*segnum*4;
  reduce(&lastsegnum,&lastsegden);
  /* scale down the fraction endnum/endden so that we do avoid the
     next segment unless endnum/endden is large enough */
  lastseg = lastsegnum/lastsegden;
  lastsegrem = lastsegnum%lastsegden;
  if (lastseg > nseg) return; /* do nothing if note extends beyond bar */
  if (lastseg - firstseg > 2) return; /* do nothing if note extends over 3 segments */ 
  dur=0.0;
  segrange=0.0;
/* now that we know the segment span of the notes average fdur 
 * over those segments. [SS] 2011-08-25 */
  if (lastseg == firstseg) { 
    dur = fdur[firstseg];} /*note is included is entirely included in segment*/
  else {  /* note may overlap more than one segment */
    for (i=firstseg;i < lastseg;i++) {
      if (i == firstseg) { /* for the first segment */
        if (firstsegrem == 0) {dur  = fdur[i];
                               segsize = (float) 1.0;
                               segrange = segsize;
                              } /* note starts at beginning of segment*/
        else { /* note starts in the middle of segment */
              segsize = (float) (firstsegden - firstsegrem) / (float) firstsegden;
              segrange = segsize; 
              dur = fdur[i]*segsize;
              }
     } else {
        dur = dur + fdur[i]; /* for other segments that note overlaps */
        segrange += (float) 1.0;  
       }
     }

    if (lastsegrem != 0) {
             segsize = (float)  lastsegrem / (float) lastsegden;
             dur = dur +  fdur[lastsegrem] *  segsize;
             segrange = segrange + segsize;
             }
  dur = dur/segrange;
  } /* end of note may overlap more than one segment */

 /* gain is set to the value of ngain[] in the first segment.*/
  gain = ngain[firstseg]; /* [SS] 2011-08-17 */
  dur = dur/maxdur;
/* since we can't lengthen notes we shorten them based on the maximum*/

  if (verbose > 1) {
    printf ("%d %d/%d = %d/%d to  %d/%d = %d/%d",pitch[n],begnum,begden,firstsegnum,firstsegden,endnum,endden,lastsegnum,lastsegden);
  printf(" dur gain = %f %d\n",dur,gain);}
/* tnotenum and tnotedenom is used for debugging only.*/
  tnotenum = (int) (0.5 +dur*100.0);
  tnotedenom = 100;
  *vel = gain;
/* compute the trim values that are applied and the end of the NOTE:
 * block in the writetrack() switch complex.*/
  trim_num = (int) ((float) num[n]*100.0*(1.0 - dur));
  trim_denom = (int) ((float) denom[n]* (float) 100.0); /* [SS] 2015-10-08 extra parentheses */
  /*printf("dur = %f %d/%d %d/%d gain = %d\n",dur,tnotenum,tnotedenom,trim_num,trim_denom,gain);*/
 }

/* [SS] 2015-09-07 */
int apply_velocity_increment_for_one_note (velocity)
    int velocity;
    {
    velocity = velocity + single_velocity_inc;
    if (velocity < 0) velocity = 0;
    if (velocity > 127) velocity = 127;
    single_velocity_inc = 0; 
    return velocity;
    }

int set_velocity_for_one_note ()
    {
    int velocity;
    velocity = single_velocity;
    single_velocity = -1;
    return velocity;
    } 


/* [SS] 2011-07-04 */
static void noteon(n)
/* compute note data and call noteon_data to write MIDI note event */
int n;
{
  int  vel;
  vel = 0; /* [SS] 2013-11-04 */
  if (beatmodel != 0)  /* [SS] 2011-08-17 */
     stress_factors (n,   &vel);
  else note_beat(n,&vel);
  if (vel == 0) note_beat(n,&vel); /* [SS] 2011-09-06 */

  /* [SS] 2015-09-07 */
  if (single_velocity >= 0)
     vel = set_velocity_for_one_note();
  else if (single_velocity_inc != 0)
     vel = apply_velocity_increment_for_one_note (vel);

  if (channel == 9) noteon_data(pitch[n],bentpitch[n],channel,vel);
  else noteon_data(pitch[n] + transpose + global_transpose, bentpitch[n], channel, vel);
}

static void write_program(p, channel)
/* write 'change program' (new instrument) command to MIDI file */
int p, channel;
{
  char data[1];

  p = p - programbase; /* [SS] 2017-06-02 */
  if (p <0) p = 0; /* [SS] 2017-06-02 */
  data[0] = p;
  if (channel >= MAXCHANS) {
    event_error("Channel limit exceeded\n");
  } else {
    mf_write_midi_event(delta_time, program_chng, channel, data, 1);
  };
  tracklen = tracklen + delta_time;
  delta_time = 0L;
}

/* [SS] 2015-07-27 */
void write_event_with_delay(delta,event_type, channel, data, n)
/* write MIDI special event such as control_change or pitch_wheel */
int delta;
int event_type;
int channel, n;
char data[];
{
  if (channel >= MAXCHANS) {
    event_error("Channel limit exceeded\n");
  } else {
    mf_write_midi_event(delta, event_type, channel, data, n); 
  };
}

void write_event(event_type, channel, data, n)
/* write MIDI special event such as control_change or pitch_wheel */
int event_type;
int channel, n;
char data[];
{
  if (channel >= MAXCHANS) {
    event_error("Channel limit exceeded\n");
  } else {
    /*mf_write_midi_event(delta_time, event_type, channel, data, n);  [SS] 2011-10-21 */
    mf_write_midi_event(0, event_type, channel, data, n);
  };
}

static char *select_channel(chan, s)
char *s;
int *chan;
/* used by dodeferred() to set channel to be used */
/* reads 'bass', 'chord' or nothing from string pointed to by p */
{
  char sel[40];
  char *p;

  p = s;
  skipspace(&p);
  *chan = channel;
  if (isalpha(*p)) {
    readstr(sel, &p, 40);
    skipspace(&p);
    if (strcmp(sel, "bass") == 0) {
      *chan = fun.chan;
    };
    if (strcmp(sel, "chord") == 0) {
      *chan = gchord.chan;
    };
  };
  return(p);
}

static int makechordchannels (n)
int n;
{
/* Allocate channels for in voice chords containing microtones.
   n is the number of channels to allocate which should be
   less than 10.
 */
int i,chan;
int prog;
/* [SS] 2015-05-17 do not make chord channels for track 0 if multi track */
if (ntracks != 1 && tracknumber == 0) return 0; /* [SS] 2015-08-06 */
if (n < 1) return -1;
if (n > 9) n = 9;
prog = current_program[channel];
chordchannels[0] = channel; /* save active channel number */
if (verbose >1) printf("making %d chord channels\n",n);
for (i=1; i<=n; i++) {
  chan = findchannel();
  chordchannels[i] = chan;
  write_program(prog, chan);
  }
nchordchannels = n;
return n;
}

int parse_stress_params (char *input) {
  char *next;
  float f;
  int n;
  int success;
  if (verbose > 1) printf("parsing stress parameters\n"); /* [SS] 2018-04-14 */
  success = 0;
  f =(float) strtod (input,&next);
  input = next;
  if (*input == '\0') {return -1;} /* no data, probably file name */
  if (f == 0.0) {return -1;} /* no data, probably file name */
  nseg = (int) (f +0.0001); /* [SS] 2015-10-08 extra parentheses */
  for (n=0;n<32;n++) {fdur[n]= 0.0; ngain[n] = 0;}
  if (nseg > 31) {return -1;} 
  n = 0;
  while (*input != '\0' && n < nseg) {
    f = (float) strtod (input,&next);
    ngain[n] = (int) (f + 0.0001); /* [SS] 2015-10-08 extra parentheses */
    if (ngain[n] > 127 || ngain[n] <0) {
        printf("**error** bad velocity value ngain[%d] = %d in ptstress command\n",n,ngain[n]);
        }
    input = next;
    f = (float) strtod (input,&next);
    fdur[n] = f;
    if (fdur[n] > (float) nseg || fdur[n] < 0.0) {
       printf("**error** bad expansion factor fdur[%d] = %f in ptstress command\n",n,fdur[n]);
       }
    input = next;
    n++;
    }
if (n != nseg) return -1;
else {
  beatmodel = 2; /* [SS] 2018-04-14 */
  barflymode = 1; /* [SS] 2018-04-16 */
  return 0;
  }
} 

/* [SS] 2011-07-04 */
void readstressfile (char * filename)
{
FILE *inputhandle;
int n;
int idummy;
maxdur = 0;
inputhandle = fopen(filename,"r");
if (inputhandle == NULL) {
    printf("Failed to open file %s\n", filename);
    return;
    }
for (n=0;n<32;n++) {fdur[n]= 0.0; ngain[n] = 0;}
fdursum[0]=fdur[0];
beatmodel = 2; /* for Phil Taylor's stress model */
idummy = fscanf(inputhandle,"%d",&nseg);
/*printf("%d\n",nseg);*/
if (nseg > 31) nseg=31;

for (n=0;n < nseg+1;n++) {
  idummy =  fscanf(inputhandle,"%d %f",&ngain[n],&fdur[n]);
  if (verbose) printf("%d %f\n",ngain[n],fdur[n]);
  }
fclose(inputhandle);
}

/* [SS] 2011-09-07 */
void calculate_stress_parameters () {
int qnotenum,qnoteden;
int n;
float lastsegvalue;
segnum = time_num;
segden = time_denom*nseg;
reduce(&segnum,&segden);
/* compute number of segments in quarter note */
qnotenum = segden;
qnoteden = segnum*4;
reduce(&qnotenum,&qnoteden);
if (verbose > 1) printf("segment size set to %d/%d\n",segnum,segden);
for (n=0;n<nseg+1;n++) {
  maxdur = MAX(maxdur,fdur[n]);
  if (n > 0) fdursum[n] = fdursum[n-1]+fdur[n-1]*(float) qnoteden /(float) qnotenum;
  if (fdursum[n] > (float) nseg + 0.05) {
     printf("**error** bad stress file: sum of the expansion factors exceeds number of segments\nAborting stress model\n");
     beatmodel = 0;
     return;
     }
  if (ngain[n] > 127 || ngain[n] < 0) {  
     printf("**error** bad stress file: note velocity not between 0 and 127\n Aborting the stress model\n");
     beatmodel = 0;
     return;
     }
/* num[],denom[] use quarter note units = 1/1, segment units are usually
   half that size, so we divide by 2.0
*/
  if (verbose > 1) printf ("%f  ",fdursum[n]);
  }
  if (verbose > 1) printf(" == fdursum\n");
/*printf("maxdur = %f\n",maxdur);*/
/*if (fdursum[nseg] != (float) nseg) fdursum[nseg] = (float) nseg; [SS] 2011-09-06 */
lastsegvalue = (float) nseg * (float) qnoteden / (float) qnotenum;
/* ensure fdursum[nseg] = lastsegvalue [SS] 2011-09-06 */
if (fdursum[nseg] != lastsegvalue)
  {printf("**warning** the sum of the expansion factors is not %d\n some adjustments are made.\n",nseg);
  fdursum[nseg] = lastsegvalue;
  }
}


/* [SS] 2011-08-17 */
void fdursum_at_segment(int segposnum, int segposden, int *val_num, int *val_den)
{
int inx0,inx1,remainder;
float val,a0,a1;
*val_num = 0;
inx0 = segposnum/segposden;
if (inx0 > nseg) {
   *val_num = *val_num + (int) ((float) 1000.0*fdursum[nseg]); /* [SS] 2015-10-08 extra parentheses */
    /*inx0 = inx0 - nseg;  [SS] 2013-06-07*/
    inx0 = inx0 % nseg;  /* [SS] 2013-06-07 */
   }
inx1 = inx0+1;
remainder = segposnum % segposden;
if (remainder == 0) {
    val = fdursum[inx0];
   } else {
    if (inx1 > nseg) {
       printf("***fdursum_at_segment: inx1 = %d too large\n",inx1);
       }
       
    a0 = (float) remainder / (float) segposden;
    a1 =(float) 1.0 - a0; 
    val = a1*fdursum[inx0] + a0*fdursum[inx1];
    }
 *val_num  += (int) (1000.0 * val +0.5);
 *val_den = 1000; 
 reduce(val_num,val_den);
}

/* [SS] 2020-08-09 */
static void expand_array (shortarray, size, longarray, factor)
/* if shortarray = {21,-40} and factor = 4
 * longarray will be {5,6,5,5,-10,-10,-10,-10}
 * It is used for smoothing a bendstring.
 */
int shortarray[], longarray[];
int size;
int factor;
{
float increment, accumulator;
float last_accumulator;
int i,j,k;
if (size*factor > 256) {printf("not enough room in bendstring\n");
	                return;
                       }
j = 0;
for (i = 0; i< size; i++) {
  increment = (float) shortarray[i]/factor;
  accumulator = 0.0;
  last_accumulator = 0.0;
  for (k = 0; k < factor; k++) {
    accumulator += increment;
    if (increment > 0)
      longarray[j] = (int) (accumulator + 0.5) - (int) (last_accumulator + 0.5);
    else 
      longarray[j] = (int) (accumulator - 0.5) - (int) (last_accumulator - 0.5);
    last_accumulator = accumulator;
    j++;
    }
  }
}


static void dodeferred(s,noteson)
/* handle package-specific command which has been held over to be */
/* interpreted as MIDI is being generated */
char* s;
int noteson;
{
  char* p;
  char command[40];
  /* char inputfile[256];  [SS] 2011-07-04 [SDG] 2020-06-04*/
  int done;
  int val;
  int i;
  int bendinput[64]; /* [SS] 2020-08-09 */

  p = s;
  skipspace(&p);
  readstr(command, &p, 40);
  skipspace(&p);
  done = 0;

  if (verbose>1)
       printf("dodeferred: track = %d cmd = %s\n",tracknumber,command);

  if (strcmp(command,"makechordchannels") == 0) {
    skipspace(&p);
    val = readnump(&p);
    makechordchannels(val);
    done = 1;
    } 

  else if (strcmp(command, "program") == 0) {
    int chan, prog;

    skipspace(&p);
    prog = readnump(&p);
    chan = channel;
    skipspace(&p);
    if ((*p >= '0') && (*p <= '9')) {
      chan = prog - 1;
      prog = readnump(&p);
    };
    if (noteson) {
      current_program[chan] = prog;
      write_program(prog, chan);
    };
    done = 1;
  }

  else if (strcmp(command, "gchord") == 0) {
    set_gchords(p);
    done = 1;
  }

  else if (strcmp(command, "drum") == 0) {
    set_drums(p);
    done = 1;
  }

  else if ((strcmp(command, "drumbars") == 0)) {
     drumbars = readnump(&p);
     if (drumbars < 1 || drumbars > 10) drumbars = 1;
     done = 1;
     drum_ptr = 0; /* [SS] 2018-06-23 */
     addtoQ(0,drum_denom,-1,drum_ptr,0,0);
     }

  else if ((strcmp(command, "gchordbars") == 0)) {
     gchordbars = readnump(&p);
     if (gchordbars < 1 || gchordbars > 10) gchordbars = 1;
     done = 1;
     g_ptr = 0; /* [SS] 2018-06-23 */
     addtoQ(0, g_denom, -1, g_ptr ,0, 0);
     }

  else if ((strcmp(command, "chordprog") == 0))  {
    int prog;

    prog = readnump(&p);
    if (gchordson) {
      write_program(prog, gchord.chan);
      /* [SS] 2011-11-18 */
      p = strstr(p,"octave=");
      if (p != 0)
                      {int octave,found;
                       p = p+7;
                       found = sscanf(p,"%d",&octave);
                       if (found == 1 && octave > -3 && octave < 3) gchord.base = 48 + 12*octave;
                       printf("gchord.base = %d\n",gchord.base);
                       }
    };
    done = 1;
  }

  else if ((strcmp(command, "bassprog") == 0)) {
    int prog;

    prog = readnump(&p);
    if (gchordson) {
      write_program(prog, fun.chan);
      /* [SS] 2011-11-18 */
      p = strstr(p,"octave=");
      if (p != 0)
                      {int octave,found;
                       p = p+7;
                       found = sscanf(p,"%d",&octave);
                       if (found == 1 && octave > -3 && octave < 3) fun.base = 36 + 12*octave;
                       printf("fun.base = %d\n",fun.base);
                       }
    };
    done = 1;
  }

  else if (strcmp(command, "chordvol") == 0) {
    gchord.vel = readnump(&p);
    done = 1;
  }

  else if (strcmp(command, "bassvol") == 0) {
    fun.vel = readnump(&p);
    done = 1;
  }


  /* [SS] 2012-12-12 */
  else if (strcmp(command, "bendvelocity") == 0) {
/* We use bendstring code so that bendvelocity integrates with !shape!.
   Bends a note along the shape of a parabola. The note is
   split into 8 segments. Given the bendacceleration and
   initial bend velocity, the new pitch bend is computed
   for each time segment.
*/
    bendvelocity = bendacceleration = 0;
    skipspace(&p);
    val = readsnump(&p);
    bendvelocity = val;
    skipspace(&p);
    val = readsnump(&p);
    bendacceleration = val;
    /* [SS] 2015-08-11 */
    bendnvals = 0;
    if (bendvelocity != 0 || bendacceleration != 0) {
        for (i = 0; i<8; i++) {
            benddata[i] = bendvelocity;
            bendvelocity = bendvelocity + bendacceleration;
            }
        bendnvals = 8;
        }
    /*bendtype = 1; [SS] 2015-08-11 */
    if (bendnvals == 1) bendtype = 3; /* [SS] 2014-09-22 */
    else bendtype = 2;
    done = 1;
    }

  /* [SS] 2014-09-10 */
  else if (strcmp(command, "bendstring") == 0) {
     i = 0;
     while (i<256) { /* [SS] 2015-09-10 2015-10-03 */
          benddata[i] = readsnump(&p);
          skipspace(&p);
          i = i + 1;
          /* [SS] 2015-08-31 */
          if (*p == 0) break;
        };
     bendnvals = i;
     done = 1;
     if (bendnvals == 1) bendtype = 3; /* [SS] 2014-09-22 */
     else bendtype = 2;
     }

  /* [SS] 2014-09-10 */
  else if (strcmp(command, "bendstringex") == 0) {
     i = 0;
     while (i<64) { /* [SS] 2020-08-09 2015-09-10 2015-10-03 */
          bendinput[i] = readsnump(&p);
          skipspace(&p);
          i = i + 1;
          if (*p == 0) break;
        };
     expand_array (bendinput, i, benddata, 4);
     bendnvals = i*4;
     done = 1;
     if (bendnvals == 1) bendtype = 3;
     else bendtype = 2;
     }


  else if (strcmp(command, "drone") == 0) {
    skipspace(&p);
    val = readnump(&p);
    if (val > 0) drone.prog = val;
    skipspace(&p);
    val = readnump(&p);
    if (val >0 ) drone.pitch1 = val;
    skipspace(&p);
    val = readnump(&p);
    if (val >0)  drone.pitch2 = val;
    skipspace(&p);
    val = readnump(&p);
    if (val >0) drone.vel1 = val;
    skipspace(&p);
    val = readnump(&p);
    if (val >0) drone.vel2 = val;
    if (drone.prog > 127) event_error("drone prog must be in the range 0-127");
    if (drone.pitch1 >127) event_error("drone pitch1 must be in the range 0-127");
    if (drone.vel1 >127) event_error("drone vel1 must be in the range 0-127");
    if (drone.pitch2 >127) event_error("drone pitch1 must be in the range 0-127");
    if (drone.vel2 >127) event_error("drone vel1 must be in the range 0-127");
    done = 1;
  }

  else if (strcmp(command, "beat") == 0) {
    skipspace(&p);
    loudnote = readnump(&p);
    skipspace(&p);
    mednote = readnump(&p);
    skipspace(&p);
    softnote = readnump(&p);
    skipspace(&p);
    beat = readnump(&p);
    if (beat == 0) {
      beat = barsize;
    };
    done = 1;
  }

  else if (strcmp(command, "beatmod") == 0) {
    skipspace(&p);
    velocity_increment = readsnump(&p);
    loudnote += velocity_increment;
    mednote  += velocity_increment;
    softnote += velocity_increment;
    if (loudnote > 127) loudnote = 127;
    if (mednote  > 127) mednote = 127;
    if (softnote > 127) softnote = 127;
    if (loudnote < 0)   loudnote = 0;
    if (mednote  < 0)   mednote = 0;
    if (softnote < 0)   softnote = 0;
    done = 1;
    }

  else if (strcmp(command, "beatstring") == 0) {
    int count;

    skipspace(&p);
    count = 0;
    while ((count < 99) && (strchr("fFmMpP", *p) != NULL)) {
      beatstring[count] = *p;
      count = count + 1;
      p = p + 1;
    };
    beatstring[count] = '\0';
    if (strlen(beatstring) == 0) {
      event_error("beatstring expecting string of 'f', 'm' and 'p'");
    }
    nbeats = strlen(beatstring);
    done = 1;
  }

  else if (strcmp(command, "control") == 0) {
    int chan, n, datum;
    char data[20];

    p = select_channel(&chan, p);
    n = 0;
    while ((n<20) && (*p >= '0') && (*p <= '9')) {
      datum = readnump(&p);
      if (datum > 127) {
        event_error("data must be in the range 0 - 127");
        datum = 0;
      };
      data[n] = (char) datum;
      n = n + 1;
      skipspace(&p);
    };
    write_event(control_change, chan, data, n);
    controldefaults[(int) data[0]] = (int) data[1]; /* [SS] 2015-08-10 */
    done = 1;
  }


  /* [SS] 2015-07-24 */
  else if (strcmp(command, "controlstring") == 0) {
     if (!controlcombo) { /* [SS] 2015-08-20 */
        for (i=0;i<MAXLAYERS;i++) controlnvals[i] = 0;
        nlayers = 0;  /* overwrite layer 0 if not a combo */
        }
     i = 0;
     if (nlayers >= MAXLAYERS) {
        event_error("too many combos for control data");
        } else {
        while (i<256) { /* [SS] 2015-09-10  2015-10-03 */
          controldata[nlayers][i] = readsnump(&p);
          skipspace(&p);
          i = i + 1;
          if (*p == 0) break;
          }
        controlnvals[nlayers] = i;
        /* [SS] 2015-08-23 */
        if (controlnvals[nlayers] < 2) event_error("empty %%MIDI controlstring"); 
        controlcombo = 0; /* turn off controlcombo */
        done = 1;
        }
     }

  /* [SS] 2015-08-20 */
  else if (strcmp(command, "controlcombo") == 0) {
     controlcombo = 1;
     nlayers++;
     done = 1;
     }

  else if( strcmp(command, "beataccents") == 0) {
    beataccents = 1;
    beatmodel = 0; /* [SS] 2011-07-04 */
    done = 1;
  }

  else if( strcmp(command, "nobeataccents") == 0) {
    beataccents = 0;
    done = 1;
  }

  else if (strcmp(command,"portamento") == 0) {
   int chan, datum;
   char data[4];
   p = select_channel(&chan, p);
   data[0] = 65;
   data[1] = 127;
   /* turn portamento on */
   write_event(control_change, chan, data, 2);
   data[0] = 5; /* coarse portamento */
   datum = readnump(&p);
   if (datum > 63) {
        event_error("data must be in the range 0 - 63");
        datum = 0;
      };
   data[1] =(char) datum;
   write_event(control_change, chan, data, 2);
   done = 1;
   } 

  else if (strcmp(command,"noportamento") == 0) {
   int chan;
   char data[4];
   p = select_channel(&chan, p);
   data[0] = 65;
   data[1] = 0;
   /* turn portamento off */
   write_event(control_change, chan, data, 2);
   done = 1;
   }

  else if (strcmp(command, "pitchbend") == 0) {
    int chan, n, datum;
    char data[2];

    p = select_channel(&chan, p);
    n = 0;
    data[0] = 0;
    data[1] = 0;
    while ((n<2) && (*p >= '0') && (*p <= '9')) {
      datum = readnump(&p);
      if (datum > 255) {
        event_error("data must be in the range 0 - 255");
        datum = 0;
      };
      data[n] = (char) datum;
      n = n + 1;
      skipspace(&p);
    };
/* don't write pitchbend in the header track [SS] 2005-04-02 */
    if (noteson) {
       write_event(pitch_wheel, chan, data, 2);
       tracklen = tracklen + delta_time;
       delta_time = 0L;
       } 
    done = 1;
  }

  else if (strcmp(command, "snt") == 0) {  /*single note tuning */
    int midikey;
    float midipitch;
    midikey = readnump(&p);
    sscanf(p,"%f", &midipitch);
    single_note_tuning_change(midikey,  midipitch);
    done = 1;
    }
   

  else if (strcmp(command,"chordattack") == 0) {
    staticnotedelay = readnump(&p);
    notedelay = staticnotedelay;
    done = 1;
  }

  else if (strcmp(command,"randomchordattack") == 0) {
    staticchordattack = readnump(&p);
    chordattack = staticchordattack;
    done = 1;
  }

  else if (strcmp(command,"drummap") == 0) {
    parse_drummap(&p);
    done = 1;
  }

  /* [SS] 2018-04-16 ptstress code moved to event_midi() in store.c */

  else if (strcmp(command,"stressmodel") == 0) { /* [SS] 2011-08-19 */
    if (barflymode == 0) 
        printf("**warning stressmodel is ignored without -BF runtime option\n");
    done = 1;
    }

  /* [SS] 2015-09-08 */
  else if (strcmp(command,"volinc") == 0) {
      single_velocity_inc = readsnump(&p);
      done = 1;
      }

  else if (strcmp(command,"vol") == 0) {
      single_velocity = readnump(&p);
      done = 1;
      }

  if (done == 0) {
    char errmsg[80];
    sprintf(errmsg, "%%%%MIDI command \"%s\" not recognized",command);
    event_error(errmsg);
  };
 if(wordson+noteson+gchordson+drumson+droneon == 0) delta_time = 0L;
  
}

static void delay(a, b, c)
/* wait for time a/b */
int a, b, c;
{
  int dt;

  dt = (div_factor*a)/b + c;
  err_num = err_num * b + ((div_factor*a)%b)*err_denom;
  err_denom = err_denom * b;
  reduce(&err_num, &err_denom);
  dt = dt + (err_num/err_denom);
  err_num = err_num%err_denom;
  timestep(dt, 0);
}

static void save_note(num, denom, pitch, pitchbend, chan, vel)
/* output 'note on' queue up 'note off' for later */
int num, denom;
int pitch, pitchbend, chan, vel;
{
/* don't transpose drum channel */
  if(chan == 9) {noteon_data(pitch, pitchbend, chan, vel);
                addtoQ(num, denom, pitch, chan,0, -1);}
  else  {noteon_data(pitch + transpose + global_transpose, pitchbend, chan, vel);
        addtoQ(num, denom, pitch + transpose + global_transpose, chan,0, -1);}
}




void dogchords(i)
/* generate accompaniment notes */
/* note no microtone or linear temperament support ! */
int i;
{
int j;
  if (g_ptr >= (int) strlen(gchord_seq)) g_ptr = 0;
  if ((i == g_ptr) ) {  /* [SS] 2018-06-23 */
    int len;
    char action;

    action = gchord_seq[g_ptr];
    len = gchord_len[g_ptr];
    if ((chordnum == -1) && (action == 'c')) {
      action = 'f';
    };
    switch (action) {

    case 'z':
      break;

    case 'f':
      if (g_started && gchords) {
        /* do fundamental */
        if (inversion == -1)
        save_note(g_num*len, g_denom, basepitch+fun.base, 8192, fun.chan, fun.vel);
        else
        save_note(g_num*len, g_denom, inversion+fun.base, 8192, fun.chan, fun.vel);
      };
      break;

    case 'b':
      if (g_started && gchords) {
        /* do fundamental */
        if (inversion == -1)  /* [SS] 2014-11-02 */
        save_note(g_num*len, g_denom, basepitch+fun.base, 8192, fun.chan, fun.vel);
        else
        save_note(g_num*len, g_denom, inversion+fun.base, 8192, fun.chan, fun.vel);
      };
/* There is no break here so the switch statement continues into the next case 'c' */ 

    case 'c':
      /* do chord with handling of any 'inversion' note */
      if (g_started && gchords) {
          for(j=0;j<gchordnotes_size;j++)
          save_note(g_num*len, g_denom, gchordnotes[j], 8192,
		    gchord.chan, gchord.vel);
        };
      break;

    case 'g':
      if(gchordnotes_size>0 && g_started && gchords)
        save_note(g_num*len, g_denom, gchordnotes[0], 8192, gchord.chan, gchord.vel); 
      else /* [SS] 2016-01-03 */
        save_note(g_num*len, g_denom, gchordnotes[gchordnotes_size], 8192, gchord.chan, gchord.vel); 
      break;

    case 'h':
      if(gchordnotes_size >1 && g_started && gchords)
        save_note(g_num*len, g_denom, gchordnotes[1], 8192, gchord.chan, gchord.vel); 
      else /* [SS] 2016-01-03 */
        save_note(g_num*len, g_denom, gchordnotes[gchordnotes_size], 8192, gchord.chan, gchord.vel); 
      break;

    case 'i':
      if(gchordnotes_size >2 && g_started && gchords)
        save_note(g_num*len, g_denom, gchordnotes[2], 8192, gchord.chan, gchord.vel); 
      else /* [SS] 2016-01-03 */
        save_note(g_num*len, g_denom, gchordnotes[gchordnotes_size], 8192, gchord.chan, gchord.vel); 
      break;

    case 'j':
      if(gchordnotes_size >3 && g_started && gchords)
        save_note(g_num*len, g_denom, gchordnotes[3], 8192, gchord.chan, gchord.vel); 
      else /* [SS] 2016-01-03 */
        save_note(g_num*len, g_denom, gchordnotes[gchordnotes_size], 8192, gchord.chan, gchord.vel); 
      break;

    case 'G':
      if(gchordnotes_size>0 && g_started && gchords)
        save_note(g_num*len, g_denom, gchordnotes[0]-12, 8192, gchord.chan, gchord.vel); 
      else /* [SS] 2016-01-03 */
        save_note(g_num*len, g_denom, gchordnotes[gchordnotes_size], 8192, gchord.chan, gchord.vel); 
      break;

    case 'H':
      if(gchordnotes_size >1 && g_started && gchords)
        save_note(g_num*len, g_denom, gchordnotes[1]-12, 8192, gchord.chan, gchord.vel); 
      else /* [SS] 2016-01-03 */
        save_note(g_num*len, g_denom, gchordnotes[gchordnotes_size], 8192, gchord.chan, gchord.vel); 
      break;

    case 'I':
      if(gchordnotes_size >2 && g_started && gchords)
        save_note(g_num*len, g_denom, gchordnotes[2]-12, 8192, gchord.chan, gchord.vel); 
      else /* [SS] 2016-01-03 */
        save_note(g_num*len, g_denom, gchordnotes[gchordnotes_size], 8192, gchord.chan, gchord.vel); 
      break;

    case 'J':
      if(gchordnotes_size >3 && g_started && gchords)
        save_note(g_num*len, g_denom, gchordnotes[3]-12, 8192, gchord.chan, gchord.vel); 
      else /* [SS] 2016-01-03 */
        save_note(g_num*len, g_denom, gchordnotes[gchordnotes_size], 8192, gchord.chan, gchord.vel); 
      break;

    case 'x':
      if(!gchord_error) {
         gchord_error++;
         event_warning("no default gchord string for this meter");
        }
      break;

     default:
       printf("no such gchord code %c\n",action);
      };


    g_ptr = g_ptr + 1; /* [SS] 2018-06-23 */
    addtoQ(g_num*len, g_denom, -1, g_ptr,0, 0);
    if (g_ptr >= (int) strlen(gchord_seq)) g_ptr = 0; /* [SS] 2018-06-23 */
    };
};

void dodrums(i)
/* generate drum notes */
int i;
{
  if (drum_ptr >= (int) strlen(drum_seq)) drum_ptr = 0; /* [SS] 2018-06-23 */
  if (i == drum_ptr) {  /* [SS] 2018-06-23 */
    int len;
    char action;

    action = drum_seq[drum_ptr];
    len = drum_len[drum_ptr];
    switch (action) {
    case 'z':
      break;
    case 'd':
      if (drum_on) {
        save_note(drum_num*len, drum_denom, drum_program[drum_ptr],8192,9, 
                  drum_velocity[drum_ptr]);
      };
    };
    drum_ptr = drum_ptr + 1;
    addtoQ(drum_num*len, drum_denom, -1, drum_ptr,0, 0);
    if (drum_ptr >= (int) strlen(drum_seq)) drum_ptr = 0; /* [SS] 2018-06-23 */
  };
}

void start_drone()
{
    int delta;
/*    delta = tracklen - drone.event; */
    delta = delta_time - drone.event;    
    if (drone.event == 0)  write_program(drone.prog, drone.chan);
    midi_noteon(delta,drone.pitch1+global_transpose,8192,drone.chan,drone.vel1);
    midi_noteon(delta,drone.pitch2+global_transpose,8192,drone.chan,drone.vel2);
/*    drone.event = tracklen;*/
    drone.event = delta_time;
}


void stop_drone()
{
    int delta;
/*    delta = tracklen - drone.event; */
    delta = delta_time - drone.event;
    midi_noteoff(delta,drone.pitch1+global_transpose,drone.chan);
    midi_noteoff(0,drone.pitch2+global_transpose,drone.chan);
/*    drone.event = tracklen; */
    drone.event = delta_time;
}



void progress_sequence(i)
int i;
{
  if (gchordson) {
    dogchords(i);
  };
  if (drumson) {
    dodrums(i);
  };
}

void init_drum_map()
{
int i;
for (i=0;i<256;i++)
   drum_map[i] = i;
}

void parse_drummap(char **s)
/* parse abc note and advance character pointer */
/* code stolen from parseabc.c and simplified */
/* [SS] 2017-12-10 no more 'static voice parse_drummap' */
{
  int mult;
  char accidental, note;
  int octave;
  int midipitch;
  int mapto;
  char msg[80];
  char *anoctave = "cdefgab";
  int scale[7] = {0, 2, 4, 5, 7, 9, 11};

  mult = 1;
  accidental = ' ';
  note = ' ';
  /* read accidental */
  switch (**s) {
  case '_':
    accidental = **s;
    *s = *s + 1;
    if (**s == '_') {
      *s = *s + 1;
      mult = 2;
    };
    break;
  case '^':
    accidental = **s;
    *s = *s + 1;
    if (**s == '^') {
      *s = *s + 1;
      mult = 2;
    };
    break;
  case '=':
    accidental = **s;
    *s = *s + 1;
    if (**s == '^') {
      accidental = **s;
      *s = *s + 1;
      } 
    else if (**s == '_') { 
      accidental = **s;
      *s = *s + 1;
      } 
    break;
  default:
    /* do nothing */
    break;
  };
  if ((**s >= 'a') && (**s <= 'g')) {
    note = **s;
    octave = 1;
    *s = *s + 1;
    while ((**s == '\'') || (**s == ',')) {
      if (**s == '\'') {
        octave = octave + 1;
        *s = *s + 1;
      };
      if (**s == ',') {
        sprintf(msg, "Bad pitch specifier , after note %c", note);
        event_error(msg);
        octave = octave - 1;
        *s = *s + 1;
      };
    };
  } else {
    octave = 0;
    if ((**s >= 'A') && (**s <= 'G')) {
      note = **s + 'a' - 'A';
      *s = *s + 1;
      while ((**s == '\'') || (**s == ',')) {
        if (**s == ',') {
          octave = octave - 1;
          *s = *s + 1;
        };
        if (**s == '\'') {
          sprintf(msg, "Bad pitch specifier ' after note %c", note + 'A' - 'a');
          event_error(msg);
          octave = octave + 1;
          *s = *s + 1;
        };
      };
    };
  };
  /*printf("note = %d octave = %d accidental = %d\n",note,octave,accidental);*/
  midipitch = (int) ((long) strchr(anoctave, note) - (long) anoctave);
  if (midipitch <0 || midipitch > 6) {
    event_error("Malformed note in drummap : expecting a-g or A-G");
    return;
    } 
  midipitch = scale[midipitch];
  if (accidental == '^') midipitch += mult;
  if (accidental == '_') midipitch -= mult;
  midipitch = midipitch + 12*octave + 60;
  skipspace(s);
  mapto = readnump(s);
  if (mapto == 0) {
      event_error("Bad drummap: expecting note followed by space and number");
       return;
      }
  if (mapto < 35 || mapto > 81) event_warning ("drummap destination should be between 35 and 81 inclusive");
  /*printf("midipitch = %d map to %d \n",midipitch,mapto);*/ 
  drum_map[midipitch] = mapto;
}

 
static void starttrack(int tracknum)
/* called at the start of each MIDI track. Sets up all necessary default */
/* and initial values */
{
  int i;

  loudnote = 105;
  mednote = 95;
  softnote = 80;
  beatstring[0] = '\0';
  beataccents = 1;
  nbeats = 0;
  transpose = 0;
/* make sure meter is reinitialized for every track
 * in case it was changed in the middle of the last track */
  set_meter(header_time_num,header_time_denom);
  div_factor = division;
  gchords = 1;
  partno = -1;
  partlabel = -1;
  g_started = 0;
  g_ptr = 0;
  drum_ptr = 0;
  Qinit();
  if (noteson) {
    /* [SS] 2015-03-24 */
    channel = trackdescriptor[tracknum].midichannel;
    if (channel == -1) {
       channel = findchannel();
       trackdescriptor[tracknum].midichannel = channel;
       if(verbose) printf("assigning channel %d to track %d\n",channel,tracknum);
       channel_in_use[channel] = 1; /* [SS] 2015-08-04 */
       }
    else
       channel_in_use[channel] = 1; /* [SS] 2015-08-04 */
       
    if (retuning) midi_re_tune (channel); /* [SS] 2012-04-01 */
  } else {
    /* set to valid value just in case - should never be used */
    channel = 0;
  };
  if (gchordson) {
    addtoQ(0, g_denom, -1, g_ptr,0, 0);
    fun.base = 36;
    fun.vel = 80;
    gchord.base = 48;
    gchord.vel = 75;
    fun.chan = findchannel();
    channel_in_use[fun.chan] = 1; /* [SS] 2015-03-27  2015-08-04 */
    if(verbose) printf("assigning channel %d to bass voice\n",fun.chan);
    gchord.chan = findchannel();
    channel_in_use[gchord.chan] = 1; /* [SS] 2015-03-27 2015-08-04 */
    if(verbose) printf("assigning channel %d to chordal accompaniment\n",gchord.chan);
    if (retuning) midi_re_tune (fun.chan); /* [SS] 2012-04-01 */
    if (retuning) midi_re_tune (gchord.chan); /* [SS] 2012-04-01 */
  };
  if (drumson) {  /* [SS] 2010-08-12 */
       drum_ptr = 0;
       addtoQ(0, drum_denom, -1, drum_ptr,0, 0);
       }
  if (droneon) {
    drone.event =0;
    drone.chan = findchannel();
    channel_in_use[drone.chan] = 1; /* [SS] 2015-03-27  2015-08-04 */
    if(verbose) printf("assigning channel %d to drone\n",drone.chan);
    if (retuning) midi_re_tune (drone.chan); /* [SS] 2012-04-01 */
    }

  g_next = 0;
  partrepno = 0;
/*  thismline = -1; [SS] july 28 2006 */
/* This disables the message 
 First lyrics line must come after first music line 
When a new voice is started with an inline voice command
eg [V:1] abcd| etc. Unfortunately this  is part of the 
abc2-draft.html standard. See canzonetta.abc in
abc.sourceforge.net/standard/abc2-draft.html 
*/
  thiswline = -1;
  nowordline = 0;
  waitforbar = 0;
  musicsyllables = 0;
  lyricsyllables = 0;
  for (i=0; i<26; i++) {
    part_count[i] = 0;
  };
  for (i=0;i<MAXCHANS;i++) {
    current_pitchbend[i] = 8192; /* neutral */
    current_program[i] = 0; /* acoustic piano */
    }
}


/* [NL] 2011-07-22  Nils Liberg EasyABC interface */
void easyabc_interface (int j) {
            char data[4];
            unsigned int row = num[j];
            unsigned int col = denom[j] + 1;           

           
            /* the row number is encoded as three 7-bit numbers: CC#110 (least significant) CC#111, and CC#112 (most significant) */                       
            data[0] = 110;
            data[1] = (row >> 14) & 0x7F;
            write_event(control_change, 0, data, 2);
           
            data[0] = 111;
            data[1] = (row >> 7) & 0x7F;
            write_event(control_change, 0, data, 2);
           
            data[0] = 112;
            data[1] = row & 0x7F;
            write_event(control_change, 0, data, 2);
           
            /* the col number is encoded as two 7-bit numbers: CC#113 (least significant) and CC#114 (most significant) */           
            data[0] = 113;
            data[1] = (col >> 7) & 0x7F;
            write_event(control_change, 0, data, 2);
           
            data[0] = 114;
            data[1] = col & 0x7F;
            write_event(control_change, 0, data, 2);
        }


/* [SS] 2011-10-19 */
void pedal_on() {
char data[4];
data[0] = 64;
data[1] = 127;
write_event(control_change, channel, data, 2);
}

/* [SS] 2011-10-19 */
void pedal_off() {
char data[4];
data[0] = 64;
data[1] = 0;
write_event(control_change, channel, data, 2);
}


long writetrack(xtrack)
/* this routine writes a MIDI track  */
int xtrack;
{
  int trackvoice;
  int inchord;
  int in_varend;
  int i,j, pass;
  int maxpass;
  int expect_repeat;
  int slurring;
  int was_slurring; /* [SS] 2011-11-30 */
  int state[6]; /* [SS] 2013-11-02 */
  int texton;
  int timekey;
  int note_num,note_denom;
  int tnote_num,tnote_denom; /* for note trimming */
  int graceflag;
  int effecton;  /* [SS] 2012-12-11 */
  char *annotation;


  tracknumber = xtrack; /* [SS] 2014-11-17 */
  /* default is a normal track */
  timekey=1;
  tracklen = 0L;
  delta_time = 1L; /* [SS] 2010-07-07 */
  delta_time_track0 = 0L; /* [SS] 2010-06-27 */
  trackvoice = xtrack;
  wordson = 0;
  noteson = 1;
  gchordson = 0;
  temposon = 0;
  texton = 1;
  drumson = 0;
  droneon = 0;
  in_varend = 0;
  maxpass = 2;
  notedelay = staticnotedelay;
  chordattack = staticchordattack;
  trim_num = 0;
  trim_denom = 1;
  graceflag = 0;
 /* ensure that the percussion channel is not selected by findchannel() */
  channel_in_use[9] = 1; 
  drumbars = 1;
  gchordbars = 1;
  effecton=0;  /* [SS] 2012-12-11 */
  bendtype = 1; /* [SS] 2014-09-11 */
  single_velocity_inc = 0;
  single_velocity = -1;

  bendstate = 8192; /* [SS] 2014-09-10 */
  for (i=0;i<16; i++) benddata[i] = 0;
  bendnvals = 0;
  /* [SS] 2015-08-29 */
  for (i=0;i<MAXLAYERS;i++) controlnvals[i] = 0;

/* [SS] 2014-09-10 */
  if (karaoke) {
    if (xtrack == 0)                  
       karaokestarttrack(xtrack);
    }
     
  if (trackdescriptor[xtrack].tracktype == NOTES) {
    kspace = 0;
    noteson = 1;
    wordson = 0;
    annotation = "note track"; /* [SS] 2015-06-22 */
    mf_write_meta_event(0L, text_event, annotation, strlen(annotation));
    trackvoice = trackdescriptor[xtrack].voicenum;
   }

  if (trackdescriptor[xtrack].tracktype == WORDS) {
    kspace = 0;
    noteson = 0;
    wordson = 1;
/*
 *  Turn text off for H:, A: and other fields.
 *  Putting it in Karaoke Words track (track 2) can throw off some Karaoke players.
 */   
    texton = 0;
    gchordson = 0;
    annotation = "lyric track"; /* [SS] 2015-06-22 */
    mf_write_meta_event(0L, text_event, annotation, strlen(annotation));
    trackvoice = trackdescriptor[xtrack].voicenum;
    }

  if (trackdescriptor[xtrack].tracktype == NOTEWORDS) {
    kspace = 0;
    noteson = 1;
    wordson = 1;
    annotation = "notes/lyric track"; /* [SS] 2015-06-22 */
    mf_write_meta_event(0L, text_event, annotation, strlen(annotation));
    trackvoice = trackdescriptor[xtrack].voicenum;
    }

  /* is this accompaniment track ? */
  if (trackdescriptor[xtrack].tracktype == GCHORDS) {
    noteson = 0; 
    gchordson = 1;
    drumson = 0;
    droneon = 0;
    temposon = 0;
    annotation = "gchord track"; /* [SS] 2015-06-22 */
    mf_write_meta_event(0L, text_event, annotation, strlen(annotation));
    trackvoice = trackdescriptor[xtrack].voicenum;
/* be sure set_meter is called before setbeat even if we
 * have to call it more than once at the start of the track */
    set_meter(header_time_num,header_time_denom);
/*    printf("calling setbeat for accompaniment track\n"); */
    setbeat();
  };

  /* is this drum track ? */
  if (trackdescriptor[xtrack].tracktype == DRUMS) {
    noteson = 0;
    gchordson = 0;
    drumson = 1;
    droneon =0;
    temposon = 0;
    annotation = "drum track"; /* [SS] 2015-06-22 */
    mf_write_meta_event(0L, text_event, annotation, strlen(annotation));
    trackvoice = trackdescriptor[xtrack].voicenum;
  };

  /* is this drone track ? */
  if (trackdescriptor[xtrack].tracktype == DRONE) {
    noteson = 0;
    gchordson = 0;
    drumson = 0;
    droneon = 1;
    temposon = 0;
    annotation = "drone track"; /* [SS] 2015-06-22 */
    mf_write_meta_event(0L, text_event, annotation, strlen(annotation));
    trackvoice = trackdescriptor[xtrack].voicenum;
   };
  nchordchannels = 0;
  if (xtrack == 0) {
    mf_write_tempo(tempo);
    /* write key */
    write_keysig(sf, mi);
    /* write timesig */
    write_meter(time_num, time_denom);
    gchordson = 0;
    temposon = 1;
    if (ntracks > 1) {
       /* type 1 files have no notes in first track */
       noteson = 0;
       texton = 0;
       trackvoice = 1;
       timekey=0;
       /* return(0L); */
    }
  }
  starttrack(xtrack);
  /* [SS] 2015-03-25 */
  if (verbose) {
     printf("trackvoice = %d track = %d",trackvoice,xtrack);
     if (noteson) printf("  noteson");
     if (wordson) printf("  wordson");
     if (gchordson) printf(" gchordson");
     if (drumson) printf(" drumson");
     if (droneon) printf(" droneon");
     if (temposon) printf(" temposon");
     printf("\n");
     }
  inchord = 0;
  /* write notes */
  j = 0;
  if ((voicesused) && (trackvoice != 1)) {
    j = findvoice(j, trackvoice, xtrack);
  };
  barno = 0;
  bar_num = 0;
  bar_denom = 1;
  err_num = 0;
  err_denom = 1;
  pass = 1;
  save_state(state, j, barno, div_factor, transpose, channel, lineno);
  slurring = 0;
  was_slurring = 0; /* [SS] 2011-11-30 */
  expect_repeat = 0;
  while (j < notes) {
    /* if (verbose >4) printf("%d %s\n",j,featname[feature[j]]);  [SS] 2012-11-21*/
    if (verbose >4) printf("%d %s %d %d/%d\n",j,featname[feature[j]],pitch[j],num[j],denom[j]); /* [SS] 2014-11-16*/
    lineposition = charloc[j]; /* [SS] 2014-12-25 */ 
    switch(feature[j]) {
    case NOTE:
	onemorenote = 0;
      if (wordson) {
        write_syllable(j);
      };
      if (nchordchannels >0) channel = chordchannels[0];
      if (noteson) {
        if (inchord) {
           notecount++;
           if (notecount > 1) {
                if(chordattack > 0) notedelay = (int) (chordattack*ranfrac());
                delta_time += notedelay;
                totalnotedelay += notedelay;
                if (notecount <= nchordchannels+1)
                     channel = chordchannels[notecount-1];
                }
           }
        noteon(j);
        /* set up note off */
       if (channel == 9) 
        addtoQ(num[j], denom[j], drum_map[pitch[j]], channel, 0, -totalnotedelay -1);
        else {
            if ((notecount > 1) && ((note_num * denom[j]) !=  (note_denom * num[j])))
               {
               char msg[100];
               sprintf(msg,"unequal notes in chord %d/%d versus %d/%d",
                  note_num,note_denom,num[j],denom[j]);
               if (!silent) event_warning(msg);
	       num[j] = note_num;
               denom[j] = note_denom;
               }
            note_num = num[j];
            note_denom = denom[j];
/* turn off slurring prematurely to separate two slurs in a row */
            if (slurring && feature[j+2] == SLUR_OFF) slurring = 0; /* [SS] 2011-11-30 */
            if (trim && !slurring && !graceflag) {
              tnote_num = note_num;
              tnote_denom = note_denom;
              if (gtfract(note_num,note_denom,trim_num,trim_denom))
                addfract(&tnote_num,&tnote_denom,-trim_num,trim_denom);
	      if (tnote_denom <= 0) {
		      event_error("note length denominator is zero or less"); /* [SS] 2020-01-14 to prevent infinite loop on some systems */
		      exit(1);
	      }
              addtoQ(tnote_num, tnote_denom, pitch[j] + transpose +global_transpose,
               channel,effecton, -totalnotedelay -1); /* [SS] 2012-12-11 */
               } 
            /* [SS] 2015-06-16 */
            else if (expand) {
              tnote_num = note_num;
              tnote_denom = note_denom;
                addfract(&tnote_num,&tnote_denom,expand_num,expand_denom);
                addtoQ(tnote_num, tnote_denom, pitch[j] + transpose +global_transpose,
                    channel,effecton, -totalnotedelay -1); /* [SS] 2012-12-11 */
                }
            else
            /* [SS] 2015-06-07 inserted effecton */
            addtoQ(note_num, note_denom, pitch[j] + transpose +global_transpose,
               channel, effecton, -totalnotedelay -1);
             }
};
      if (!inchord) {
        delay(num[j], denom[j], 0);
        addunits(num[j], denom[j]);
        notecount =0;
        totalnotedelay=0;
      };
      effecton = 0;  /* [SS] 2012-12-11 */
      break;
    case TNOTE:
	onemorenote = 1;
      if (wordson) {
        /* counts as 2 syllables : note + first tied note.
	 * We ignore any bar line placed between tied notes
	 * since this causes write_syllable to lose synchrony
	 * with the notes.                                    */
        write_syllable(j);
        waitforbar = 0;
        write_syllable(j);
      };
      if (noteson) {
        noteon(j);
        /* set up note off */
       if (channel == 9) 
        addtoQ(num[j], denom[j], drum_map[pitch[j]], channel, 0, -totalnotedelay -1);
        else addtoQ(num[j], denom[j], pitch[j] + transpose +global_transpose, channel, effecton, -totalnotedelay -1);
        effecton = 0;
      };
      break;
    case OLDTIE:
      if (wordson) {
        /* extra syllable beyond first two in a tie */
        write_syllable(j);
      };
      break;
    case REST:
      if (!inchord) {
        delay(num[j], denom[j], 0);
        addunits(num[j], denom[j]);
      };
      break;
    case CHORDON:
      inchord = 1;
      break;
    case CHORDOFF:
    case CHORDOFFEX:
      if (wordson) {
        write_syllable(j);
      };
      inchord = 0;
      delay(num[j], denom[j], 0);
      totalnotedelay=0;
      notecount=0;
      notedelay = staticnotedelay;
      chordattack = staticchordattack;
      note_num = num[j];
      note_denom = denom[j];
      addunits(note_num, note_denom);
      if (trim) {
          if (gtfract(note_num,note_denom,trim_num,trim_denom))
              addfract(&note_num,&note_denom,-trim_num,trim_denom);
      }
      break;
    case LINENUM:
      /* get correct line number for diagnostics */
      lineno = pitch[j];
      break;
    case MUSICLINE:
      if (wordson) {
        thismline = j;
        nowordline = 0;
      };
      break;
    case MUSICSTOP:
      if (wordson) {
        checksyllables();
      };
      break;
    case PART:
      in_varend = 0;
      j = partbreak(xtrack, trackvoice, j);
      if (parts == -1) {
        char msg[1];

        msg[0] = (char) pitch[j];
        mf_write_meta_event(0L, marker, msg, 1);
      };
      break;
    case VOICE:
      /* search on for next occurrence of voice */
      j = findvoice(j, trackvoice, xtrack);
      /* [SS] 2011-12-11 inline voice commands are not followed
       by MUSICLINE where we would normally get thismline */
      if (wordson) {     
        thismline = j+1;
        nowordline = 0;
      };
      break;
    case TEXT:
      if (texton) {
        mf_write_meta_event(0L, text_event, atext[pitch[j]],
                          strlen(atext[pitch[j]]));
      };
      break;
    case TITLE:
/*  Write name of song as sequence name in track 0 and as track 1 name. */
/*  karaokestarttrack routine handles this instead if tune is a Karaoke tune. */
        if (!karaoke) {
           if (xtrack < 2)
              mf_write_meta_event(0L, sequence_name, atext[pitch[j]],
                                strlen(atext[pitch[j]]));
        }
      break;
    case SINGLE_BAR:
      waitforbar = 0;
      checkbar(pass);
      break;
    case DOUBLE_BAR:  /* || */
      in_varend = 0;
      waitforbar = 0;
      softcheckbar(pass);
      break;

    case BAR_REP: /* |: */
    /* ensures that two |: don't occur in a row                */
    /* saves position of where to return when :| is encountered */
      in_varend = 0;
      waitforbar = 0;
      softcheckbar(pass);
      if ((pass==1)&&(expect_repeat)) {
        event_error("Expected end repeat not found at |:");
      };
      save_state(state, j, barno, div_factor, transpose, channel, lineno);
      expect_repeat = 1;
      pass = 1;
      maxpass=2;
      break;

    case REP_BAR:  /* :|  */
    /* ensures it was preceded by |: so we know where to return */
    /* returns index j to the place following |:                */ 
      in_varend = 0;
      waitforbar = 0;
      softcheckbar(pass);
      if (pass == 1) {
         if (!expect_repeat) {
            event_error("Found unexpected :|");
          } else {
          /*  pass = 2;  [SS] 2004-10-14 */
            pass++;   /* we may have multiple repeats */
            restore_state(state, &j, &barno, &div_factor, &transpose, &channel, &lineno);
            slurring = 0;
            was_slurring = 0;
            expect_repeat = 0;
          };

      } 
      else {
     /* we could have multi repeats.                        */
     /* pass = 1;          [SS] 2004-10-14                  */
     /* we could have accidentally have                       */
     /*   |: .sect 1..  :| ...sect 2 :|.  We  don't want to */
     /* go back to sect 1 when we encounter :| in sect 2.   */
     /* We signal that we will expect |: but we wont't check */
            if(pass < maxpass)
              {
              expect_repeat = 0;
              pass++;   /* we may have multiple repeats */
              restore_state(state, &j, &barno, &div_factor, &transpose, &channel, &lineno);
              slurring = 0;
              was_slurring = 0;
              }
      };
      break;

    case PLAY_ON_REP: /* |[1 or |[2 or |1 or |2 */
    /* keeps count of the pass number and selects the appropriate   */
    /* to be played for each pass. This code was designed to handle */ 
    /* multirepeats using the inlist() function however the pass    */
    /* variable is not set up correctly for multirepeats.           */
      {
        int passnum;
        char errmsg[80];
 
        if (in_varend != 0) {
          event_error("Need || |: :| or ::  to mark end of variant ending");
        };
        passnum = -1;
        if (((expect_repeat)||(pass>1))) {
          passnum = pass;
        }

        if (passnum == -1) {
          event_error("multiple endings do not follow |: or ::");
          passnum = 1;
        };
       if (inlist(j, passnum) != 1) {
          j = j + 1;
     /* if this is not the variant ending to be played on this pass*/
     /* then skip to the end of this section watching out for voice*/
     /* changes. Usually a section end with a :|, but the last     */
     /* last section could end with almost anything including a    */
     /* PART change.                                               */
          if(feature[j] == VOICE) j = findvoice(j, trackvoice, xtrack);
          while ((j<notes) && (feature[j] != REP_BAR) && 
                 (feature[j] != BAR_REP) &&
                 (feature[j] != PART) &&
                 (feature[j] != DOUBLE_BAR) &&
                 (feature[j] != THICK_THIN) &&
                 (feature[j] != THIN_THICK) &&
                 (feature[j] != PLAY_ON_REP)) {
            j = j + 1;
            if(feature[j] == VOICE) j = findvoice(j, trackvoice, xtrack);
          };
          barno = barno + 1;
          if ((j == notes) /* || (feature[j] == PLAY_ON_REP) */) { 
          /* end of tune was encountered before finding end of */
          /* variant ending.  */
            sprintf(errmsg, 
              "Cannot find :| || [| or |] to close variant ending");
            event_error(errmsg);
          } else {
            if (feature[j] == PART) {
              j = j - 1; 
            };
          };
        } else {
          in_varend = 1;   /* segment matches pass number, we play it */
          /*printf("playing at %d for pass %d\n",j,passnum); */
          if (maxpass < 4) maxpass = pass+1; /* [SS] 2010-09-28 */
        };
      };
      break;

    case DOUBLE_REP:     /*  ::  */
      in_varend = 0;
      waitforbar = 0;
      softcheckbar(pass);
      if (pass > 1) {
        /* Already gone through last time. Process it as a |:*/
        /* and continue on.                                  */
        expect_repeat = 1;
        save_state(state, j, barno, div_factor, transpose, channel, lineno);
        pass = 1;
        maxpass=2;
      } else {
          /* should do a repeat unless |: is missing.       */
          if (!expect_repeat) {
            /* missing |: don't repeat but set up for next repeat */
            /* section.                                           */
            event_error("Found unexpected ::");
            expect_repeat = 1;
            save_state(state, j, barno, div_factor, transpose, channel, lineno);
            pass = 1;
          } else {
            /* go back and do the repeat */
            restore_state(state, &j, &barno, &div_factor, &transpose, &channel, &lineno);
            slurring = 0;
            was_slurring = 0;
            /*pass = 2;  [SS] 2004-10-14*/
            pass++;
          };
      };
      break;

    case GCHORD:
      basepitch = pitch[j];
      inversion = num[j];
      chordnum = denom[j];
      g_started = 1;
      configure_gchord();
      break;
    case GCHORDON:
      if (gchordson) {
        gchords = 1;
      };
      break;
    case GCHORDOFF:
      gchords = 0;
      break;
    case DRUMON:
      if (drumson) {
        drum_on = 1;
      };
      break;
    case DRUMOFF:
      drum_on = 0;
      break;
    case DRONEON:
      if (droneon) 
         start_drone();
      break;
    case DRONEOFF:
      if (droneon) 
         stop_drone();
      break;
    case ARPEGGIO:
       notedelay = 3*staticnotedelay;
       chordattack=3*staticchordattack;
       break;
    case GRACEON:
       graceflag = 1;
       break;
    case GRACEOFF:
       graceflag = 0;
       break;
    case DYNAMIC:
      dodeferred(atext[pitch[j]],noteson);
      break;
    case KEY:
      if(timekey) write_keysig(pitch[j], denom[j]);
      break;
    case TIME:
      if(timekey) {
        barchecking = pitch[j];
        write_meter(num[j], denom[j]);
        setbeat();   /* NEW [SS] 2003-APR-27 */
        }
      break;
    case TEMPO:
      if (temposon) {
        char data[3];
/*
        long newtempo;

        newtempo = ((long)num[j]<<16) | ((long)denom[j] & 0xffff);
        printf("New tempo = %ld [%x %x]\n", newtempo, num[j], denom[j]);
*/
        data[0] = num[j] & 0xff;
        data[1] = (denom[j]>>8) & 0xff;
        data[2] = denom[j] & 0xff;
/* new [SS] 2010-06-27 delta_time_track0 */
        if (ntracks != 1) {  /*  [SS] 2010-08-31 */
              mf_write_meta_event(delta_time_track0, set_tempo, data, 3);
              tracklen = tracklen + delta_time_track0;
              delta_time_track0=0L;
              } else { /* [SS] 2010-08-31 */
              mf_write_meta_event(delta_time, set_tempo, data, 3);
              delta_time=0L;
              }

/*
        if (j > 0) {
          div_factor = pitch[j];
        };
*/
      };
      break;
    case CHANNEL:
      channel = pitch[j];
      break;
    case TRANSPOSE:
      transpose = pitch[j];
      break;
    case GTRANSPOSE:
      global_transpose = pitch[j];
      break;
    case RTRANSPOSE:
      global_transpose +=  pitch[j];
      break;
    case SLUR_ON:
      /*
      if (slurring) {
        event_error("Unexpected start of slur found");
      }; [SS] 2014-04-24
      */
      slurring = 1;
      was_slurring = 1; /* [SS] 2011-11-30 */
      break;
    case SLUR_OFF:
      /*
      if (!slurring && !was_slurring) { 
        event_error("Unexpected end of slur found");
      };
      [SS] 2014-04-24 */
      slurring = 0;
      was_slurring = 0;
      break;
    case COPYRIGHT:
       if (xtrack == 0) {
          mf_write_meta_event(delta_time, copyright_notice, atext[pitch[j]], strlen (atext[pitch[j]]));
       }
      break;
    case SETTRIM:
       trim_num = num[j];
       trim_denom = denom[j];
       if (trim_num > 0) trim = 1;
       else trim = 0;
       break;
    case EXPAND:
        expand_num = num[j];
        expand_denom = denom[j];
        if (expand_num > 0) {trim = 0;
                             expand = 1;
                            }
        else expand = 0;
        break;
    case META:    /* [SS] 2011-07-18 */
       if (pitch[j] == 0 && noteson==1)  {
            /*printf("linenum = %d charpos = %d\n",num[j],denom[j]);*/
            easyabc_interface(j);
          }
       break; /* [SS] 2012-05-28 */
    case PEDAL_ON: /* [SS] 2011-10-19 */
       pedal_on();
       break;

    case PEDAL_OFF: /* [SS] 2011-10-19 */
       pedal_off();
       break;

    case EFFECT: 
       if (pitch[j] == 1) /* [SS] 2015-07-26 */
           effecton = bendtype;  /* [SS] 2012-12-11 2014-09-11 */
       else
           effecton = 10;
       break;
    
    default:
      break;
    };
    j = j + 1;
  };
  if ((expect_repeat)&&(pass==1) && !silent) {
    event_error("Missing :| at end of tune");
  };
  clearQ();
  tracklen = tracklen + delta_time;
  if (xtrack == 1) {
    tracklen1 = tracklen;
  } else {
    if ((xtrack != 0) && (tracklen != tracklen1)) {
      char msg[100];
      float fbeats,fbeats1; /* [SS] 2013-12-14 */
      fbeats  = (float) tracklen/(float) 480.0;
      fbeats1 = (float) tracklen1/(float) 480.0;

      sprintf(msg, "Track %d is %f quarter notes long not %f",
              xtrack, fbeats, fbeats1);
      event_warning(msg);
    };
  };
  return (delta_time);
}



void dumpfeat (int from, int to)
{
int i,j;
for (i=from;i<=to;i++)
  {
  j = feature[i]; 
  if (j<0 || j>74) printf("illegal feature[%d] = %d\n",i,j); /* [SS] 2012-11-25 */
  else printf("%d %s   %d %d %d %d %d %d\n",i,featname[j],pitch[i],bentpitch[i],decotype[i],num[i],denom[i],charloc[i]);
  }
}

void dump_barloc (FILE *diaghandle, int trkno)
{
int i;
fprintf(diaghandle,"track = %d voice = %d type = %d number of bars = %d\n",trkno,trackdescriptor[trkno].voicenum,trackdescriptor[trkno].tracktype,barno);
for (i=0;i<barno;i++) {
  fprintf(diaghandle,"%6.2f\t",(double) barloc[i]/480.0);
  if (i%8 == 7) fprintf(diaghandle,"\n");
  }
fprintf(diaghandle,"\n");
}



/* see file queues.c for routines to handle note queue */


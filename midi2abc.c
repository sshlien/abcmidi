/* midi2abc - program to convert MIDI files to abc notation.
 * Copyright (C) 1998 James Allwright
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
 */
 

/* new midi2abc - converts MIDI file to abc format files
 * 
 *
 * re-written to use dynamic data structures 
 *              James Allwright
 *               5th June 1998
 *
 * added output file option -o
 * added summary option -sum
 * added option -u to enter xunit directly
 * fixed computation of xunit using -b option
 * added -obpl (one bar per line) option
 * add check for carriage return embedded inside midi text line
 *                Seymour Shlien  04/March/00
 * made to conform as much as possible to the official version.
 * check for drum track added
 * when midi program channel is command encountered, we ensure that 
 * we are using the correct channel number for the Voice by sending
 * a %%MIDI channel message.
 *
 * Many more changes (see doc/CHANGES) 
 *
 *                Seymour Shlien  2005
 * 
 * based on public domain 'midifilelib' package.
 */

#define VERSION "3.47 November 01 2020 midi2abc"

/* Microsoft Visual C++ Version 6.0 or higher */
#ifdef _MSC_VER
#define snprintf _snprintf
#define ANSILIBS
#endif

#include <stdio.h>
#include <math.h>
#ifdef PCCFIX
#define stdout 1
#endif

/* define USE_INDEX if your C libraries have index() instead of strchr() */
#ifdef USE_INDEX
#define strchr index
#endif

#ifdef ANSILIBS
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#else
extern char* malloc();
extern char* strchr();
#endif
#include "midifile.h"
#define BUFFSIZE 200
/* declare MIDDLE C */
#define MIDDLE 72
void initfuncs();
void setupkey(int);
void stats_finish();
int testtrack(int trackno, int barbeats, int anacrusis);
int open_note(int chan, int pitch, int vol);
int close_note(int chan, int pitch, int *initvol);
float histogram_entropy (int *histogram, int size); 
void stats_noteoff(int chan,int pitch,int vol);
void reset_back_array (); /* [SS] 2019-05-08 */ 


/* Global variables and structures */

extern long Mf_toberead;

static FILE *F;
static FILE *outhandle; /* for producing the abc file */

int tracknum=0;  /* track number */
int division;    /* pulses per quarter note defined in MIDI header    */
long tempo = 500000; /* the default tempo is 120 quarter notes/minute */
int unitlen;     /* abc unit length usually defined in L: field       */
int header_unitlen; /* first unitlen set                              */
int unitlen_set =0; /* once unitlen is set don't allow it to change   */
int parts_per_unitlen = 2; /* specifies minimum quantization size */
long laston = 0; /* length of MIDI track in pulses or ticks           */
char textbuff[BUFFSIZE]; /*buffer for handling text output to abc file*/
int trans[256], back[256]; /*translation tables for MIDI pitch to abc note*/
int barback[256]; /* to reinitialize back[] after each bar line */
char atog[256]; /* translation tables for MIDI pitch to abc note */
int symbol[256]; /*translation tables for MIDI pitch to abc note */
int key[12];
int sharps;
int trackno;
int maintrack;
int format; /* MIDI file type                                   */

int karaoke, inkaraoke;
int midline;
int tempocount=0;  /* number of tempo indications in MIDI file */
int gotkeysig=0; /*set to 1 if keysignature found in MIDI file */

/* global parameters that may be set by command line options    */
int xunit;    /* pulses per abc unit length                     */
int tsig_set; /* flag - time signature already set by user      */
int ksig_set; /* flag - key signature already set by user       */
int xunit_set;/* flat - xunit already set by user               */
int extracta; /* flag - get anacrusis from strong beat          */
int guessu;   /* flag - estimate xunit from note durations      */
int guessa;   /* flag - get anacrusis by minimizing tied notes  */
int guessk;   /* flag - guess key signature                     */
int summary;  /* flag - output summary info of MIDI file        */
int keep_short; /*flag - preserve short notes                   */
int swallow_rests; /* flag - absorb short rests                 */
int midiprint; /* flag - run midigram instead of midi2abc       */
int stats = 0; /* flag - gather and print statistics            */
int usesplits; /* flag - split measure into parts if needed     */
int restsize; /* smallest rest to absorb                        */
/* [SS] 2017-01-01 */
/*int no_triplets;  flag - suppress triplets or broken rhythm   */
int allow_triplets; /* flag to allow triplets                   */
int allow_broken;   /* flag to allow broken rhythms > <         */
int obpl = 0; /* flag to specify one bar per abc text line      */
int nogr = 0; /* flag to put a space between every note         */
int noly = 0; /* flag to allow lyrics [SS] 2019-07-12           */
int bars_per_line=4;  /* number of bars per output line         */
int bars_per_staff=4; /* number of bars per music staff         */
int asig, bsig;  /* time signature asig/bsig                    */
int header_asig =0; /* first time signature encountered         */
int header_bsig =0; /* first time signature encountered         */
int header_bb;      /* first ticks/quarter note encountered     */
int active_asig,active_bsig;  /* last time signature declared   */
int last_asig, last_ksig; /* last time signature printed        */
int barsize; /* barsize in parts_per_unitlen units                   */
int chordthreshold; /* number of maximum number of pulses separating note */
int Qval;        /* tempo - quarter notes per minute            */
int verbosity=0; /* control amount of detail messages in abcfile*/
int debug=0; /* for debugging */

/* global arguments dependent on command line options or computed */

int anacrusis=0; 
int bars;
int keysig;
int header_keysig=  -50;  /* header key signature                     */
int active_keysig = -50;  /* last key signature declared        */
int xchannel;  /* channel number to be extracted. -1 means all  */
int timeunits = 1; /*tells prtime to display time in beats [SS] 2018-10-25 */


/* structure for storing music notes */
struct anote {
  int pitch;  /* MIDI pitch    */
  int chan;   /* MIDI channel  */
  int vel;    /* MIDI velocity */
  long time;  /* MIDI onset time in pulses */
  long dtnext; /* time increment to next note in pulses */
  long tplay;  /* note duration in pulses */
  int xnum;    /* number of xunits to next note */
  int playnum; /* note duration in number of xunits */
  int posnum; /* note position in xunits */
  int splitnum; /* voice split number */
  /* int denom; */
};


/* linked list of notes */
struct listx {
  struct listx* next;
  struct anote* note;
};

/* linked list of text items (strings) */
struct tlistx {
  struct tlistx* next;
  char* text;
  long when; 	/* time in pulses to output */
  int type;     /* 0 - comments, other - field commands */
};


/* a MIDI track */
struct atrack {
  struct listx* head;    /* first note */
  struct listx* tail;    /* last note */
  struct tlistx* texthead; /* first text string */
  struct tlistx* texttail; /* last text string */
  int notes;             /* number of notes in track */
  long tracklen;
  long startwait;
  int startunits;
  int drumtrack;
  /* [SS] 2019-05-29 for debugging */
  int texts;            /* number of text links in track */
};

/* can cope with up to 64 track MIDI files */
struct atrack track[64];
int trackcount = 0;
int maxbarcount = 0;
/* maxbarcount  is used to return the numbers of bars created.*/
/* obpl is a flag for one bar per line. */

/* double linked list of notes */
/* used for temporary list of chords while abc is being generated */
struct dlistx {
  struct dlistx* next;
  struct dlistx* last;
  struct anote* note;
};

int notechan[2048],notechanvol[2048]; /*for linking on and off midi
					channel commands            */
int last_tick[17]; /* for getting last pulse number in MIDI file */
int last_on_tick[17]; /* for detecting chords [SS] 2019-08-02 */

char *title = NULL; /* for pasting title from argv[] */
char *origin = NULL; /* for adding O: info from argv[] */
int newline_flag = 0; /* [SS] 2019-06-14 signals new line was just issued */


void remove_carriage_returns();
int validnote();
void printpitch(struct anote*);
void printfract(int, int);
void txt_trackend();
void txt_noteoff(int,int,int);
/* [SS] 2019-05-29 new functions to handle type 0 midi files */
void txt_trackstart_type0();
void txt_noteon_type0(int,int,int);
void txt_program_type0(int,int);

/* The following variables are used by the -stats option
 * which is used by a separate application called midiexplorer.tcl.
 * The channel numbers go from 1 to 16 instead of 0 to 15
 */
struct trkstat {
  int notecount[17];
  int chordcount[17];
  int notemeanpitch[17];
  int notelength[17];
  int pitchbend[17];
  int pressure[17];
  int cntlparam[17];
  int program[17];
  int tempo[17];
  int npulses[17];
  } trkdata;

/* The trkstat references the individual channels in the midi file.
 * notecount is the number of notes or bass notes in the chord.
 * chordcount is the number of notes not counting the bass notes.
 * notemeanpitch is the average pitch for the channel.
 * notelength is the average note length.
 * pitchbend is the number of pitch bends for the channel.
 * pressure is the number of control pressure commands.
 * cntlparam is the number of control parameter commands.
 * program is number of times there is a program command for the channel.
 * tempo is the number of times there is a tempo command.
 * npulses is the number of pulses.
 */

int progcolor[17]; /* used by stats_program */
int drumhistogram[82]; /* counts drum noteons */
int pitchhistogram[12]; /* pitch distribution for non drum notes */
int channel2prog[17]; /* maps channel to program */
int channel2nnotes[17]; /*maps channel to note count */
int chnactivity[17]; /* [SS] 2018-02-02 */
int progactivity[128]; /* [SS] 2018-02-02 */
int pitchclass_activity[12]; /* [SS] 2018-02-02 */


/* [SS] 2017-11-01 */
static int progmapper[] = {
 0,  0,  0,  0,  0,  0,  0,  0,
 0,  1,  1,  1,  1,  1,  1,  2,
 3,  3,  3,  3,  3,  3,  3,  3,
 2,  2,  4,  4,  4,  4,  4,  2,
 5,  5,  5,  5,  5,  5,  5,  5,
 6,  6,  6,  6,  6,  2,  7, 10,
 7,  7,  7,  7,  8,  8,  8,  8,
 9,  9,  9,  9,  9,  9,  9,  9,
11, 11, 11, 11, 11, 11, 11, 11,
12, 12, 12, 12, 12, 12, 12, 12,
13, 13, 13, 13, 13, 13, 13, 13,
14, 14, 14, 14, 14, 14, 14, 14,
15, 15, 15, 15, 15, 15, 15, 15,
 2,  2,  2,  2,  2, 12,  6, 12,
 1,  1, 10, 10, 10, 10, 10,  1,
16, 16, 16, 16, 16, 16, 16, 16
};

/*              Stage 1. Parsing MIDI file                   */

/* Functions called during the reading pass of the MIDI file */

/* The following C routines are required by midifilelib.  */
/* They specify the action to be taken when various items */
/* are encountered in the MIDI.  The mfread function scans*/
/* the MIDI file and calls these functions when needed.   */


int filegetc()
{
    return(getc(F));
}


void fatal_error(s)
char* s;
/* fatal error encounterd - abort program */
{
  fprintf(stderr, "%s\n", s);
  exit(1);
}


void event_error(s)
char *s;
/* problem encountered but OK to continue */
{
  char msg[256];

  sprintf(msg, "Error: Time=%ld Track=%d %s\n", Mf_currtime, trackno, s);
  printf("%s",msg);
}


int* checkmalloc(bytes)
/* malloc with error checking */
int bytes;
{
  int *p;

  p = (int*) malloc(bytes);
  if (p == NULL) {
    fatal_error("Out of memory error - cannot malloc!");
  };
  return (p);
}



char* addstring(s)
/* create space for string and store it in memory */
char* s;
{
  char* p;
  int numbytes;

  /* p = (char*) checkmalloc(strlen(s)+1); */
  /* Integer overflow leading to heap buffer overflow in midi2abc
   * Debian Bug report log #924947 [SS] 2019-08-11
   * https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=924947
   */
  numbytes = strlen(s)+2; /* [SS] 2019-08-11 */
  if (numbytes > 1024) numbytes = 1024; /* [SS] 2019-08-11 */
  p = (char*) checkmalloc(numbytes); /* [SS] 2019-04-13  2019-08-11*/
  strncpy(p, s,numbytes); /* [SS] 2017-08-30 [SDG] 2020-06-03 */
  p[numbytes-1] = '\0'; /* [JA] 2020-11-01 */
  return(p);
}

void addtext(s, type)
/* add structure for text */
/* used when parsing MIDI file */
char* s;
int type;
{
  struct tlistx* newx;

  track[trackno].texts = track[trackno].texts + 1; /* [SS] 2019-05-29 */
  newx = (struct tlistx*) checkmalloc(sizeof(struct tlistx));
  newx->next = NULL;
  newx->text = addstring(s);
  newx->type = type;
  newx->when = Mf_currtime;
  if (track[trackno].texthead == NULL) {
    track[trackno].texthead = newx;
    track[trackno].texttail = newx;
  }
  else {
    track[trackno].texttail->next = newx;
    track[trackno].texttail = newx;
  };
}
  
/* [SS] 2019-05-2019 forwards text to track[chn] */
void addtext_type0(s, type,chn)
/* add structure for text */
/* used when parsing MIDI file */
char* s;
int type;
int chn;
{
  struct tlistx* newx;

  track[chn].texts = track[chn].texts + 1;
  newx = (struct tlistx*) checkmalloc(sizeof(struct tlistx));
  newx->next = NULL;
  newx->text = addstring(s);
  newx->type = type;
  newx->when = Mf_currtime;
  if (track[chn].texthead == NULL) {
    track[chn].texthead = newx;
    track[chn].texttail = newx;
  }
  else {
    track[chn].texttail->next = newx;
    track[chn].texttail = newx;
  };
}
  



/* The MIDI file has  separate commands for starting                 */
/* and stopping a note. In order to determine the duration of        */
/* the note it is necessary to find the note_on command associated   */
/* with the note off command. We rely on the note's pitch and channel*/
/* number to find the right note. While we are parsing the MIDI file */
/* we maintain a list of all the notes that are currently on         */
/* head and tail of list of notes still playing.                     */
/* The following doubly linked list is used for this purpose         */

struct dlistx* playinghead;
struct dlistx* playingtail; 


void noteplaying(p)
/* This function adds a new note to the playinghead list. */
struct anote* p;
{
  struct dlistx* newx;

  newx = (struct dlistx*) checkmalloc(sizeof(struct dlistx));
  newx->note = p;
  newx->next = NULL;
  newx->last = playingtail;
  if (playinghead == NULL) {
    playinghead = newx;
  };
  if (playingtail == NULL) {
    playingtail = newx;
  } 
  else {
    playingtail->next = newx;
    playingtail = newx;
  };
}


void addnote(p, ch, v)
/* add structure for note */
/* used when parsing MIDI file */
int p, ch, v;
{
  struct listx* newx;
  struct anote* newnote;

  track[trackno].notes = track[trackno].notes + 1;
  newx = (struct listx*) checkmalloc(sizeof(struct listx));
  newnote = (struct anote*) checkmalloc(sizeof(struct anote));
  newx->next = NULL;
  newx->note = newnote;
  if (track[trackno].head == NULL) {
    track[trackno].head = newx;
    track[trackno].tail = newx;
  } 
  else {
    track[trackno].tail->next = newx;
    track[trackno].tail = newx;
  };
  if (ch == 9) {
    track[trackno].drumtrack = 1;
  };
  newnote->pitch = p;
  newnote->chan = ch;
  newnote->vel = v;
  newnote->time = Mf_currtime;
  laston = Mf_currtime;
  newnote->tplay = Mf_currtime;
  noteplaying(newnote);
}

/* [SS] 2019-05-29 forwards note to track[ch] */
void addnote_type0(p, ch, v)
/* add structure for note */
/* used when parsing MIDI file */
int p, ch, v;
{
  struct listx* newx;
  struct anote* newnote;

  track[ch].notes = track[ch].notes + 1;
  newx = (struct listx*) checkmalloc(sizeof(struct listx));
  newnote = (struct anote*) checkmalloc(sizeof(struct anote));
  newx->next = NULL;
  newx->note = newnote;
  if (track[ch].head == NULL) {
    track[ch].head = newx;
    track[ch].tail = newx;
  } 
  else {
    track[ch].tail->next = newx;
    track[ch].tail = newx;
  };
  if (ch == 9) {
    track[ch].drumtrack = 1;
  };
  newnote->pitch = p;
  newnote->chan = ch;
  newnote->vel = v;
  newnote->time = Mf_currtime;
  laston = Mf_currtime;
  newnote->tplay = Mf_currtime;
  noteplaying(newnote);
}



void notestop(p, ch)
/* MIDI note stops */
/* used when parsing MIDI file */
int p, ch;
{
  struct dlistx* i;
  int found;
  char msg[80];

  i = playinghead;
  found = 0;
  while ((found == 0) && (i != NULL)) {
    if ((i->note->pitch == p)&&(i->note->chan==ch)) {
      found = 1;
    } 
    else {
      i = i->next;
    };
  };
  if (found == 0) {
    sprintf(msg, "Note terminated when not on - pitch %d", p);
    event_error(msg);
    return;
  };
  /* fill in tplay field */
  i->note->tplay = Mf_currtime - (i->note->tplay);
  /* remove note from list */
  if (i->last == NULL) {
    playinghead = i->next;
  } 
  else {
    (i->last)->next = i->next;
  };
  if (i->next == NULL) {
    playingtail = i->last;
  } 
  else {
    (i->next)->last = i->last;
  };
  free(i);
}




FILE *
efopen(name,mode)
char *name;
char *mode;
{
    FILE *f;

    if ( (f=fopen(name,mode)) == NULL ) {
      char msg[256];
      sprintf(msg,"Error - Cannot open file %s",name);
      fatal_error(msg);
    }
    return(f);
}

void error(s)
char *s;
{
    fprintf(stderr,"Error: %s\n",s);
}


/* [SS] 2017-11-19 */
void stats_error(s)
char *s;
{
    fprintf(stderr,"Error: %s\n",s);
    fprintf(stderr,"activetrack %d\n",tracknum);
    stats_finish();
}



void txt_header(xformat,ntrks,ldivision)
int xformat, ntrks, ldivision;
{
    division = ldivision; 
    format = xformat;
    if (format != 0) {
    /*  fprintf(outhandle,"%% format %d file %d tracks\n", format, ntrks);*/
      if(summary>0) printf("This midi file has %d tracks\n\n",ntrks);
    } 
    else {
/*     fprintf(outhandle,"%% type 0 midi file\n"); */
/* [SS] 2019-05-29 use these functions for type 0 midi files */
      Mf_trackstart =  txt_trackstart_type0;
      Mf_trackend =  txt_trackend;
      Mf_noteon =  txt_noteon_type0;
      Mf_noteoff =  txt_noteoff;
      Mf_program =  txt_program_type0;
     if(summary>0) {
	     printf("This is a type 0 midi file.\n");
             printf("All the channels are in one track.\n");
             printf("You may need to process the channels separately\n\n");
            }
     }
     
}


void txt_trackstart()
{
  laston = 0L;
  track[trackno].notes = 0;
  track[trackno].texts = 0; /* [SS] 2019-05-29 */
  track[trackno].head = NULL;
  track[trackno].tail = NULL;
  track[trackno].texthead = NULL;
  track[trackno].texttail = NULL;
  track[trackno].tracklen = Mf_currtime;
  track[trackno].drumtrack = 0;
}


/* [SS] 2019-05-29 new function. We need all 16 tracks
 * to handle type 0 midi file. Each track links to one
 * of the 16 midi channels                            */
void txt_trackstart_type0()
{
  int i;
  for (i=0;i<16;i++) {
    track[i].notes = 0;
    track[i].texts = 0; /* [SS] 2019-05-29 */
    track[i].head = NULL;
    track[i].tail = NULL;
    track[i].texthead = NULL;
    track[i].texttail = NULL;
    track[i].tracklen = Mf_currtime;
    track[i].drumtrack = 0;
    }
}


void txt_trackend()
{
  /* check for unfinished notes */
  if (playinghead != NULL) {
    printf("Error in MIDI file - notes still on at end of track!\n");
  };
  track[trackno].tracklen = Mf_currtime - track[trackno].tracklen;
  trackno = trackno + 1;
  trackcount = trackcount + 1;
}

void txt_noteon(chan,pitch,vol)
int chan, pitch, vol;
{
  if ((xchannel == -1) || (chan == xchannel)) {
    if (vol != 0) {
      addnote(pitch, chan, vol);
    } 
    else {
      notestop(pitch, chan);
    };
  };
}

/* [SS] 2019-05-29 new. Calls addnote_type0() function */
void txt_noteon_type0(chan,pitch,vol)
int chan, pitch, vol;
{
  if ((xchannel == -1) || (chan == xchannel)) {
    if (vol != 0) {
      addnote_type0(pitch, chan, vol);
    } 
    else {
      notestop(pitch, chan);
    };
  };
}

void txt_noteoff(chan,pitch,vol)
int chan, pitch, vol;
{
  if ((xchannel == -1) || (chan == xchannel)) {
    notestop(pitch, chan);
  };
}

void txt_pressure(chan,pitch,press)
int chan, pitch, press;
{
}

void stats_pressure(chan,press)
int chan, press;
{
trkdata.pressure[0]++;
}

void txt_parameter(chan,control,value)
int chan, control, value;
{
}

void txt_pitchbend(chan,lsb,msb)
int chan, msb, lsb;
{
}

void txt_program(chan,program)
int chan, program;
{
/* suppress the %%MIDI program for channel 10 */
  if (chan == 9) return;  /* [SS] 2020-02-17 */
  sprintf(textbuff, "%%%%MIDI program %d", program);
  addtext(textbuff,0);
/* abc2midi does not use the same channel number as specified in 
  the original midi file, so we should not specify that channel
  number in the %%MIDI program. If we leave it out the program
  will refer to the current channel assigned to this voice.
*/
}

/* [SS] 2019-05-29 new. Calls addtext_type0 function. */
void txt_program_type0(chan,program)
int chan, program;
{
  if (chan == 9) return;  /* [SS] 2020-02-17 */
  sprintf(textbuff, "%%%%MIDI program %d", program);
  addtext_type0(textbuff,0,chan);
/* abc2midi does not use the same channel number as specified in 
  the original midi file, so we should not specify that channel
  number in the %%MIDI program. If we leave it out the program
  will refer to the current channel assigned to this voice.
*/
}

void txt_sysex(leng,mess)
int leng;
char *mess;
{
}

void txt_metamisc(type,leng,mess)
int type, leng;
char *mess;
{
}

void txt_metaspecial(type,leng,mess)
int type, leng;
char *mess;
{
}

void txt_metatext(type,leng,mess)
int type, leng;
char *mess;
{ 
    char *ttype[] = {
    NULL,
    "Text Event",        /* type=0x01 */
    "Copyright Notice",    /* type=0x02 */
    "Sequence/Track Name",
    "Instrument Name",    /* ...     */
    "Lyric",
    "Marker",
    "Cue Point",        /* type=0x07 */
    "Unrecognized"
  };
  int unrecognized = (sizeof(ttype)/sizeof(char *)) - 1;
  unsigned char c;
  int n;
  char *p = mess;
  char *buff;
  char buffer2[BUFFSIZE];

  if ((type < 1)||(type > unrecognized))
      type = unrecognized;
  buff = textbuff;
  for (n=0; n<leng; n++) {
    c = *p++;
    if (buff - textbuff < BUFFSIZE - 6) {
      sprintf(buff, 
           (isprint(c)||isspace(c)) ? "%c" : "\\0x%02x" , c);
      buff = buff + strlen(buff);
    };
  }
  if (strncmp(textbuff, "@KMIDI KARAOKE FILE", 14) == 0) {
    karaoke = 1;
  } 
  else {
    if ((karaoke == 1) && (*textbuff != '@')) {
      addtext(textbuff,0);
    } 
    else {
      if (leng < BUFFSIZE - 3) {
        snprintf(buffer2, sizeof(buffer2), " %s", textbuff); /*[SDG] 2020-06-03*/
        addtext(buffer2,type); /* [SS] 2019-07-12 */
      };
    };
  };
}

void txt_metaseq(num)
int num;
{  
  sprintf(textbuff, "%%Meta event, sequence number = %d",num);
  addtext(textbuff,0);
}

void txt_metaeot()
/* Meta event, end of track */
{
}

void txt_keysig(sf,mi)
char sf, mi;
{
  int accidentals;
  gotkeysig =1;
  sprintf(textbuff, 
         "%% MIDI Key signature, sharp/flats=%d  minor=%d",
          (int) sf, (int) mi);
  if(verbosity) addtext(textbuff,0);
  sprintf(textbuff,"%d %d\n",sf,mi);
  if (!ksig_set) {
	  addtext(textbuff,1);
	  keysig=sf;
  }
  if (header_keysig == -50) header_keysig = keysig;
  if (summary <= 0) return;
  /* There may be several key signature changes in the midi
     file so that key signature in the mid file does not conform
     with the abc file. Show all key signature changes. 
  */   
  accidentals = (int) sf;
  if (accidentals <0 )
    {
    accidentals = -accidentals;
    printf("Key signature: %d flats", accidentals);
    }
  else
     printf("Key signature : %d sharps", accidentals);
  if (ksig_set) printf(" suppressed\n");
  else printf("\n");
}

void txt_tempo(ltempo)
long ltempo;
{
    if(tempocount>0) return; /* ignore other tempo indications */
    tempo = ltempo;
    tempocount++;
}


void setup_timesig(nn,  denom,  bb)
int nn,denom,bb;
{
  asig = nn;
  bsig = denom;
/* we must keep unitlen and xunit fixed for the entire tune */
  if (unitlen_set == 0) {
    unitlen_set = 1; 
    if ((asig*4)/bsig >= 3) {
      unitlen =8;
      } 
    else {
      unitlen = 16;
      };
   }
/* set xunit for this unitlen */
  if(!xunit_set) xunit = (division*bb*4)/(8*unitlen);
  barsize = parts_per_unitlen*asig*unitlen/bsig;
/*  printf("setup_timesig: unitlen=%d xunit=%d barsize=%d\n",unitlen,xunit,barsize); */
  if (header_asig ==0) {header_asig = asig;
	                header_bsig = bsig;
			header_unitlen = unitlen;
			header_bb = bb;
                       }
}


void txt_timesig(nn,dd,cc,bb)
int nn, dd, cc, bb;
{
  int denom = 1;
  while ( dd-- > 0 )
    denom *= 2;
  sprintf(textbuff, 
          "%% Time signature=%d/%d  MIDI-clocks/click=%d  32nd-notes/24-MIDI-clocks=%d", 
    nn,denom,cc,bb);
  if (verbosity) addtext(textbuff,0);
  sprintf(textbuff,"%d %d %d\n",nn,denom,bb);
  if (!tsig_set) {
	  addtext(textbuff,2);
          setup_timesig(nn, denom,bb);
   }
  if (summary>0) {
    if(tsig_set) printf("Time signature = %d/%d suppressed\n",nn,denom);
    else printf("Time signature = %d/%d\n",nn,denom);
    }
}


void txt_smpte(hr,mn,se,fr,ff)
int hr, mn, se, fr, ff;
{
}

void txt_arbitrary(leng,mess)
char *mess;
int leng;
{
}



/* Dummy functions for handling MIDI messages.
 *    */
 void no_op0() {}
 void no_op1(int dummy1) {}
 void no_op2(int dummy1, int dummy2) {}
 void no_op3(int dummy1, int dummy2, int dummy3) { }
 void no_op4(int dummy1, int dummy2, int dummy3, int dummy4) { }
 void no_op5(int dummy1, int dummy2, int dummy3, int dummy4, int dummy5) { }


void print_txt_header(xformat,ntrks,ldivision)
int xformat, ntrks, ldivision;
{
    division = ldivision; 
    format = xformat;
    printf("Header %d %d %d\n",format,ntrks,ldivision); /*[SS] 2019-11-13 */
}


void print_txt_noteon(chan, pitch, vol)
int chan, pitch, vol;
{
int start_time;
int initvol;
if (vol > 0)
open_note(chan, pitch, vol);
else {
  start_time = close_note(chan, pitch,&initvol);
  if (start_time >= 0)
     /* printf("%8.4f %8.4f %d %d %d %d\n",
       (double) start_time/(double) division,
       (double) Mf_currtime/(double) division,
       trackno+1, chan +1, pitch,initvol);
     */
       printf("%d %ld %d %d %d %d\n",
       start_time, Mf_currtime, trackno+1, chan +1, pitch,initvol);

      if(Mf_currtime > last_tick[chan+1]) last_tick[chan+1] = Mf_currtime;
   }
}



void print_txt_noteoff(chan, pitch, vol)
int chan, pitch, vol;
{
int start_time,initvol;

start_time = close_note(chan, pitch, &initvol);
if (start_time >= 0)
/*
    printf("%8.4f %8.4f %d %d %d %d\n",
     (double) start_time/(double) division,
     (double) Mf_currtime/(double) division,
     trackno+1, chan+1, pitch,initvol);
*/
     printf("%d %ld %d %d %d %d\n",
       start_time, Mf_currtime, trackno+1, chan +1, pitch,initvol);
    if(Mf_currtime > last_tick[chan+1]) last_tick[chan+1] = Mf_currtime;
}



/* In order to associate a channel note off message with its
 * corresponding note on message, we maintain the information
 * the notechan array. When a midi pitch (0-127) is switched
 * on for a particular channel, we record the time that it
 * was turned on in the notechan array. As there are 16 channels
 * and 128 pitches, we initialize an array 128*16 = 2048 elements
 * long.
**/
void init_notechan()
{
/* signal that there are no active notes */
 int i;
 for (i = 0; i < 2048; i++) notechan[i] = -1;
}


/* The next two functions update notechan when a channel note on
   or note off is encountered. The second function close_note,
   returns the time when the note was turned on.
*/
int open_note(int chan, int pitch, int vol)
{
    notechan[128 * chan + pitch] = Mf_currtime;
    notechanvol[128 * chan + pitch] = vol;
    return 0;
}


int close_note(int chan, int pitch, int *initvol)
{
    int index, start_tick;
    index = 128 * chan + pitch;
    if (notechan[index] < 0)
	return -1;
    start_tick = notechan[index];
    *initvol = notechanvol[index];
    notechan[index] = -1;
    return start_tick;
}

void print_txt_program(int chan,int program) {
  /* [SS] 2019-11-06 */
  printf("%ld Program  %2d %d \n",Mf_currtime,chan+1, program);
  /*printf("Program  %2d %d \n",chan+1, program);*/
  }

/* mftext mode */
int prtime(int units)
{
/*  if(Mf_currtime >= pulses) ignore=0; 
  if (ignore) return 1; 
  linecount++;
  if(linecount > maxlines) {fclose(F); exit(0);}
*/
  if(units==1)
 /*seconds*/
     printf("%6.2f   ",mf_ticks2sec(Mf_currtime,division,tempo));
  else if (units==2)
 /*beats*/
     printf("%6.2f   ",(float) Mf_currtime/(float) division);
  else
 /*pulses*/
    printf("%6ld  ",Mf_currtime);
  return 0;
}

char * pitch2key(int note)
{
static char name[16]; /* [SDG] 2020-06-03 */
char* s = name;
  switch(note % 12)
  {
  case 0: *s++ = 'c'; break;
  case 1: *s++ = 'c'; *s++ = '#'; break;
  case 2: *s++ = 'd'; break;
  case 3: *s++ = 'd'; *s++ = '#'; break;
  case 4: *s++ = 'e'; break;
  case 5: *s++ = 'f'; break;
  case 6: *s++ = 'f'; *s++ = '#'; break;
  case 7: *s++ = 'g'; break;
  case 8: *s++ = 'g'; *s++ = '#'; break;
  case 9: *s++ = 'a'; break;
  case 10: *s++ = 'a'; *s++ = '#'; break;
  case 11: *s++ = 'b'; break;
  }
  sprintf(s, "%d", (note / 12)-1);  /* octave  (assuming Piano C4 is 60)*/
  return  name;
}


void pitch2drum(midipitch)
int midipitch;
{
static char *drumpatches[] = {
 "Acoustic Bass Drum", "Bass Drum 1", "Side Stick", "Acoustic Snare",
 "Hand Clap", "Electric Snare", "Low Floor Tom", "Closed Hi Hat",
 "High Floor Tom", "Pedal Hi-Hat", "Low Tom", "Open Hi-Hat",
 "Low-Mid Tom", "Hi Mid Tom", "Crash Cymbal 1", "High Tom",		
 "Ride Cymbal 1", "Chinese Cymbal", "Ride Bell", "Tambourine",
 "Splash Cymbal", "Cowbell", "Crash Cymbal 2", "Vibraslap",
 "Ride Cymbal 2", "Hi Bongo", "Low Bongo",	"Mute Hi Conga",
 "Open Hi Conga", "Low Conga", "High Timbale", "Low Timbale",
 "High Agogo", "Low Agogo", "Cabasa", "Maracas",
 "Short Whistle", "Long Whistle", "Short Guiro", "Long Guiro",
 "Claves", "Hi Wood Block", "Low Wood Block", "Mute Cuica",
 "Open Cuica", "Mute Triangle", "Open Triangle" };
if (midipitch >= 35 && midipitch <= 81) {
  printf(" (%s)",drumpatches[midipitch-35]);
  }
}


void mftxt_header (int format, int ntrks, int ldivision)
{
  division = ldivision;
  printf("Header format=%d ntrks=%d division=%d\n",format,ntrks,division);
}


void stats_header (int format, int ntrks, int ldivision)
{
  int i;
  division = ldivision;
  printf("ntrks %d\n",ntrks);
  printf("ppqn %d\n",ldivision);
  chordthreshold = ldivision/16; /* [SS] 2018-01-21 */
  trkdata.tempo[0] = 0;
  trkdata.pressure[0] = 0;
  trkdata.program[0] = 0;
  for (i=0;i<17;i++) {
    trkdata.npulses[i] = 0;
    trkdata.pitchbend[i] = 0;
    progcolor[i] = 0;
    channel2prog[i] = -1;
    channel2nnotes[i] = 0;
    chnactivity[i] = 0; /* [SS] 2018-02-02 */
    }
  for (i=0;i<82;i++) drumhistogram[i] = 0;
  for (i=0;i<12;i++) pitchhistogram[i] = 0; /* [SS] 2017-11-01 */
  for (i=0;i<12;i++) pitchclass_activity[i] = 0; /* [SS] 2018-02-02 */
  for (i=0;i<128;i++) progactivity[i] = 0; /* [SS] 2018-02-02 */
}

void determine_progcolor ()
{
int i;
for (i=0;i<17;i++) progcolor[i] =0;
for (i=0;i<128;i++) {
  progcolor[progmapper[i]] += progactivity[i];
  }
}


/* [SS] 2018-04-24 */
void output_progs_data () {
int i;
/* check that there is valid progactivity */

  printf("progs ");
  for (i=0;i<128;i++) 
    if(progactivity[i] > 0) printf(" %d",i);
  printf("\nprogsact ");
  for (i=0;i<128;i++) 
    if(progactivity[i] > 0) printf(" %d",progactivity[i]);
  }



void stats_finish()
{
int i; /* [SDG] 2020-06-03 */
int npulses;
int nprogs;

npulses = trkdata.npulses[0]; 
printf("npulses %d\n",trkdata.npulses[0]);
printf("tempocmds %d\n",trkdata.tempo[0]);
printf("pitchbends %d\n",trkdata.pitchbend[0]);
for (i=1;i<17;i++) {
  if (trkdata.pitchbend[i] > 0) {
    printf("pitchbendin %d %d\n",i,trkdata.pitchbend[i]); }
    }
 
if (trkdata.pressure[0] > 0) 
  printf("pressure %d\n",trkdata.pressure[0]);
printf("programcmd %d\n",trkdata.program[0]);

nprogs = 0; /* [SS] 2018-04-24 */
for (i=1;i<128;i++)
  if(progactivity[i] >0) nprogs++;
if (nprogs > 0) output_progs_data();
else {
  for (i=0;i<17;i++) 
    if(chnactivity[i] > 0) 
       progactivity[channel2prog[i]] = chnactivity[i];
  output_progs_data();
  }

determine_progcolor();
printf("\nprogcolor ");
if (npulses > 0)
  for (i=0;i<17;i++) printf("%5.2f ",progcolor[i]/(double) npulses);
else 
  for (i=0;i<17;i++) printf("%5.2f ",(double) progcolor[i]);
printf("\n");
  
printf("drums ");
for (i=35;i<82;i++) {
  if (drumhistogram[i] > 0) printf("%d ",i);
  }
printf("\ndrumhits ");
for (i=35;i<82;i++) {
  if (drumhistogram[i] > 0) printf("%d ",drumhistogram[i]);
  }

printf("\npitches "); /* [SS] 2017-11-01 */
for (i=0;i<12;i++) printf("%d ",pitchhistogram[i]);
printf("\npitchact "); /* [SS] 2018-02-02 */
if (npulses > 0)
  for (i=0;i<12;i++) printf("%5.2f ",pitchclass_activity[i]/(double) npulses);
else 
  for (i=0;i<12;i++) printf("%5.2f ",(double) pitchclass_activity[i]);
printf("\nchnact "); /* [SS] 2018-02-08 */
if (npulses > 0)
  for (i=1;i<17;i++) printf("%5.2f ",chnactivity[i]/(double) trkdata.npulses[0]);
else 
  for (i=0;i<17;i++) printf("%5.2f ",(double) chnactivity[i]);
printf("\npitchentropy %f\n",histogram_entropy(pitchclass_activity,12));
printf("\n");
}



float histogram_entropy (int *histogram, int size) 
  {
  int i;
  int total;
  float entropy;
  float e,p;
  total = 0;
  entropy = 0.0;
  for (i=0;i<size;i++) {
    total += histogram[i];
    } 
  for (i=0;i<size;i++) {
    if (histogram[i] < 1) continue;
    p = (float) histogram[i]/total;
    e = p*log(p);
    entropy = entropy + e;
    } 
  return -entropy/log(2.0);
  } 



void mftxt_trackstart()
{
  int numbytes;
  tracknum++;
  numbytes = Mf_toberead;
  /*if(track != 0 && tracknum != track) {ignore_bytes(numbytes); return;} */
  printf("Track %d contains %d bytes\n",tracknum,numbytes);
}

void output_count_trkdata(data_array,name)
int data_array[];
char *name;
{
int i,sum;
sum = 0;
for (i=1;i<17;i++) sum += data_array[i];
if (sum != 0) {
      for (i=0;i<17;i++)
        if(data_array[i]>0) {
          printf("%s ",name);
          printf("%d %d ",i,data_array[i]);
          printf("\n");
          }
     }
}



void output_track_summary () {
int i;
/* find first channel containing data */
for (i=0;i<17;i++) {
   if(trkdata.notecount[i] == 0 && trkdata.chordcount[i] == 0) continue; 
   printf("trkinfo ");
   printf("%d %d ",i,trkdata.program[i]); /* channel number and program*/
   printf("%d %d ",trkdata.notecount[i],trkdata.chordcount[i]);
   printf("%d %d",trkdata.notemeanpitch[i], trkdata.notelength[i]);
   printf("\n");

   channel2nnotes[i] += trkdata.notecount[i] + trkdata.chordcount[i];
  }
}




void stats_trackstart()
{
  int i;
  tracknum++;
  for (i=0;i<17;i++) {
     trkdata.notecount[i] = 0;
     trkdata.notemeanpitch[i] = 0;
     trkdata.notelength[i] = 0;
     trkdata.chordcount[i] = 0;
     trkdata.cntlparam[i] = 0;
     last_tick[i] = -1;
     last_on_tick[i] = -1;
     }
  printf("trk %d \n",tracknum);
}

void stats_trackend()
{
 trkdata.npulses[tracknum] = Mf_currtime; 
 if (trkdata.npulses[0] < Mf_currtime) trkdata.npulses[0] = Mf_currtime;
 output_track_summary();
}


void mftxt_noteon(chan,pitch,vol)
int chan, pitch, vol;
{
  char *key;
/*
  if (onlychan >=0 && chan != onlychan) return;
*/
  if (prtime(timeunits)) return;
  key = pitch2key(pitch);
  printf("Note on  %2d %2d (%3s) %3d",chan+1, pitch, key,vol);
  if (chan == 9) pitch2drum(pitch);
  printf("\n");
}

void stats_noteon(chan,pitch,vol)
int chan, pitch, vol;
{
 if (vol == 0) {
    /* treat as noteoff */
    stats_noteoff(chan,pitch,vol);
    return;
    }
 trkdata.notemeanpitch[chan+1] += pitch;
 if (abs(Mf_currtime - last_on_tick[chan+1]) < chordthreshold) trkdata.chordcount[chan+1]++;
 else trkdata.notecount[chan+1]++; /* [SS] 2019-08-02 */
 last_tick[chan+1] = Mf_currtime;
 last_on_tick[chan+1] = Mf_currtime; /* [SS] 2019-08-02 */
 /* last_on_tick not updated by stats_noteoff */
 if (chan == 9) {
   if (pitch < 0 || pitch > 81) 
         printf("****illegal drum value %d\n",pitch);
   else  drumhistogram[pitch]++;
   }
 else pitchhistogram[pitch % 12]++;  /* [SS] 2017-11-01 */
}


void mftxt_noteoff(chan,pitch,vol)
int chan, pitch, vol;
{
  char *key;
/*
  if (onlychan >=0 && chan != onlychan) return;
*/
  if (prtime(timeunits)) return;
  key = pitch2key(pitch);
  printf("Note off %2d %2d  (%3s) %3d\n",chan+1,pitch, key,vol);
}


void stats_noteoff(int chan,int pitch,int vol)
{
  int length;
  int program;
  /* ignore if there was no noteon */
  if (last_tick[chan+1] == -1) return;
  length = Mf_currtime - last_tick[chan+1];
  trkdata.notelength[chan+1] += length;
  chnactivity[chan+1] += length;
  if (chan == 9) return; /* drum channel */
  pitchclass_activity[pitch % 12] += length;
  program = trkdata.program[chan+1];
  progactivity[program] += length;
  /* [SS] 2018-04-18 */
  if(Mf_currtime > last_tick[chan+1]) last_tick[chan+1] = Mf_currtime;
}

void mftxt_pressure(chan,pitch,press)
int chan, pitch, press;
{
  char *key;
  if (prtime(timeunits)) return;
  key = pitch2key(pitch);
  printf("Pressure %2d   %3s %3d\n",chan+1,key,press);
}


void mftxt_pitchbend(chan,lsb,msb)
int chan, lsb, msb;
{
 float bend;
 int pitchbend;
/*
  if (onlychan >=0 && chan != onlychan) return;
*/
  if (prtime(timeunits)) return;
  /* [SS] 2014-01-05  2015-08-04*/
  pitchbend = (msb*128 + lsb);
  bend =  (float) (pitchbend - 8192);
  bend = bend/40.96;
  printf("Pitchbend %2d %d  bend = %6.3f (cents)\n",chan+1,pitchbend,bend);
}

void stats_pitchbend(chan,lsb,msb)
int chan, lsb, msb;
{
trkdata.pitchbend[0]++;
trkdata.pitchbend[chan+1]++;
}

void mftxt_program(chan,program)
int chan, program;
{
static char *patches[] = {
 "Acoustic Grand","Bright Acoustic","Electric Grand","Honky-Tonk", 
 "Electric Piano 1","Electric Piano 2","Harpsichord","Clav", 
 "Celesta", "Glockenspiel",  "Music Box",  "Vibraphone", 
 "Marimba", "Xylophone", "Tubular Bells", "Dulcimer", 
 "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ", 
 "Reed Organ", "Accordian", "Harmonica", "Tango Accordian",
 "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)",
 "Electric Guitar (jazz)", "Electric Guitar (clean)", 
 "Electric Guitar (muted)", "Overdriven Guitar",
 "Distortion Guitar", "Guitar Harmonics",
 "Acoustic Bass", "Electric Bass (finger)",
 "Electric Bass (pick)", "Fretless Bass",
 "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
 "Violin", "Viola", "Cello", "Contrabass",
 "Tremolo Strings", "Pizzicato Strings",
 "Orchestral Strings", "Timpani",
 "String Ensemble 1", "String Ensemble 2",
 "SynthStrings 1", "SynthStrings 2", 
 "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit",
 "Trumpet", "Trombone", "Tuba", "Muted Trumpet",
 "French Horn", "Brass Section", "SynthBrass 1", "SynthBrass 2",
 "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
 "Oboe", "English Horn", "Bassoon", "Clarinet",
 "Piccolo", "Flute", "Recorder", "Pan Flute",
 "Blown Bottle", "Skakuhachi", "Whistle", "Ocarina",
 "Lead 1 (square)", "Lead 2 (sawtooth)",
 "Lead 3 (calliope)", "Lead 4 (chiff)", 
 "Lead 5 (charang)", "Lead 6 (voice)",
 "Lead 7 (fifths)", "Lead 8 (bass+lead)",
 "Pad 1 (new age)", "Pad 2 (warm)",
 "Pad 3 (polysynth)", "Pad 4 (choir)",
 "Pad 5 (bowed)", "Pad 6 (metallic)",
 "Pad 7 (halo)", "Pad 8 (sweep)",
 "FX 1 (rain)", "(soundtrack)",
 "FX 3 (crystal)", "FX 4 (atmosphere)",
 "FX 5 (brightness)", "FX 6 (goblins)",
 "FX 7 (echoes)", "FX 8 (sci-fi)",
 "Sitar", "Banjo", "Shamisen", "Koto",
 "Kalimba", "Bagpipe", "Fiddle", "Shanai",
 "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock",
 "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
 "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
 "Telephone ring", "Helicopter", "Applause", "Gunshot"};
/*
  if (onlychan >=0 && chan != onlychan) return;
*/
  if (prtime(timeunits)) return;
  printf("Program  %2d %d (%s)\n",chan+1, program,patches[program]);
   }


void stats_program(chan,program)
int chan, program;
{
int beatnumber;
if (program <0 || program > 127) return; /* [SS] 2018-03-06 */
if (trkdata.program[chan+1] != 0) {
  beatnumber = Mf_currtime/division;
  printf("cprogram %d %d %d\n",chan+1,program,beatnumber);
  /* count number of times the program was modified for a channel */
  trkdata.program[0] = trkdata.program[0]+1;
  } else {
  printf("program %d %d\n",chan+1,program);
  trkdata.program[chan+1] = program;
  }
  if (channel2prog[chan+1]== -1) channel2prog[chan+1] = program;
}


void mftxt_parameter(chan,control,value)
int chan, control, value;
{
  static char *ctype[] = {
 "Bank Select",       "Modulation Wheel",     /*1*/
 "Breath controller", "unknown",              /*3*/
 "Foot Pedal",        "Portamento Time",      /*5*/
 "Data Entry",        "Volume",               /*7*/
 "Balance",           "unknown",              /*9*/
 "Pan position",      "Expression",           /*11*/
 "Effect Control 1",  "Effect Control 2",     /*13*/
 "unknown",           "unknown",              /*15*/
 "Slider 1",          "Slider 2",             /*17*/
 "Slider 3",          "Slider 4",             /*19*/
 "unknown",           "unknown",              /*21*/
 "unknown",           "unknown",              /*23*/
 "unknown",           "unknown",              /*25*/
 "unknown",           "unknown",              /*27*/
 "unknown",           "unknown",              /*29*/
 "unknown",           "unknown",              /*31*/
 "Bank Select (fine)",  "Modulation Wheel (fine)",    /*33*/
 "Breath controller (fine)",  "unknown",              /*35*/
 "Foot Pedal (fine)",   "Portamento Time (fine)",     /*37*/
 "Data Entry (fine)",   "Volume (fine)",              /*39*/
 "Balance (fine)",      "unknown",                    /*41*/
 "Pan position (fine)", "Expression (fine)",          /*43*/
 "Effect Control 1 (fine)",  "Effect Control 2 (fine)", /*45*/
 "unknown",           "unknown",             /*47*/
 "unknown",           "unknown",             /*49*/
 "unknown",           "unknown",             /*51*/
 "unknown",           "unknown",             /*53*/
 "unknown",           "unknown",             /*55*/
 "unknown",           "unknown",             /*57*/
 "unknown",           "unknown",             /*59*/
"unknown",           "unknown",             /*61*/
 "unknown",           "unknown",             /*63*/
 "Hold Pedal",        "Portamento",          /*65*/
 "Susteno Pedal",     "Soft Pedal",          /*67*/
 "Legato Pedal",      "Hold 2 Pedal",        /*69*/
 "Sound Variation",   "Sound Timbre",        /*71*/
 "Sound Release Time",  "Sound Attack Time", /*73*/
 "Sound Brightness",  "Sound Control 6",     /*75*/
 "Sound Control 7",   "Sound Control 8",     /*77*/
 "Sound Control 9",   "Sound Control 10",    /*79*/
 "GP Button 1",       "GP Button 2",         /*81*/
 "GP Button 3",       "GP Button 4",         /*83*/
 "unknown",           "unknown",             /*85*/
 "unknown",           "unknown",             /*87*/
 "unknown",           "unknown",             /*89*/
 "unknown",           "Effects Level",       /*91*/
 "Tremolo Level",     "Chorus Level",        /*93*/
 "Celeste Level",     "Phaser Level",        /*95*/
 "Data button increment",  "Data button decrement", /*97*/
 "NRP (fine)",        "NRP (coarse)",        /*99*/
 "Registered parameter (fine)", "Registered parameter (coarse)", /*101*/
 "unknown",           "unknown",             /*103*/
 "unknown",           "unknown",             /*105*/
 "unknown",           "unknown",             /*107*/
 "unknown",           "unknown",             /*109*/
 "unknown",           "unknown",             /*111*/
 "unknown",           "unknown",             /*113*/
 "unknown",           "unknown",             /*115*/
 "unknown",           "unknown",             /*117*/
 "unknown",           "unknown",             /*119*/
 "All Sound Off",     "All Controllers Off", /*121*/
 "Local Keyboard (on/off)","All Notes Off",  /*123*/
 "Omni Mode Off",     "Omni Mode On",        /*125*/
 "Mono Operation",    "Poly Operation"};

/*  if (onlychan >=0 && chan != onlychan) return; */
  if (prtime(timeunits)) return;

  printf("CntlParm %2d %s = %d\n",chan+1, ctype[control],value);
}

void stats_parameter(chan,control,value)
int chan, control, value;
{
/*if (control == 7) {
  printf("cntrlvolume %d %d \n",chan+1,value);
  }
*/
trkdata.cntlparam[chan+1]++;
}

void mftxt_metatext(type,leng,mess)
int type, leng;
char *mess;
{
  static char *ttype[] = {
    NULL,
    "Text Event",    /* type=0x01 */
    "Copyright Notice",  /* type=0x02 */
    "Seqnce/Track Name",
    "Instrument Name",  /* ...       */
    "Lyric",
    "Marker",
    "Cue Point",    /* type=0x07 */
    "Unrecognized"
  };
  int unrecognized = (sizeof(ttype)/sizeof(char *)) - 1;
  int len;
  register int n, c;
  register char *p = mess;

  if ( type < 1 || type > unrecognized )
    type = unrecognized;
  if (prtime(timeunits)) return;
  printf("Metatext (%s) ",ttype[type]);
  len = leng;
  if (len > 15) len = 15;
  for ( n=0; n<len; n++ ) {
    c = (*p++) & 0xff;
    if(iscntrl(c)) {printf(" \\0x%02x",c); continue;} /* no <cr> <lf> */
    printf( (isprint(c)||isspace(c)) ? "%c" : "\\0x%02x" , c);
  }
  if (leng>15) printf("...");
  printf("\n");
}


void stats_metatext(type,leng,mess)
int type, leng;
char *mess;
{
int i; 
if (type != 3) return;
printf("metatext %d ",type);
for (i=0;i<leng;i++) printf("%c",mess[i]);
printf("\n");
}


void mftxt_keysig(sf,mi)
int sf, mi;
{
  static char *major[] = {"Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F",
    "C", "G", "D", "A", "E", "B", "F#", "C#"};
  static char *minor[] = {"Abmin", "Ebmin", "Bbmin", "Fmin", "Cmin",
    "Gmin", "Dmin", "Amin", "Emin", "Bmin", "F#min", "C#min", "G#min"};
  int index;
  index = sf + 7;
  if (prtime(timeunits)) return;
  if (mi)
    printf("Metatext key signature %s (%d/%d)\n",minor[index],sf,mi);
  else
    printf("Metatext key signature %s (%d/%d)\n",major[index],sf,mi);
}


/* [SS] 2018-01-02 */
void stats_keysig(sf,mi)
int sf, mi;
{
  float beatnumber;
  static char *major[] = {"Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F",
    "C", "G", "D", "A", "E", "B", "F#", "C#"};
  static char *minor[] = {"Abmin", "Ebmin", "Bbmin", "Fmin", "Cmin",
    "Gmin", "Dmin", "Amin", "Emin", "Bmin", "F#min", "C#min", "G#min"};
  int index;
  beatnumber = Mf_currtime/division;
  index = sf + 7;
  if (index < 0 || index >12) return;
  if (mi)
    printf("keysig %s %d %d %6.2f\n",minor[index],sf,mi,beatnumber);
  else
    printf("keysig %s %d %d %6.2f\n",major[index],sf,mi,beatnumber);
}


void mftxt_tempo(ltempo)
long ltempo;
{
  tempo = ltempo;
  if (prtime(timeunits)) return;
  printf("Metatext tempo = %6.2f bpm\n",60000000.0/tempo);
}

/* [SS] 2018-01-02 */
void stats_tempo(ltempo)
long ltempo;
{
  float beatnumber;
  tempo = ltempo;
  beatnumber = Mf_currtime/division;
  if (trkdata.tempo[0] == 0) printf("tempo %6.2f bpm\n",60000000.0/tempo);
  else if (trkdata.tempo[0] < 10) printf("ctempo  %6.2f %6.2f\n",60000000.0/tempo,beatnumber);
  trkdata.tempo[0]++;
}

void mftxt_timesig(nn,dd,cc,bb)
int nn, dd, cc, bb;
{
  int denom = 1;
  while ( dd-- > 0 )
    denom *= 2;
  if (prtime(timeunits)) return;
  printf("Metatext time signature=%d/%d\n",nn,denom);
/*  printf("Time signature=%d/%d  MIDI-clocks/click=%d \
  32nd-notes/24-MIDI-clocks=%d\n", nn,denom,cc,bb); */
}

void stats_timesig(nn,dd,cc,bb)
int nn, dd, cc, bb;
{
  float beatnumber;
  int denom = 1;
  beatnumber = Mf_currtime/division;
  while ( dd-- > 0 )
    denom *= 2;
  printf("timesig %d/%d %6.2f\n",nn,denom,beatnumber);
} 

void mftxt_smpte(hr,mn,se,fr,ff)
int hr, mn, se, fr, ff;
{
  if (prtime(timeunits)) return;
  printf("Metatext SMPTE, %d:%d:%d  %d=%d\n", hr,mn,se,fr,ff);
}

void mftxt_metaeot()
{
  if (prtime(timeunits)) return;
  printf("Meta event, end of track\n");
}


void initfunc_for_midinotes()
{
    Mf_error = error;
    Mf_header = print_txt_header;
    Mf_trackstart = no_op0;
    Mf_trackend = txt_trackend;
    Mf_noteon = print_txt_noteon;
    Mf_noteoff = print_txt_noteoff;
    Mf_pressure = no_op3;
    Mf_parameter = no_op3;
    Mf_pitchbend = no_op3;
    Mf_program = print_txt_program;
    Mf_chanpressure = no_op3;
    Mf_sysex = no_op2;
    Mf_metamisc = no_op3;
    Mf_seqnum = no_op1;
    Mf_eot = no_op0;
    Mf_timesig = no_op4;
    Mf_smpte = no_op5;
    Mf_tempo = no_op1;
    Mf_keysig = no_op2;
    Mf_seqspecific = no_op3;
    Mf_text = no_op3;
    Mf_arbitrary = no_op2;
}


void initfunc_for_mftext()
{
    Mf_error = error;
    Mf_header = mftxt_header;
    Mf_trackstart = mftxt_trackstart;
    Mf_trackend = txt_trackend;
    Mf_noteon = mftxt_noteon;
    Mf_noteoff = mftxt_noteoff;
    Mf_pressure =mftxt_pressure;
    Mf_parameter = mftxt_parameter;
    Mf_pitchbend = mftxt_pitchbend;
    Mf_program = mftxt_program;
    Mf_chanpressure = mftxt_pressure;
    Mf_sysex = no_op2;
    Mf_metamisc = no_op3;
    Mf_seqnum = no_op1;
    Mf_eot = mftxt_metaeot;
    Mf_timesig = mftxt_timesig;
    Mf_smpte = mftxt_smpte;
    Mf_tempo = mftxt_tempo;
    Mf_keysig = mftxt_keysig;
    Mf_seqspecific = no_op3;
    Mf_text = mftxt_metatext;
    Mf_arbitrary = no_op2;
}

void initfunc_for_stats()
{
    Mf_error = stats_error; /* [SS] 2017-11-19 */
    Mf_header = stats_header;
    Mf_trackstart = stats_trackstart;
    Mf_trackend = stats_trackend;
    Mf_noteon = stats_noteon;
    Mf_noteoff = stats_noteoff;
    Mf_pressure = no_op3;
    Mf_parameter = stats_parameter;
    Mf_pitchbend = stats_pitchbend;
    Mf_program = stats_program;
    Mf_chanpressure = stats_pressure;
    Mf_sysex = no_op2;
    Mf_metamisc = no_op3;
    Mf_seqnum = no_op1;
    Mf_eot = no_op0;
    Mf_timesig = stats_timesig;
    Mf_smpte = no_op5;
    Mf_tempo = stats_tempo;
    Mf_keysig = stats_keysig;
    Mf_seqspecific = no_op3;
    Mf_text = stats_metatext;
    Mf_arbitrary = no_op2;
}



void initfuncs()
{
    Mf_error = error;
    Mf_header =  txt_header;
    Mf_trackstart =  txt_trackstart;
    Mf_trackend =  txt_trackend;
    Mf_noteon =  txt_noteon;
    Mf_noteoff =  txt_noteoff;
    Mf_pressure =  txt_pressure;
    Mf_parameter =  txt_parameter;
    Mf_pitchbend =  txt_pitchbend;
    Mf_program =  txt_program;
    Mf_chanpressure =  txt_pressure;
    Mf_sysex =  txt_sysex;
    Mf_metamisc =  txt_metamisc;
    Mf_seqnum =  txt_metaseq;
    Mf_eot =  txt_metaeot;
    Mf_timesig =  txt_timesig;
    Mf_smpte =  txt_smpte;
    Mf_tempo =  txt_tempo;
    Mf_keysig =  txt_keysig;
    Mf_seqspecific =  txt_metaspecial;
    Mf_text =  txt_metatext;
    Mf_arbitrary =  txt_arbitrary;
}


/*  Stage 2 Quantize MIDI tracks. Get key signature, time signature...   */ 


void postprocess(trackno)
/* This routine calculates the time interval before the next note */
/* called after the MIDI file has been read in */
int trackno;
{
  struct listx* i;

  i = track[trackno].head;
  if (i != NULL) {
    track[trackno].startwait = i->note->time;
  } 
  else {
    track[trackno].startwait = 0;
  };
  while (i != NULL) {
    if (i->next != NULL) {
      i->note->dtnext = i->next->note->time - i->note->time;
    } 
    else {
      i->note->dtnext = i->note->tplay;
    };
    i = i->next;
  };
}

void scannotes(trackno)
int trackno;
/* diagnostic routine to output notes in a track */
{
  struct listx* i;

  i = track[trackno].head;
  while (i != NULL) {
    printf("Pitch %d chan %d vel %d time %ld  %ld xnum %d playnum %d\n",
            i->note->pitch, i->note->chan, 
            i->note->vel, i->note->dtnext, i->note->tplay,
            i->note->xnum, i->note->playnum);
    i = i->next;
  };
}


int xnum_to_next_nonchordal_note(fromitem,spare,quantum)
struct listx* fromitem;
int spare,quantum;
{
struct anote* jnote;
struct listx* nextitem;
int i,xxnum;
jnote = fromitem->note;
if (jnote->xnum > 0) return jnote->xnum;
i = 0;
nextitem = fromitem->next;
while (nextitem != NULL && i < 5) {
  jnote = nextitem->note;
  xxnum = (2*(jnote->dtnext + spare + (quantum/4)))/quantum;
  if (xxnum > 0) return xxnum;
  i++;
  nextitem = nextitem->next;
  }
return 0;
}

int quantize(trackno, xunit)
/* Work out how long each note is in musical time units.
 * The results are placed in note.playnum              */
int trackno, xunit;
{
  struct listx* j;
  struct anote* this;
  int spare;
  int toterror;
  int quantum;
  int posnum,xxnum;

  /* fix to avoid division by zero errors in strange MIDI */
  if (xunit == 0) {
    return(10000);
  };
  quantum = (int) (2.*xunit/parts_per_unitlen); /* xunit assume 2 parts_per_unit */
  track[trackno].startunits = (2*(track[trackno].startwait + (quantum/4)))/quantum;
  spare = 0;
  toterror = 0;
  j = track[trackno].head;
  posnum = 0;
  while (j != NULL) {
    this = j->note;
    /* this->xnum is the quantized inter onset time */
    /* this->playnum is the quantized note length   */
    this->xnum = (2*(this->dtnext + spare + (quantum/4)))/quantum;
    this->playnum = (2*(this->tplay + (quantum/4)))/quantum;
    if ((this->playnum == 0) && (keep_short)) {
      this->playnum = 1;
    };
    /* In the event of short rests, the inter onset time
     * will be larger than the note length. However, for
     * chords the inter onset time can be zero.          */
    xxnum =  xnum_to_next_nonchordal_note(j,spare,quantum);
    if ((swallow_rests>=0) && (xxnum - this->playnum <= restsize)
		        && xxnum > 0) {
      this->playnum = xxnum;
    };
   /* this->denom = parts_per_unitlen;  this variable is never used ! */
    spare = spare + this->dtnext - (this->xnum*xunit/parts_per_unitlen);
    if (spare > 0) {
      toterror = toterror + spare;
    } 
    else {
      toterror = toterror - spare;
    };
    /* gradually forget old errors so that if xunit is slightly off,
       errors don't accumulate over several bars */
    spare = (spare * 96)/100;
    this->posnum = posnum;
    posnum += this->xnum;
    j = j->next;
  };
  return(toterror);
}


void guesslengths(trackno)
/* work out most appropriate value for a unit of musical time */
int trackno;
{
  int i;
  int trial[100];
  float avlen, factor, tryx;
  long min;

  min = track[trackno].tracklen;
  if (track[trackno].notes == 0) {
    return;
  };
  avlen = ((float)(min))/((float)(track[trackno].notes));
  tryx = avlen * (float) 0.75;
  factor = tryx/100;
  for (i=0; i<100; i++) {
    trial[i] = quantize(trackno, (int) tryx);
    if ((long) trial[i] < min) {
      min = (long) trial[i];
      xunit = (int) tryx;
    };
    tryx = tryx + factor;
  };
xunit_set = 1;
}


int findana(maintrack, barsize)
/* work out anacrusis from MIDI */
/* look for a strong beat marking the start of a bar */
int maintrack;
int barsize;
{
  int min, mincount;
  int place;
  struct listx* p;

  min = 0;
  mincount = 0;
  place = 0;
  p = track[maintrack].head;
  while ((p != NULL) && (place < barsize)) {
    if ((p->note->vel > min) && (place > 0)) {
      min = p->note->vel;
      mincount = place;
    };
    place = place + (p->note->xnum);
    p = p->next;
  };
  return(mincount);
}



int guessana(barbeats)
int barbeats;
/* try to guess length of anacrusis */
{
  int score[64];
  int min, minplace;
  int i,j;

  if (barbeats > 64) {
    fatal_error("Bar size exceeds static limit of 64 units!");
  };
  for (j=0; j<barbeats; j++) {
    score[j] = 0;
    for (i=0; i<trackcount; i++) {
      score[j] = score[j] + testtrack(i, barbeats, j);
      /* restore values to num */
      quantize(i, xunit);
    };
  };
  min = score[0];
  minplace = 0;
  for (i=0; i<barbeats; i++) {
    if (score[i] < min) {
      min = score[i];
      minplace = i;
    };
  };
  return(minplace);
}


int findkey(maintrack)
int maintrack;
/* work out what key MIDI file is in */
/* algorithm is simply to minimize the number of accidentals needed. */
{
  int j;
  int max, min, n[12], key_score[12];
  int minkey, minblacks;
  static int keysharps[12] = {
	  0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5};
  struct listx* p;
  int thispitch;
  int lastpitch;
  int totalnotes;

  /* analyse pitches */
  /* find key */
  for (j=0; j<12; j++) {
    n[j] = 0;
  };
  min = track[maintrack].tail->note->pitch;
  max = min;
  totalnotes = 0;
  for (j=0; j<trackcount; j++) {
    totalnotes = totalnotes + track[j].notes;
    p = track[j].head;
    while (p != NULL) {
      thispitch = p->note->pitch;
      if (thispitch > max) {
        max = thispitch;
      } 
      else {
        if (thispitch < min) {
          min = thispitch;
        };
      };
      n[thispitch % 12] = n[thispitch % 12] + 1;
      p = p->next;
    };
  };
  /* count black notes for each key */
  /* assume pitch = 0 is C */
  minkey = 0;
  minblacks = totalnotes;
  for (j=0; j<12; j++) {
    key_score[j] = n[(j+1)%12] + n[(j+3)%12] + n[(j+6)%12] +
                   n[(j+8)%12] + n[(j+10)%12];
    /* printf("Score for key %d is %d\n", j, key_score[j]); */
    if (key_score[j] < minblacks) {
      minkey = j;
      minblacks = key_score[j];
    };
  };
  /* do conversion to abc pitches */
  /* Code changed to use absolute rather than */
  /* relative choice of pitch for 'c' */
  /* MIDDLE = (min + (max - min)/2 + 6)/12 * 12; */
  /* Do last note analysis */
  lastpitch = track[maintrack].tail->note->pitch;
  if (minkey != (lastpitch%12)) {
    fprintf(outhandle,"%% Last note suggests ");
    switch((lastpitch+12-minkey)%12) {
    case(2):
      fprintf(outhandle,"Dorian ");
      break;
    case(4):
      fprintf(outhandle,"Phrygian ");
      break;
    case(5):
      fprintf(outhandle,"Lydian ");
      break;
    case(7):
      fprintf(outhandle,"Mixolydian ");
      break;
    case(9):
      fprintf(outhandle,"minor ");
      break;
    case(11):
      fprintf(outhandle,"Locrian ");
      break;
    default:
      fprintf(outhandle,"unknown ");
      break;
    };
    fprintf(outhandle,"mode tune\n");
  };
  /* switch to minor mode if it gives same number of accidentals */
  if ((minkey != ((lastpitch+3)%12)) && 
      (key_score[minkey] == key_score[(lastpitch+3)%12])) {
         minkey = (lastpitch+3)%12;
  };
  /* switch to major mode if it gives same number of accidentals */
  if ((minkey != (lastpitch%12)) && 
      (key_score[minkey] == key_score[lastpitch%12])) {
         minkey = lastpitch%12;
  };
  sharps = keysharps[minkey];
  return(sharps);
}




/* Stage 3  output MIDI tracks in abc format                        */


/* head and tail of list of notes in current chord playing */
/* used while abc is being generated */
struct dlistx* chordhead;
struct dlistx* chordtail;


void printchordlist()
/* diagnostic routine */
{
  struct dlistx* i;

  i = chordhead;
  printf("----CHORD LIST------\n");
  while(i != NULL) {
    printf("pitch %d len %d\n", i->note->pitch, i->note->playnum);
    if (i->next == i) {
      fatal_error("Loopback problem!");
    };
    i = i->next;
  };
}

void checkchordlist()
/* diagnostic routine */
/* validates data structure */
{
  struct dlistx* i;
  int n;

  if ((chordhead == NULL) && (chordtail == NULL)) {
    return;
  };
  if ((chordhead == NULL) && (chordtail != NULL)) {
    fatal_error("chordhead == NULL and chordtail != NULL");
  };
  if ((chordhead != NULL) && (chordtail == NULL)) {
    fatal_error("chordhead != NULL and chordtail == NULL");
  };
  if (chordhead->last != NULL) {
    fatal_error("chordhead->last != NULL");
  };
  if (chordtail->next != NULL) {
    fatal_error("chordtail->next != NULL");
  };
  i = chordhead;
  n = 0;
  while((i != NULL) && (i->next != NULL)) {
    if (i->next->last != i) {
      char msg[80];

      sprintf(msg, "chordlist item %d : i->next->last!", n);
      fatal_error(msg);
    };
    i = i->next;
    n = n + 1;
  };
  /* checkchordlist(); */
}

void addtochord(p)
/* used when printing out abc */
struct anote* p;
{
  struct dlistx* newx;
  struct dlistx* place;

  if (debug > 3) printf("addtochord %d\n",p->pitch);

  newx = (struct dlistx*) checkmalloc(sizeof(struct dlistx));
  newx->note = p;
  newx->next = NULL;
  newx->last = NULL;

  if (chordhead == NULL) {
    chordhead = newx;
    chordtail = newx;
    checkchordlist();
    return;
  };
  place = chordhead;
  while ((place != NULL) && (place->note->pitch > p->pitch)) {
    place = place->next;
  };
  if (place == chordhead) {
    newx->next = chordhead;
    chordhead->last = newx;
    chordhead = newx;
    checkchordlist();
    return;
  };
  if (place == NULL) {
    newx->last = chordtail;
    chordtail->next = newx;
    chordtail = newx;
    checkchordlist();
    return;
  };
  newx->next = place;
  newx->last = place->last;
  place->last = newx;
  newx->last->next = newx;
  checkchordlist();
}

struct dlistx* removefromchord(i)
/* used when printing out abc */
struct dlistx* i;
{
  struct dlistx* newi;

  /* remove note from list */
  if (i->last == NULL) {
    chordhead = i->next;
  } 
  else {
    (i->last)->next = i->next;
  };
  if (i->next == NULL) {
    chordtail = i->last;
  } 
  else {
    (i->next)->last = i->last;
  };
  newi = i->next;
  free(i);
  checkchordlist();
  return(newi);
}

int findshortest(gap)
/* find the first note in the chord to terminate */
int gap;
{
  int min, v;
  struct dlistx* p;

  p = chordhead;
  min = gap;
  while (p != NULL) {
    v = p->note->playnum;
    if (v < min) {
      min = v;
    };
    p = p->next;
  };
  if (debug >3) printf("find_shortest %d --> %d\n",gap,min);
  return(min);
}

void advancechord(len)
/* adjust note lengths for all notes in the chord */
int len;
{
  struct dlistx* p;

  if (debug > 3 && len >0) printf("\nadvancechord %d\n",len);
  p = chordhead;
  while (p != NULL) {
    if (p->note->playnum <= len) {
      if (p->note->playnum < len) {
        fatal_error("Error - note too short!");
      };
      /* remove note */
      checkchordlist();
      p = removefromchord(p);
    } 
    else {
      /* shorten note */
      p->note->playnum = p->note->playnum - len;
      p = p->next;
    };
  };
}

void freshline()
/* if the current line of abc or text is non-empty, start a new line */
{
  if (newline_flag) return; /* [SS] 2019-06-14 */
  if (midline == 1) {
    fprintf(outhandle,"\n");
    midline = 0;
  };
  newline_flag = 1; /* [SS] 2019-06-14 */
}


void printnote (struct listx *i)
{
      printf("%ld ",i->note->time);
      printpitch(i->note);
      printfract(i->note->playnum, parts_per_unitlen);
      printf(" %d %d %d %d\n",i->note->xnum, i->note->playnum,
       i->note->posnum,i->note->splitnum); 
}

void listnotes(int trackno, int start, int end)
/* A diagnostic like scannotes. I usually call it when
   I am in the debugger (for example in printtrack).
*/
{
struct listx* i;
int k;
i = track[trackno].head;
k = 0;
printf("ticks pitch xnum,playnum,posnum,splitnum\n");
while (i != NULL && k < end)
  {
  if (k >= start) 
    printnote(i);
  k++;
  i = i->next;
  }
}

/* [SS] 2019-05-29 new. To see what is going on. I usually
 * call it from the debugger just before printtrack().
 */
void listtrack_sizes () 
{
int i;
for (i =0;i<16;i++) {
  printf("track %d = %d notes %d texts\n",i,track[i].notes,track[i].texts);
  }
}


void pitch_percentiles (int trackno, int * ten, int * ninety)
{
int histogram[128];
struct listx* i;
struct anote* n;
int j,p;
float qprob,total;
for (j=0;j<127;j++) histogram[j] = 0;
i = track[trackno].head;
while (i != NULL)
  {
  n = i->note;
  p = (int) n->pitch;
  if (p >-1 && p < 127) histogram[p]++;
  i = i->next;
  }
/*printf("pitch histogram ");
for (j=0;j<127;j++) printf(" %d",histogram[j]);
printf("\n");
*/
for (j=0;j<127;j++)
  histogram[j+1] = histogram[j+1] + histogram[j];
*ten = 0;
*ninety = 0;
if (histogram[126] == 0) return;
total = (float) histogram[126];
for (j=0;j<127;j++) {
 qprob = (float) histogram[j]/total;
 if (qprob < 0.1) *ten = j;
 if (qprob < 0.9) *ninety = j;
 }
/* printf("10 and 90 percentiles = %d %d\n",*ten,*ninety); */
}



int testtrack(trackno, barbeats, anacrusis)
/* print out one track as abc */
int trackno, barbeats, anacrusis;
{
  struct listx* i;
  int step, gap;
  int barnotes;
  int barcount;
  int breakcount;

  breakcount = 0;
  chordhead = NULL;
  chordtail = NULL;
  i = track[trackno].head;
  gap = 0;
  if (anacrusis > 0) {
    barnotes = anacrusis;
  } 
  else {
    barnotes = barbeats;
  };
  barcount = 0;
  while((i != NULL)||(gap != 0)) {
    if (gap == 0) {
      /* add notes to chord */
      addtochord(i->note);
      gap = i->note->xnum;
      i = i->next;
      advancechord(0); /* get rid of any zero length notes */
    } 
    else {
      step = findshortest(gap);
      if (step > barnotes) {
        step = barnotes;
      };
      if (step == 0) {
        fatal_error("Advancing by 0 in testtrack!");
      };
      advancechord(step);
      gap = gap - step;
      barnotes = barnotes - step;
      if (barnotes == 0) {
        if (chordhead != NULL) {
          breakcount = breakcount + 1;
        };
        barnotes = barbeats;
        barcount = barcount + 1;
        if (barcount>0  && barcount%4 ==0) {
        /* can't zero barcount because I use it for computing maxbarcount */
          freshline();
          barcount = 0;
        };
      };
    };
  };
  return(breakcount);
}

void printpitch(j)
/* convert numerical value to abc pitch */
struct anote* j;
{
  int p, po,i;

  p = j->pitch;
  if (p == -1) {
    fprintf(outhandle,"z");
  } 
  else {
    po = p % 12;
    /* if ((back[trans[p]] != p) || (key[po] == 1)) { [SS] 2010-05-08 */
    if (back[trans[p]] != p ) {  /* [SS] 2019-05-08 */
      fprintf(outhandle,"%c%c", symbol[po], atog[p]);
      for (i=p%12; i<256; i += 12) /* apply accidental to all octaves */
         back[trans[i]] = i;
    } 
    else {
      fprintf(outhandle,"%c", atog[p]);
    };
    while (p >= MIDDLE + 12) {
      fprintf(outhandle,"'");
      p = p - 12;
    };
    while (p < MIDDLE - 12) {
      fprintf(outhandle,",");
      p = p + 12;
    };
  };
}

static void reduce(a, b)
int *a, *b;
{
  int t, n, m;

    /* find HCF using Euclid's algorithm */
    if (*a > *b) {
        n = *a;
        m = *b;
      }
    else {
        n = *b;
        m = *a;
       };
while (m != 0) {
      t = n % m;
      n = m;
      m = t;
    };
*a = *a/n;
*b = *b/n;
}



void printfract(a, b)
/* print fraction */
/* used when printing abc */
int a, b;
{
  int c, d;

  c = a;
  d = b;
  reduce(&c,&d);
  /* print out length */
  if (c != 1) {
    fprintf(outhandle,"%d", c);
  };
  if (d != 1) {
    fprintf(outhandle,"/%d", d);
  };
}

void printchord(len)
/* Print out the current chord. Any notes that haven't            */
/* finished at the end of the chord are tied into the next chord. */
int len;
{
  struct dlistx* i;

  i = chordhead;
  if (i == NULL) {
    /* no notes in chord */
#ifdef zSPLITCODE
    fprintf(outhandle,"x");
#else
    fprintf(outhandle,"z");
#endif
    printfract(len, parts_per_unitlen);
    midline = 1;
  } 
  else {
    if (i->next == NULL) {
      /* only one note in chord */
      printpitch(i->note);
      printfract(len, parts_per_unitlen);
      midline = 1;
      if (len < i->note->playnum) {
        fprintf(outhandle,"-");
      };
    } 
    else {
      fprintf(outhandle,"[");
      while (i != NULL) {
        printpitch(i->note);
        /* printfract(len, parts_per_unitlen); [SS] 2020-01-06 */
        if (len < i->note->playnum) {
          fprintf(outhandle,"-");
        };
      if (nogr && i->next != NULL) fprintf(outhandle," ");
        i = i->next;
      };
      fprintf(outhandle,"]");
      printfract(len, parts_per_unitlen); /* [SS] 2020-01-06 */
      midline = 1;
    };
  };
  newline_flag = 0; /* [SS] 2019-06-14 */
}

char dospecial(i, barnotes, featurecount,allow_broken,allow_triplets)
/* identify and print out triplets and broken rhythm */
struct listx* i;
int* barnotes;
int* featurecount;
int allow_broken,allow_triplets;
{
  int v1, v2, v3, vt;
  int xa, xb;
  int pnum;
  long total, t1, t2, t3;

  
  if ((chordhead != NULL) || (i == NULL) || (i->next == NULL)
  /* || (asig%3 == 0) || (asig%2 != 0) 2004/may/09 SS*/) {
    return(' ');
  };
  t1 = i->note->dtnext;
  v1 = i->note->xnum;
  pnum = i->note->playnum;
  if ((v1 < pnum) || (v1 > 1 + pnum) || (pnum == 0)) {
    return(' ');
  };
  t2 = i->next->note->dtnext;
  v2 = i->next->note->xnum;
  pnum = i->next->note->playnum;
  if (/*(v2 < pnum) ||*/ (v2 > 1 + pnum) || (pnum == 0) || (v1+v2 > *barnotes)) {
    return(' ');
  };
  /* look for broken rhythm */
  total = t1 + t2;
  if (total == 0L) {
    /* shouldn't happen, but avoids possible divide by zero */
    return(' ');
  };
  /* [SS] 2017-01-01 */
  if (allow_broken == 1 && ((v1+v2)%2 == 0) && ((v1+v2)%3 != 0)) {
    vt = (v1+v2)/2;
      if (vt == validnote(vt)) {
      /* do not try to break a note which cannot be legally expressed */
      switch ((int) ((t1*6+(total/2))/total)) {
        case 2:
          *featurecount = 2;
          i->note->xnum  = vt;
          i->note->playnum = vt;
          i->next->note->xnum  = vt;
          i->next->note->playnum = vt;
          return('<');
          break;
        case 4:
          *featurecount = 2;
          i->note->xnum  = vt;
          i->note->playnum = vt;
          i->next->note->xnum  = vt;
          i->next->note->playnum = vt;
          return('>');
          break;
        default:
          break;
      };
    };
  };
  if (allow_triplets == 0) return(' '); /* [SS] 2017-01-01 */
  /* look for triplet */
  if (i->next->next != NULL) {
    t3 = i->next->next->note->dtnext;
    v3 = i->next->next->note->xnum;
    pnum = i->next->next->note->playnum;
    if ((v3 < pnum) || (v3 > 1 + pnum) || (pnum == 0) || 
        (v1+v2+v3 > *barnotes)) {
      return(' ');
    };
    if ((v1+v2+v3)%2 != 0) {
      return(' ');
    };
    vt = (v1+v2+v3)/2;
    if ((vt%2 == 1) && (vt > 1)) {
      /* don't want strange fractions in triplet */
      return(' ');
    };
    total = t1+t2+t3;
    xa = (int) ((t1*6+(total/2))/total); 
    xb = (int) (((t1+t2)*6+(total/2))/total);
    if ((xa == 2) && (xb == 4) && (vt%3 != 0) ) {
      *featurecount = 3;
      *barnotes = *barnotes + vt;
      i->note->xnum = vt;
      i->note->playnum = vt;
      i->next->note->xnum = vt;
      i->next->note->playnum = vt;
      i->next->next->note->xnum = vt;
      i->next->next->note->playnum = vt;
    };
  };
  return(' ');
}

int validnote(n)
int n;
/* work out a step which can be expressed as a musical time */
{
  int v;

  if (n <= 4) {
    v = n;
  } 
  else {
    v = 4;
    while (v*2 <= n) {
      v = v*2;
    };
    if (v + v/2 <= n) {
      v = v + v/2;
    };
  };
  return(v);
}

void handletext(t, textplace, trackno)
/* print out text occuring in the body of the track */
/* The text is printed out at the appropriate place within the track */
/* In addition the function handles key signature and time */
/* signature changes that can occur in the middle of the tune. */
long t;
struct tlistx** textplace;
int trackno;
{
  char* str;
  char ch;
  int type,sf,mi,nn,denom,bb;

  while (((*textplace) != NULL) && ((*textplace)->when <= t)) {
    str = (*textplace)->text;
    ch = *str;
    type = (*textplace)->type;
    if (type == 5 && noly == 1) {  /* [SS] 2019-07-12 */
        *textplace = (*textplace)->next;
	return;
        }

    remove_carriage_returns(str);
    if (((int)ch == '\\') || ((int)ch == '/')) {
      inkaraoke = 1;
    };
    if ((inkaraoke == 1) && (karaoke == 1)) {
      switch(ch) {
        case ' ':
          fprintf(outhandle,"%s", str);
          midline = 1;
          break;
        case '\\':
          freshline();
          fprintf(outhandle,"w:%s", str + 1);
          midline = 1;
          break;
        case '/':
          freshline();
          fprintf(outhandle,"w:%s", str + 1);
          midline = 1;
          break;
        default :
          if (midline == 0) {
            fprintf(outhandle,"%%%s", str);
          } 
	  else {
            fprintf(outhandle,"-%s", str);
          };
          break;
      };
      newline_flag = 0; /* [SS] 2019-06-14 */
    } 
    else {
      freshline();
      ch=*(str+1);
      switch (type) {
      case 0:
      if (ch != '%') 
      fprintf(outhandle,"%%%s\n", str);
      else 
      fprintf(outhandle,"%s\n", str);
      break;
      case 1: /* key signature change */
      sscanf(str,"%d %d",&sf,&mi);
      if((trackno != 0 || trackcount==1) &&
	 (active_keysig != sf)) {
	      setupkey(sf);
	      active_keysig=sf;
              }
      break;
      case 2: /* time signature change */
      sscanf(str,"%d %d %d",&nn,&denom,&bb);
      if ((trackno != 0 || trackcount ==1) &&
	  (active_asig != nn || active_bsig != denom))
        {
  	setup_timesig(nn,denom,bb);
	fprintf(outhandle,"M: %d/%d\n",nn,denom);
        fprintf(outhandle,"L: 1/%d\n",unitlen);
	active_asig=nn;
	active_bsig=denom;
	}
      break;
      case 5: /* lyric [SS] 2019-07-12*/
      if (ch != '%') 
      fprintf(outhandle,"%%%s\n", str);
      else 
      fprintf(outhandle,"%s\n", str);
      break;
     default:
      break;
      }
      newline_flag = 1; /* 2019-06-14 */
  }
  *textplace = (*textplace)->next;
 }
}



/* This function identifies irregular chords, (notes which
   do not exactly overlap in time). The notes in the
   chords are split into separate lines (split numbers).
   The xnum (delay) to next note is updated.
*/

int splitstart[10],splitend[10]; /* used in forming chords from notes*/
int lastposnum[10]; /* posnum of previous note in linked list */
int endposnum; /* posnum at last note in linked list */
struct anote* prevnote[10]; /*previous note in linked list */
struct listx*  last_i[10];  /*note after finishing processing bar*/
int existingsplits[10]; /* existing splits in active bar */
struct dlistx* splitchordhead[10]; /* chordhead list for splitnum */
struct dlistx* splitchordtail[10]; /* chordtail list for splitnum */
int splitgap[10]; /* gap to next note at end of split measure */

void label_split(struct anote *note, int activesplit)
{
/* The function assigns a split number (activesplit), to
   a specific note, (*note). We also update splitstart
   and splitend which specifies the region in time where
   the another note must occur if it forms a proper chord.
   After assigning a split number to the note we need to
   update note->xnum as this indicates the gap to the
   next note in the same split number. The function uses
   a greedy algorithm. It assigns a note to the first
   splitnumber code which satisfies the above constraint.
   If it cannot find a splitnumber, a new one (voice or track)
   is created. It would be nice if the voices kept the
   high and low notes (in pitch) separate.
*/
     note->splitnum = activesplit;
     splitstart[activesplit] = note->posnum;
     splitend[activesplit] = splitstart[activesplit] + note->playnum;
     if (prevnote[activesplit]) 
         prevnote[activesplit]->xnum = note->posnum - lastposnum[activesplit];
     lastposnum[activesplit] = note->posnum;
     prevnote[activesplit] = note;
     /* in case this is the last activesplit note make sure it
        xnum points to end of track. Otherwise it will be changed
        when the next activesplit note is labeled.
     */
     note->xnum = endposnum - note->posnum;
     existingsplits[activesplit]++;
}


int label_split_voices (int trackno)
{
/* This function sorts all the notes in the track into
   separate split part. A note is placed into a separate
   part if it forms a chord in the current part but
   does not have the same onset time and same end time.
   If this occurs, we search for another part where
   this does not happen. If we can't find such a part
   a new part (split) is created.
   The heuristic used needs to be improved, so
   that split number 0 always contains notes and so
   that notes in the same pitch range or duration are
   given the same split number.
*/ 
int activesplit,nsplits;
int done;
struct listx* i;
int k;
int firstposnum = 0; /* [SDG] 2020-06-03 */
/* initializations */
activesplit = 0;
nsplits = 0;
if (debug > 4) printf("label_split_voices:\n");
for (k=0;k<10;k++) {
       splitstart[k]=splitend[k]=lastposnum[k]=0;
       prevnote[k] = NULL;
       existingsplits[k] = 0;
       splitgap[k]=0;
       }
i = track[trackno].head;
if (track[trackno].tail == 0x0) {return 0;}
endposnum =track[trackno].tail->note->posnum +
           track[trackno].tail->note->playnum;

if (i != NULL) label_split(i->note, activesplit);
/* now label all the notes in the track */
while (i != NULL)
  {
  done =0;
  if (nsplits == 0) { /*no splits exist, create split number 0 */
     activesplit = 0;
     nsplits++;
     i->note->splitnum = activesplit;
     splitstart[activesplit] = i->note->posnum;
     splitend[activesplit] = splitstart[activesplit] + i->note->playnum;
     firstposnum = splitstart[activesplit];
     } else {  /* do a compatibility check with the last split number */
  if (  (   i->note->posnum == splitstart[activesplit] 
         && i->note->playnum == (splitend[activesplit] - splitstart[activesplit]))
      || i->note->posnum >= splitend[activesplit])
     {
     if (existingsplits[activesplit] == 0) {
         last_i[activesplit] = i;
         splitgap[activesplit] = i->note->posnum - firstposnum;
         }
     label_split(i->note, activesplit);
     done = 1;
     }
/* need to search for any other compatible split numbers  */
  if (done == 0) for (activesplit=0;activesplit<nsplits;activesplit++) {
      if (  (   i->note->posnum == splitstart[activesplit] 
             && i->note->playnum == splitend[activesplit] - splitstart[activesplit])
          || i->note->posnum >= splitend[activesplit])
       {
        if (existingsplits[activesplit] == 0) {
           last_i[activesplit] = i;
           splitgap[activesplit] = i->note->posnum - firstposnum;
           }
        label_split(i->note,activesplit);
        done = 1;
        break;
        }
      }
     
/* No compatible split number found. Create new split */
   if (done == 0) {
     if(nsplits < 10) {nsplits++; activesplit = nsplits-1;}  
     if (existingsplits[activesplit] == 0) {
         last_i[activesplit] = i;
         splitgap[activesplit] = i->note->posnum - firstposnum;
         }
     label_split(i->note,activesplit);
     }
   } 
if (debug>2)  printf("note %d links to %d  %d (%d %d)\n",i->note->pitch,activesplit,
i->note->posnum,splitstart[activesplit],splitend[activesplit]);

  i = i->next;
  } /* end while loop */
return nsplits;
}


int nextsplitnum(int splitnum)
  {
  while (splitnum < 9) {
      splitnum++;
      if (existingsplits[splitnum]) return splitnum;
      } 
     return -1;
}


int count_splits()
{
int i,n;
n = 0;
for (i=0;i<10;i++)
  if (existingsplits[i]) n++;
return n;
}


/* [SS] 2019-06-26 */
int firstgap[10];

void set_first_gaps (int trackno) {
struct listx* i;
int j;
int start;
int splitnumber;
start = track[trackno].startunits;
for (j=0;j<10;j++) firstgap[j] = -1;
i = track[trackno].head;
while((i != NULL)) {
  splitnumber =  i->note->splitnum; 
  if (firstgap[splitnumber] < 0) {
     firstgap[splitnumber] = start + i->note->posnum;
     /* printf("split = %d gap = %d\n",splitnumber,firstgap[splitnumber]);*/
     }
  i = i->next;
  }
}


/* [SS] 2019-06-13 */
void printtrack_split (int trackno, int splitnum, int anacrusis)
  {
  struct listx* i;
  struct tlistx* textplace;
  struct tlistx* textplace0; /* track 0 text storage */
  int step, gap;
  int featurecount;
  int lastnote_in_split;
  char broken;
  int barnotes;
  int barcount;
  int last_barsize,barnotes_correction;
  long now;
  int nlines;
  int bars_on_line;

  featurecount = 0;
  lastnote_in_split = 0;
  broken = ' ';
  now = 0L;
  nlines= 0;
  bars_on_line = 0;
  chordhead = NULL;  /* [SS] 2019-06-17 */
  chordtail = NULL;
  i = track[trackno].head;
  /* gap = track[trackno].startunits; */
  gap = firstgap[splitnum];  /* [SS] 2019-06-26 */
  if (anacrusis > 0) {
    barnotes = anacrusis;
    barcount = -1;
  } 
  else {
    barnotes = barsize;
    barcount = 0;
  };
  last_barsize = barsize;

  textplace = track[trackno].texthead;
  textplace0 = track[0].texthead;
  fprintf(outhandle,"V: split%d%c\n",trackno+1,'A'+splitnum);
  newline_flag = 1; /* [SS] 2019-06-14 */

  while((i != NULL)||(gap != 0)) {
    if (gap == 0) {
      if (i->note->posnum + i->note->xnum == endposnum) lastnote_in_split = 1;
      /* do triplet here */
      if (featurecount == 0) {
        if (allow_triplets || allow_broken) {  /* [SS] 2017-01-01 */
          broken = dospecial(i, &barnotes, &featurecount,allow_broken,allow_triplets);
        };
      };
/* ignore any notes that are not in the current splitnum */
      if (i->note->splitnum == splitnum) {
               /*printf("\nadding ");
                 printnote(i); */
	addtochord(i->note);
	gap = i->note->xnum;
	now = i->note->time;
	}
      i = i->next;
      advancechord(0); /* get rid of any zero length notes */
      if (trackcount > 1 && trackno !=0 && splitnum == 0)
	      handletext(now, &textplace0, trackno);
      /* handletext(now, &textplace,trackno); 2019-06-26 */
      barnotes_correction = barsize - last_barsize;
      barnotes += barnotes_correction;
      last_barsize = barsize;
    } 
    else {
      step = findshortest(gap);
      if (step > barnotes) {
        step = barnotes;
      };
      step = validnote(step);
      if (step == 0) {
        fatal_error("Advancing by 0 in printtrack!");
      };
      if (featurecount == 3)
        {
        fprintf(outhandle," (3");
        };
      printchord(step);
      if ( featurecount > 0) {
        featurecount = featurecount - 1;
      };
      if ((featurecount == 1) && (broken != ' ')) {
        fprintf(outhandle,"%c", broken);
      };
      advancechord(step);
      gap = gap - step;
      barnotes = barnotes - step;
      if (barnotes == 0) {
        nlines++;
        if (nlines > 5000) {
            printf("\nProbably infinite loop: aborting\n");
            fprintf(outhandle,"\n\nProbably infinite loop: aborting\n");
            return;
            }
        fprintf(outhandle,"|");
	reset_back_array();
        barnotes = barsize;
        barcount = barcount + 1;
	bars_on_line++;
        if (barcount >0 && barcount%bars_per_staff == 0)  {
		freshline();
		bars_on_line=0;
	}
     /* can't zero barcount because I use it for computing maxbarcount */
        else if(bars_on_line >= bars_per_line && i != NULL) {
		if (!lastnote_in_split) fprintf(outhandle," \\");
	       	freshline();
	        bars_on_line=0;}
      }
      else if (featurecount == 0) {
          /* note grouping algorithm */
          /* [SS] 2016-07-20 waltz is a special case */
          if ((barsize/parts_per_unitlen) % 3 == 0 && asig != 3) {
            if ( (barnotes/parts_per_unitlen) % 3 == 0
               &&(barnotes%parts_per_unitlen) == 0) {
              fprintf(outhandle," ");
	      newline_flag = 0; /* [SS] 2019-06-14 */
            };
          } 
	  else {
            if (((barsize/parts_per_unitlen) % 2 == 0)
                && (barnotes % parts_per_unitlen) == 0
                && ((barnotes/parts_per_unitlen) % 2 == 0)) {
              fprintf(outhandle," ");
	      newline_flag = 0; /* [SS] 2019-06-14 */
            };
          };
      }
      if (nogr) {fprintf(outhandle," "); newline_flag = 0;}
    };
  if (barcount > maxbarcount) maxbarcount = barcount;
  }
  freshline(); /* [SS] 2019-06-17 */
}

void printtrack_split_voice(trackno, anacrusis)
/* print out one track as abc */
int trackno,  anacrusis;
{
  int splitnum;
  long now;
  int nsplits;
  int i;
  struct tlistx* textplace;
  nsplits = label_split_voices (trackno);
  /*printf("%d splits were detected\n",nsplits);*/
  set_first_gaps (trackno);  /* [SS] 2019-06-26 */
  textplace = track[trackno].texthead;
  midline = 0;
  inkaraoke = 0;
  now = 0L;
  active_asig = header_asig;
  active_bsig = header_bsig;
  setup_timesig(header_asig,header_bsig,header_bb);
  active_keysig = header_keysig;
  handletext(now, &textplace, trackno);
  splitnum = 0;
  /* gap = splitgap[splitnum]; [SS] 2019-05-12 */
  /* [SS] 2019-06-13 */
  for (i=0;i <nsplits;i++) 
     printtrack_split(trackno, i, anacrusis);
}



void printtrack(trackno, anacrusis)
/* print out one track as abc */
int trackno,  anacrusis;
{
  struct listx* i;
  struct tlistx* textplace;
  struct tlistx* textplace0; /* track 0 text storage */
  int step, gap;
  int barnotes;
  int barcount;
  int bars_on_line;
  long now;
  char broken;
  int featurecount;
  int last_barsize,barnotes_correction;

  midline = 0;
  featurecount = 0;
  inkaraoke = 0;
  now = 0L;
  broken = ' ';
  chordhead = NULL;
  chordtail = NULL;
  i = track[trackno].head;
  textplace = track[trackno].texthead;
  textplace0 = track[0].texthead;
  gap = track[trackno].startunits;
  if (anacrusis > 0) {
    barnotes = anacrusis;
    barcount = -1;
  } 
  else {
    barnotes = barsize;
    barcount = 0;
  };
  bars_on_line = 0;
  last_barsize = barsize;
  active_asig = header_asig;
  active_bsig = header_bsig;
  setup_timesig(header_asig,header_bsig,header_bb);
  active_keysig = header_keysig;
  handletext(now, &textplace, trackno);

  while((i != NULL)||(gap != 0)) {
    if (debug > 4) printf("gap = %d\n",gap);
    if (gap == 0) {
      /* do triplet here */
      if (featurecount == 0) {
        if (allow_triplets || allow_broken) {  /* [SS] 2017-01-01 */
          broken = dospecial(i, &barnotes, &featurecount,allow_broken,allow_triplets);
        };
      };
      /* add notes to chord */
      addtochord(i->note);
      gap = i->note->xnum;
      now = i->note->time;
      i = i->next;
      advancechord(0); /* get rid of any zero length notes */

      /*if (trackcount > 1 && trackno !=0)
	      handletext(now, &textplace0, trackno);
      [SS] 2019-12-30   */

      handletext(now, &textplace,trackno);
      barnotes_correction = barsize - last_barsize;
      barnotes += barnotes_correction;
      last_barsize = barsize;
    } 
    else {
      step = findshortest(gap);
      if (step > barnotes) {
        step = barnotes;
      };
      step = validnote(step);
      if (step == 0) {
        fatal_error("Advancing by 0 in printtrack!");
      };
      if (featurecount == 3)
        {
        fprintf(outhandle," (3");
        };
      printchord(step);
      if ( featurecount > 0) {
        featurecount = featurecount - 1;
      };
      if ((featurecount == 1) && (broken != ' ')) {
        fprintf(outhandle,"%c", broken);
      };
      advancechord(step);
      gap = gap - step;
      barnotes = barnotes - step;
      if (barnotes == 0) {
        fprintf(outhandle,"|");
	reset_back_array();
        barnotes = barsize;
        barcount = barcount + 1;
	bars_on_line++;
        if (barcount >0 && barcount%bars_per_staff == 0)  {
		freshline();
		bars_on_line=0;
	}
     /* can't zero barcount because I use it for computing maxbarcount */
        else if(bars_on_line >= bars_per_line && i != NULL) {
		fprintf(outhandle," \\");
	       	freshline();
	        bars_on_line=0;}
      }
      else if (featurecount == 0) {
          /* note grouping algorithm */
          /* [SS] 2016-07-20 waltz is a special case */
          if ((barsize/parts_per_unitlen) % 3 == 0 && asig != 3) {
            if ( (barnotes/parts_per_unitlen) % 3 == 0
               &&(barnotes%parts_per_unitlen) == 0) {
              fprintf(outhandle," ");
	      newline_flag = 0; /* [SS] 2019-06-14 */
            };
          } 
	  else {
            if (((barsize/parts_per_unitlen) % 2 == 0)
                && (barnotes % parts_per_unitlen) == 0
                && ((barnotes/parts_per_unitlen) % 2 == 0)) {
              fprintf(outhandle," ");
	      newline_flag = 0; /* [SS] 2019-06-14 */
            };
          };
      }
      if (nogr) {fprintf(outhandle," ");
	         newline_flag = 0;
                }
    };
  };
  /* print out all extra text */
  while (textplace != NULL) {
    handletext(textplace->when, &textplace, trackno);
  };
  freshline();
  if (barcount > maxbarcount) maxbarcount = barcount;
}




void remove_carriage_returns(char *str)
{
/* a carriage return might be embedded in a midi text meta-event.
   do not output this in the abc file or this would make a nonsyntactic
   abc file.
*/
char * loc;
while (loc  = (char *) strchr(str,'\r')) *loc = ' ';
while (loc  = (char *) strchr(str,'\n')) *loc = ' ';
}



void printQ()
/* print out tempo for abc */
{
  float Tnote, freq;
  Tnote = mf_ticks2sec((long)((xunit*unitlen)/4), division, tempo);
  freq = (float) 60.0/Tnote;
  fprintf(outhandle,"Q:1/4=%d\n", (int) (freq+0.5));
  if (summary>0) printf("Tempo: %d quarter notes per minute\n",
    (int) (freq + 0.5));
}



/* [SS] 2019-05-08 */
void reset_back_array () 
/* reset back[] after each bar line */
{
  int i;
  for (i=0;i<256;i++) back[i] = barback[i];
}

void setupkey(sharps)
int sharps;
/* set up variables related to key signature */
{
  char sharp[13], flat[13], shsymbol[13], flsymbol[13];
  int j, t, issharp;
  int minkey;

  for (j=0; j<12; j++) 
    key[j] = 0;
  minkey = (sharps+12)%12;
  if (minkey%2 != 0) {
    minkey = (minkey+6)%12;
  };
  /* [SS] 2017-12-20 changed strncpy to memcpy and limit to 12 */
  /*strncpy(sharp,    "ccddeffggaab",16);  [SS] 2017-08-30 */
  memcpy(sharp,    "ccddeffggaab",12);  /* [SS] 2017-12-20 */
  memcpy(shsymbol, "=^=^==^=^=^=",12); /* [SS] 2017-12-20 */
  if (sharps == 6) {
    sharp[6] = 'e';
    shsymbol[6] = '^';
  };
  memcpy(flat, "cddeefggaabb",12); /* [SS] 2017-12-20 */
  memcpy(flsymbol, "=_=_==_=_=_=",12); /* [SS] 2017-12-20 */
  /* Print out key */

  if (sharps >= 0) {
    if (sharps == 6) {
      fprintf(outhandle,"K:F#");
    } 
    else {
      fprintf(outhandle,"K:%c", sharp[minkey] + 'A' - 'a');
    };
    issharp = 1;
  } 
  else {
    if (sharps == -1) {
      fprintf(outhandle,"K:%c", flat[minkey] + 'A' - 'a');
    } 
    else {
      fprintf(outhandle,"K:%cb", flat[minkey] + 'A' - 'a');
    };
    issharp = 0;
  };
  if (sharps >= 0) {
    fprintf(outhandle," %% %d sharps\n", sharps);
  }
  else {
    fprintf(outhandle," %% %d flats\n", -sharps);
  };
  key[(minkey+1)%12] = 1;
  key[(minkey+3)%12] = 1;
  key[(minkey+6)%12] = 1;
  key[(minkey+8)%12] = 1;
  key[(minkey+10)%12] = 1;
  for (j=0; j<256; j++) {
    t = j%12;
    if (issharp) {
      atog[j] = sharp[t];
      symbol[j] = shsymbol[t];
    } 
    else {
      atog[j] = flat[t];
      symbol[j] = flsymbol[t];
    };
    trans[j] = 7*(j/12)+((int) atog[j] - 'a');
    if (j < MIDDLE) {
      atog[j] = (char) (int) atog[j] + 'A' - 'a';
    };
    if (key[t] == 0) {
      barback[trans[j]] = j; /* [SS] 2019-05-08 */
    };
  };
reset_back_array(); /* [SS] 2019-05-08 */ 
}




/*  Functions for supporting the command line user interface to midi2abc.     */


int readnum(num) 
/* read a number from a string */
/* used for processing command line */
char *num;
{
  int t;
  char *p;
  int neg;
  
  t = 0;
  neg = 1;
  p = num;
  if (*p == '-') {
    p = p + 1;
    neg = -1;
  };
  while (((int)*p >= '0') && ((int)*p <= '9')) {
    t = t * 10 + (int) *p - '0';
    p = p + 1;
  };
  return neg*t;
}


int readnump(p) 
/* read a number from a string (subtly different) */
/* used for processing command line */
char **p;
{
  int t;
  
  t = 0;
  while (((int)**p >= '0') && ((int)**p <= '9')) {
    t = t * 10 + (int) **p - '0';
    *p = *p + 1;
  };
  return t;
}


void readsig(a, b, sig)
/* read time signature */
/* used for processing command line */
int *a, *b;
char *sig;
{
  char *p;
  int t;

  p = sig;
  if ((int)*p == 'C') {
    *a = 4;
    *b = 4;
    return;
  };
  *a = readnump(&p);
  if ((int)*p != '/') {
    char msg[80];

    sprintf(msg, "Expecting / in time signature found %c!", *p);
    fatal_error(msg);
  };
  p = p + 1;
  *b = readnump(&p);
  if ((*a == 0) || (*b == 0)) {
    char msg[80];

    sprintf(msg, "%d/%d is not a valid time signature!", *a, *b);
    fatal_error(msg);
  };
  t = *b;
  while (t > 1) {
    if (t%2 != 0) {
      fatal_error("Bad key signature, divisor must be a power of 2!");
    }
    else {
      t = t/2;
    };
  };
}

int is_power_of_two(int numb)
/* checks whether numb is a power of 2 less than 256 */
{
int i,k;
k = 1;
for (i= 0;i<8;i++) {
  if(numb == k) return(1);
  k *= 2;
  }
return(0);
}

int getarg(option, argc, argv)
/* extract arguments from command line */
char *option;
char *argv[];
int argc;
{
  int j, place;

  place = -1;
  for (j=0; j<argc; j++) {
    if (strcmp(option, argv[j]) == 0) {
      place = j + 1;
    };
  };
  return (place);
}

int huntfilename(argc, argv)
/* look for filename argument if -f option is missing */
/* assumes filename does not begin with '-'           */
char *argv[];
int argc;
{
  int j, place;

  place = -1;
  j = 1;
  while ((place == -1) && (j < argc)) {
    if (strncmp("-", argv[j], 1) != 0) {
      place = j;
    } 
    else {
     if (strchr("ambQkcou", *(argv[j]+1)) == NULL) {
       j = j + 1;
     }
     else {
       j = j + 2;
     };
    };
  };
  return(place);
}

int process_command_line_arguments(argc,argv)
char *argv[];
int argc;
{
  int val;
  int arg;
  arg = getarg("-ver",argc,argv);
  if (arg != -1) {printf("%s\n",VERSION); exit(0);}
  midiprint = 0;
  arg = getarg("-midigram",argc,argv);
  if (arg != -1) 
   {
   midiprint = 1;
   }


  usesplits = 0;
  arg = getarg("-splitvoices",argc,argv);
  if (arg != -1) 
   usesplits = 2;

  arg = getarg("-mftext",argc,argv);
  if (arg != -1) 
   {
   midiprint = 2;
   timeunits = 2; /* [SS] 2018-10-25 */
   }

  arg = getarg("-mftextpulses",argc,argv);
  if (arg != -1) 
   {
   midiprint = 2;
   timeunits = 3; /* [SS] 2018-10-25 */
   }



  arg = getarg("-stats",argc,argv);
  if (arg != -1)
   {
   stats = 1;
   } 

  arg = getarg("-d",argc,argv);
  if ((arg != -1) && (arg <argc)) {
   debug = readnum(argv[arg]);
   }

  arg = getarg("-a", argc, argv);
  if ((arg != -1) && (arg < argc)) {
    anacrusis = readnum(argv[arg]);
  } 
  else {
    anacrusis = 0;
  };
  arg = getarg("-m", argc, argv);
  if ((arg != -1) && (arg < argc)) {
    readsig(&asig, &bsig, argv[arg]);
    tsig_set = 1;
  }
  else {
    asig = 4;
    bsig = 4;
    tsig_set = 0;
  };
  arg = getarg("-Q", argc, argv);
  if (arg != -1 && arg >= argc+1) { /* [SS] 2015-02-22 */
    Qval = readnum(argv[arg]);
  }
  else {
    Qval = 0;
  };
  arg = getarg("-u", argc,argv);
  if (arg != -1) {
    xunit = readnum(argv[arg]);
    xunit_set = 1;
  }
  else {
	xunit = 0;
	xunit_set = 0;
       };
  arg = getarg("-ppu",argc,argv);
  if (arg != -1) {
     val = readnum(argv[arg]);
     if (is_power_of_two(val)) parts_per_unitlen = val;
     else {
	   printf("*error* -ppu parameter must be a power of 2\n");
           parts_per_unitlen = 2;
          }
  }
  else
     parts_per_unitlen = 2;
  arg = getarg("-aul",argc,argv);
  if (arg != -1) {
     val = readnum(argv[arg]);
     if (is_power_of_two(val)) {
	    unitlen = val;
	    unitlen_set = 1;}
        else 
           printf("*error* -aul parameter must be a power of 2\n");
      }
  arg = getarg("-bps",argc,argv);
  if (arg != -1)
   bars_per_staff = readnum(argv[arg]);
  else {
       bars_per_staff=4;
   };
  arg = getarg("-bpl",argc,argv);
  if (arg != -1)
   bars_per_line = readnum(argv[arg]);
  else {
       bars_per_line=1;
   };


  extracta = (getarg("-xa", argc, argv) != -1);
  guessa = (getarg("-ga", argc, argv) != -1);
  guessu = (getarg("-gu", argc, argv) != -1);
  guessk = (getarg("-gk", argc, argv) != -1);
  keep_short = (getarg("-s", argc, argv) != -1);
  summary = getarg("-sum",argc,argv); 
  swallow_rests = getarg("-sr",argc,argv);
  if (swallow_rests != -1) {
	 restsize = readnum(argv[swallow_rests]);
         if(restsize <1) restsize=1;
        }
  obpl = getarg("-obpl",argc,argv);
  if (obpl>= 0) bars_per_line=1;
  if (!unitlen_set) {
    if ((asig*4)/bsig >= 3) {
      unitlen =8;
     }
    else {
      unitlen = 16;
     };
   }
  arg = getarg("-b", argc, argv);
  if ((arg != -1) && (arg < argc)) {
    bars = readnum(argv[arg]);
  }
  else {
    bars = 0;
  };
  arg = getarg("-c", argc, argv);
  if ((arg != -1) && (arg < argc)) {
    xchannel = readnum(argv[arg]) - 1;
  }
  else {
    xchannel = -1;
  };
  arg = getarg("-k", argc, argv);
  if ((arg != -1) && (arg < argc)) {
    keysig = readnum(argv[arg]);
    if (keysig<-6) keysig = 12 - ((-keysig)%12);
    if (keysig>6)  keysig = keysig%12;
    if (keysig>6)  keysig = keysig - 12;
    ksig_set = 1;
  } 
  else {
    keysig = -50;
    ksig_set = 0;
  };

  if(guessk) ksig_set=1;

  arg = getarg("-o",argc,argv);
  if ((arg != -1) && (arg < argc))  {
    outhandle = efopen(argv[arg],"w");  /* open output abc file */
  } 
  else {
    outhandle = stdout;
  };
  arg = getarg("-nt", argc, argv);
  if (arg == -1) {  /* 2017-01-01 */
    allow_triplets = 1;
  } 
  else {
    allow_triplets = 0;
  };
  arg = getarg("-nb", argc, argv);
  if (arg == -1) { /* 2017-01-01 */
    allow_broken = 1;
  } 
  else {
    allow_broken = 0;
  };
  arg = getarg("-nogr",argc,argv);
  if (arg != -1)
     nogr=1;
  else nogr = 0;

  arg = getarg("-noly",argc,argv); /* [SS] 2019-07-12 */
  if (arg != -1)
     noly=1;
  else noly = 0;

  arg = getarg("-title",argc,argv);
  if (arg != -1) {
       title = addstring(argv[arg]);
       }

  arg = getarg("-origin",argc,argv);
  if (arg != -1) {
       origin = addstring(argv[arg]);
       }
  

  arg = getarg("-f", argc, argv);
  if (arg == -1) {
    arg = huntfilename(argc, argv);
  };

  if ((arg != -1) && (arg < argc)) {
    F = efopen(argv[arg],"rb");
/*    fprintf(outhandle,"%% input file %s\n", argv[arg]); */
  }
  else {
    printf("midi2abc version %s\n  usage :\n",VERSION);
    printf("midi2abc filename <options>\n");
    printf("         -a <beats in anacrusis>\n");
    printf("         -xa  Extract anacrusis from file ");
    printf("(find first strong note)\n");
    printf("         -ga  Guess anacrusis (minimize ties across bars)\n");
    printf("         -gk  Guess key signature \n");
    printf("         -gu  Guess xunit from note duration statistics\n");
    printf("         -m <time signature>\n");
    printf("         -b <bars wanted in output>\n");
    printf("         -Q <tempo in quarter-notes per minute>\n");
    printf("         -k <key signature> -6 to 6 sharps\n");
    printf("         -c <channel>\n");
    printf("         -u <number of midi pulses in abc time unit>\n");
    printf("         -ppu <number of parts in abc time unit>\n");
    printf("         -aul <denominator of L: unit length>\n");
    printf("         [-f] <input file>\n");
    printf("         -o <output file>\n");
    printf("         -s Do not discard very short notes\n");
    printf("         -sr <number> Absorb short rests following note\n");
    printf("           where <number> specifies its size\n");
    printf("         -sum summary\n");
    printf("         -nb Do not look for broken rhythms\n");
    printf("         -nt Do not look for triplets or broken rhythm\n");
    printf("         -bpl <number> of bars printed on one line\n");
    printf("         -bps <number> of bars to be printed on staff\n");
    printf("         -obpl One bar per line\n");
    printf("         -nogr No note grouping. Space between all notes\n");
    printf("         -noly Suppress lyric output\n");
    printf("         -splitvoices  splits voices to avoid nonhomophonic chords\n");
    printf("         -title <string> Pastes title following\n");
    printf("         -origin <string> Adds O: field containing string\n");
    printf("         -midigram   Prints midigram instead of abc file\n");
    printf("         -mftext mftext output in beats\n"); 
    printf("         -mftextpulses mftext output in midi pulses\n"); 
    printf("         -mftext mftext output in seconds\n"); 
    printf("         -stats summary and statistics output\n"); 
    printf("         -ver version number\n");
    printf("         -d <number> debug parameter\n");
    printf(" None or only one of the options -aul -gu, -b, -Q -u should\n");
    printf(" be specified. If none are present, midi2abc will uses the\n");
    printf(" the PPQN information in the MIDI file header to determine\n");
    printf(" the suitable note length. This is the recommended method.\n");
    printf(" The input filename is assumed to be any string not\n");
    printf(" beginning with a - (hyphen). It may be placed anywhere.\n");

    exit(0);
  };
  return arg;
}



void midi2abc (arg, argv)
char *argv[];
int arg;
{
int voiceno;
int accidentals; /* used for printing summary */
int j;
int ten,ninety;
int argc;


/* initialization */
  trackno = 0;
  track[trackno].texthead = NULL;
  track[trackno].texttail = NULL;
  initfuncs();
  playinghead = NULL;
  playingtail = NULL;
  karaoke = 0;
  Mf_getc = filegetc;

/* parse MIDI file */
  mfread();
  if (debug>0) printf("mfread finished: %d tracks read\n",trackcount);

  fclose(F);

/* [SS] 2019-05-29 */
/* run on all 16 tracks for midi file type 0 */
if (trackcount == 1) {
  if(debug >5)  listtrack_sizes();
   trackcount = 16; 
  }

/* count MIDI tracks with notes */
  maintrack = 0;
  while ((track[maintrack].notes == 0) && (maintrack < trackcount)) {
    maintrack = maintrack + 1;
  };
  if (track[maintrack].notes == 0) {
    fatal_error("MIDI file has no notes!");
  };


/* compute dtnext for each note */
  for (j=0; j<trackcount; j++) {
    postprocess(j);
  };
  if (debug>0) printf("postprocess run on all tracks\n");

  if (tsig_set == 1){  /* for -m parameter set up time signature*/
       header_asig = asig; 
       header_unitlen = unitlen;
       header_bsig = bsig;
  };

/* print abc header block */
  argc = huntfilename(arg, argv);
  fprintf(outhandle,"X: 1\n"); 

  if (title != NULL) 
  fprintf(outhandle,"T: %s\n",title);
  else
  fprintf(outhandle,"T: from %s\n",argv[argc]); 

  if (origin != NULL)
  fprintf(outhandle,"O: %s\n",origin);

  if (header_bsig == 0) {
     fprintf(outhandle,"%%***Missing time signature meta command in MIDI file\n");
     setup_timesig(4,  4,  8);
     }
  fprintf(outhandle,"M: %d/%d\n", header_asig, header_bsig);
  fprintf(outhandle,"L: 1/%d\n", header_unitlen); 

  barsize = parts_per_unitlen*header_asig*header_unitlen/header_bsig;

/* compute xunit size for -Q -b options */
  if (Qval != 0) {
    xunit = mf_sec2ticks((60.0*4.0)/((float)(Qval*unitlen)), division, tempo);
  };
  if (bars > 0) {
    xunit = (int) ((float) track[maintrack].tracklen*2/(float) (bars*barsize));
    /* we multiply by 2 because barsize is in half unitlen. */
  };

/* estimate xunit if not set or if -gu runtime option */
  if (xunit == 0 || guessu) {
    guesslengths(maintrack);
  };

/* output Q: (tempo) to abc file */
  printQ();

/* Quantize note lengths of all tracks */
  for (j=0; j<trackcount; j++) {
    quantize(j, xunit);
  };

/* Estimate anacrusis if requested. Otherwise it is set by       */
/* user or set to 0.                                             */
  if (extracta) {
    anacrusis = findana(maintrack, barsize);
    fprintf(outhandle,"%%beats in anacrusis = %d\n", anacrusis);
  };
  if (guessa) {
    anacrusis = guessana(barsize);
    fprintf(outhandle,"%%beats in anacrusis = %d\n", anacrusis);
  };

/* If key signature is not known find the best one.              */
  if ((keysig == -50 && gotkeysig ==0) || guessk) {
       keysig = findkey(maintrack);
       if (summary>0) printf("Best key signature = %d flats/sharps\n",keysig);
       }
       header_keysig = keysig;
       setupkey(header_keysig);

/* scannotes(maintrack); For debugging */
  if(debug>0) printf("finished getting header parameters.\n");

/* Convert each track to abc format and print */
  if (trackcount > 1) {
    voiceno = 1;
    for (j=0; j<trackcount; j++) {
      freshline();
      if (track[j].notes > 0) {
        if(debug > 0) printf("outputting track %d\n",j);
        fprintf(outhandle,"V:%d\n", voiceno);
	if (track[j].drumtrack) fprintf(outhandle,"%%%%MIDI channel 10\n");
        voiceno = voiceno + 1;
      pitch_percentiles(j,&ten,&ninety);
      if (ten !=0) {
         if(ten < 56 && ninety > 64 && ninety < 68) fprintf(outhandle,"%%%%clef bass\n");
         if(ten < 56 && ninety > 67) fprintf(outhandle,"%%%%clef treble\n");
          };

      if (usesplits==2) printtrack_split_voice(j, anacrusis);
      else printtrack(j,anacrusis);
      }; /*track[j].notes > 0 */
    } /* for loop */

   } else {  /* trackcount == 1 */
      pitch_percentiles(maintrack,&ten,&ninety);
      if (ten !=0) {
         if(ten < 56 && ninety > 64 && ninety < 70) fprintf(outhandle,"%%%%clef bass\n");

         if(ten < 56 && ninety > 69) fprintf(outhandle,"%%%%clef treble\n");
	   
      }
     if (usesplits==2) printtrack_split_voice(maintrack, anacrusis);
     else printtrack(maintrack,anacrusis);
  };

  /* scannotes(maintrack); for debugging */

/* output report if requested */
  if(summary>0) {
   accidentals = keysig;
   if (accidentals <0 )
     {
     accidentals = -accidentals;
     printf("Using key signature: %d flats\n", accidentals);
     }
   else
     printf("Using key signature : %d sharps\n", accidentals);
   printf("Using an anacrusis of %d beats\n",anacrusis);
   printf("Using unit length : 1/%d\n",unitlen);
   printf("Using %d pulses per unit length (xunit).\n",xunit);
   printf("Producing %d bars of output.\n",maxbarcount);
   }


/* free up data structures */
  for (j=0; j< trackcount; j++) {
    struct listx* this;
    struct listx* x;
    struct tlistx* tthis;
    struct tlistx* tx;

    this = track[j].head;
    while (this != NULL) {
      free(this->note);
      x = this->next ;
      free(this);
      this = x;
    };
    tthis = track[j].texthead;
    while (tthis != NULL) {
      free(tthis->text);
      tx = tthis->next;
      free(tthis);
      tthis = tx;
    };
  };
  fclose(outhandle);
}

void midigram(argc,argv)
char *argv[];
int argc;
{
int i;
int verylasttick;
initfunc_for_midinotes();
init_notechan();
for (i=0;i<17;i++) {last_tick[i]=0;}
/*F = efopen(argv[argc -1],"rb");*/
Mf_getc = filegetc;
mfread();
verylasttick = 0;
for (i=0;i<17;i++) {
  if(verylasttick < last_tick[i]) verylasttick = last_tick[i];
  }
printf("%d\n",verylasttick);
}

void mftext(argc,argv)
char *argv[];
int argc;
{
int i;
initfunc_for_mftext();
init_notechan();
for (i=0;i<17;i++) {last_tick[i]=0;}
/*F = efopen(argv[argc -1],"rb");*/
Mf_getc = filegetc;
mfread();
/*printf("%d\n",last_tick);*/
}


void midistats(argc,argv)
char *argv[];
int argc;
{
initfunc_for_stats();
Mf_getc = filegetc;
mfread();
stats_finish();
}



int main(argc,argv)
char *argv[];
int argc;
{
  FILE *efopen();
  int arg;

  arg = process_command_line_arguments(argc,argv);
  if(midiprint ==1) { midigram(argc,argv);
  } else if(midiprint ==2)  { mftext(argc,argv);
  } else if(stats == 1) { midistats(argc,argv);
  } else midi2abc(argc,argv); 
  return 0;
}

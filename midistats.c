/* midistats - program to extract statistics from MIDI files
 * Derived from midi2abc.c
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
 
#define VERSION "0.58 December 08 2022 midistats"

#include <limits.h>
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
void initfuncs();
void stats_finish();
float histogram_entropy (int *histogram, int size); 
void stats_noteoff(int chan,int pitch,int vol);
void stats_eot ();


/* Global variables and structures */

extern long Mf_toberead;

static FILE *F;
static FILE *outhandle; /* for producing the abc file */

int tracknum=0;  /* track number */
int division;    /* pulses per quarter note defined in MIDI header    */
int quietLimit;  /* minimum number of pulses with no activity */
long tempo = 500000; /* the default tempo is 120 quarter notes/minute */
long laston = 0; /* length of MIDI track in pulses or ticks           */
int key[12];
int sharps;
int trackno;
int maintrack;
int format; /* MIDI file type                                   */
int debug;
int chordthreshold; /* number of maximum number of pulses separating note */
int beatsPerBar = 4; /* 4/4 time */
int divisionsPerBar;
int unitDivision;



int tempocount=0;  /* number of tempo indications in MIDI file */

int stats = 0; /* flag - gather and print statistics            */



/* can cope with up to 64 track MIDI files */
int trackcount = 0;

int notechan[2048],notechanvol[2048]; /*for linking on and off midi
					channel commands            */
int last_tick[17]; /* for getting last pulse number in MIDI file */
int last_on_tick[17]; /* for detecting chords [SS] 2019-08-02 */




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
  int lastNoteOff[17];
  int quietTime[17];
  int rhythmpatterns[17];
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


struct barPattern {
	int activeBarNumber;
	int rhythmPattern;
} barChn[17];


#define HashSize 257 
struct hashStruct {
	int pattern[HashSize];
	int count[HashSize];
} hasher[17] = {0};

int ncollisions = 0;
int nrpatterns = 0;

void handle_collision () {
ncollisions++;
}

void put_pattern (int chan, int pattern) {
int hashindex;
hashindex = pattern % HashSize;
if (hasher[chan].pattern[hashindex] == 0) {
	hasher[chan].pattern[hashindex] = pattern;
	nrpatterns++;
	/*printf ("hasher[%d].pattern[%d] = %d\n",chan,hashindex,pattern);*/
  } else if (hasher[chan].pattern[hashindex] != pattern) {
	 /* printf("collision\n"); */
	  handle_collision ();
  } else {
  hasher[chan].count[hashindex]++;
  }
}


int count_patterns_for (int chan) {
int i;
int sum;
sum = 0;
for (i = 0; i<HashSize; i++) {
  if (hasher[chan].pattern[i] > 0) sum++;  
  /* hasher[chan].pattern[i] = 0;  reset hasher */
  }
trkdata.rhythmpatterns[chan] = sum;
return sum;
}


void output_hasher_results () {
int i;
/* printf("rhythmPatterns ");*/
for (i = 0; i<17; i++) {
	trkdata.rhythmpatterns[i] = count_patterns_for(i);
/*	printf("%d ",count_patterns_for (i)); */
       }
printf("\n");
}


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




/* Dummy functions for handling MIDI messages.
 *    */
 void no_op0() {}
 void no_op1(int dummy1) {}
 void no_op2(int dummy1, int dummy2) {}
 void no_op3(int dummy1, int dummy2, int dummy3) { }
 void no_op4(int dummy1, int dummy2, int dummy3, int dummy4) { }
 void no_op5(int dummy1, int dummy2, int dummy3, int dummy4, int dummy5) { }



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




void stats_header (int format, int ntrks, int ldivision)
{
  int i;
  division = ldivision;
  quietLimit = ldivision*8;
  divisionsPerBar = division*beatsPerBar;
  unitDivision = divisionsPerBar/24;
  printf("ntrks %d\n",ntrks);
  printf("ppqn %d\n",ldivision);
  chordthreshold = ldivision/16; /* [SS] 2018-01-21 */
  trkdata.tempo[0] = 0;
  trkdata.pressure[0] = 0;
  trkdata.program[0] = 0;
  for (i=0;i<17;i++) {
    trkdata.npulses[i] = 0;
    trkdata.pitchbend[i] = 0;
    trkdata.cntlparam[i] = 0; /* [SS] 2022-03-04 */
    trkdata.pressure[i] = 0; /* [SS] 2022-03-04 */
    trkdata.quietTime[i] = 0; /* [SS] 2022-08-22 */
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
double delta;


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
printf("\nquietTime ");
for (i=1;i<17;i++) {
  delta = trkdata.npulses[0] - trkdata.quietTime[i];
  if (trkdata.quietTime[i] < quietLimit) delta = 0;
  delta = delta / (double) trkdata.npulses[0];
  /* printf (" %5.3f ", delta); */
  printf (" %d ", trkdata.quietTime[i]);
  }

printf("\npitchentropy %f\n",histogram_entropy(pitchclass_activity,12));
printf("totalrhythmpatterns =%d\n",nrpatterns);
printf("collisions = %d\n",ncollisions);
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
output_hasher_results();
for (i=1;i<17;i++) {
   if(trkdata.notecount[i] == 0 && trkdata.chordcount[i] == 0) continue; 
   printf("trkinfo ");
   printf("%d %d ",i,trkdata.program[i]); /* channel number and program*/
   printf("%d %d ",trkdata.notecount[i],trkdata.chordcount[i]);
   printf("%d %d ",trkdata.notemeanpitch[i], trkdata.notelength[i]);
   printf("%d %d ",trkdata.cntlparam[i],trkdata.pressure[i]); /* [SS] 2022-03-04 */
   printf("%d %d",trkdata.quietTime[i],trkdata.rhythmpatterns[i]);
   trkdata.quietTime[i] = 0;
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



void stats_noteon(chan,pitch,vol)
int chan, pitch, vol;
{
 int delta;
 int barnum;
 int unit;

 if (vol == 0) {
    /* treat as noteoff */
    stats_noteoff(chan,pitch,vol);
    trkdata.lastNoteOff[chan+1] = Mf_currtime; /* [SS] 2022.08.22 */
    return;
    }
 trkdata.notemeanpitch[chan+1] += pitch;
 if (trkdata.lastNoteOff[chan+1] >= 0) {
	 delta = Mf_currtime - trkdata.lastNoteOff[chan+1];
	 trkdata.lastNoteOff[chan+1] = -1; /* in case of chord */
	 if (delta > quietLimit) {
		 trkdata.quietTime[chan+1] += delta;
	 }
    }
	 
 if (abs(Mf_currtime - last_on_tick[chan+1]) < chordthreshold) trkdata.chordcount[chan+1]++;
 else trkdata.notecount[chan+1]++; /* [SS] 2019-08-02 */
 last_tick[chan+1] = Mf_currtime;
 last_on_tick[chan+1] = Mf_currtime; /* [SS] 2019-08-02 */
 /* last_on_tick not updated by stats_noteoff */


 if (chan != 9) {
   barnum = Mf_currtime/divisionsPerBar;
   if (barnum != barChn[chan].activeBarNumber) {
	 /*printf("%d %d %d\n",chan,barChn[chan].activeBarNumber,
			 barChn[chan].rhythmPattern);      */
         put_pattern (chan+1, barChn[chan].rhythmPattern);

	 barChn[chan].rhythmPattern = 0;
         barChn[chan].activeBarNumber = barnum;
    }
  unit = (Mf_currtime % divisionsPerBar)/unitDivision;
  //printf("unit = %d pattern = %d \n",unit,barChn[chan].rhythmPattern);
  barChn[chan].rhythmPattern = barChn[chan].rhythmPattern |= (1UL << unit);
  }



 if (chan == 9) {
   if (pitch < 0 || pitch > 81) 
         printf("****illegal drum value %d\n",pitch);
   else  drumhistogram[pitch]++;
   }
 else pitchhistogram[pitch % 12]++;  /* [SS] 2017-11-01 */
}


void stats_eot () {
trkdata.lastNoteOff[0] = Mf_currtime; /* [SS] 2022.08.24 */
}

void stats_noteoff(int chan,int pitch,int vol)
{
  int length;
  int program;
  /* ignore if there was no noteon */
  if (last_tick[chan+1] == -1) return;
  length = Mf_currtime - last_tick[chan+1];
  trkdata.notelength[chan+1] += length;
  trkdata.lastNoteOff[chan+1] = Mf_currtime; /* [SS] 2022.08.22 */
  chnactivity[chan+1] += length;
  if (chan == 9) return; /* drum channel */
  pitchclass_activity[pitch % 12] += length;
  program = trkdata.program[chan+1];
  progactivity[program] += length;
  /* [SS] 2018-04-18 */
  if(Mf_currtime > last_tick[chan+1]) last_tick[chan+1] = Mf_currtime;
}


void stats_pitchbend(chan,lsb,msb)
int chan, lsb, msb;
{
trkdata.pitchbend[0]++;
trkdata.pitchbend[chan+1]++;
}

void stats_pressure(chan,press)
int chan, press;
{
trkdata.pressure[0]++;
trkdata.pressure[chan+1]++; /* [SS] 2022.04.28 */
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



void stats_parameter(chan,control,value)
int chan, control, value;
{
/*if (control == 7) {
  printf("cntrlvolume %d %d \n",chan+1,value);
  }
*/
trkdata.cntlparam[chan+1]++;
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
    Mf_eot = stats_eot;
    Mf_timesig = stats_timesig;
    Mf_smpte = no_op5;
    Mf_tempo = stats_tempo;
    Mf_keysig = stats_keysig;
    Mf_seqspecific = no_op3;
    Mf_text = stats_metatext;
    Mf_arbitrary = no_op2;
}


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
  /* [JA] 2021-05-25 */
  while (((int)*p >= '0') && ((int)*p <= '9') && (t < (INT_MAX-9)/10)) {
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
  /* [JA] 2021-05-25 */
  while (((int)**p >= '0') && ((int)**p <= '9') && (t < (INT_MAX-9)/10)) {
    t = t * 10 + (int) **p - '0';
    *p = *p + 1;
  };
  /* advance over any spurious extra digits */
  while (isdigit(**p)) {
    *p = *p + 1;
  }
  return t;
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

  stats = 1;

  arg = getarg("-d",argc,argv);
  if ((arg != -1) && (arg <argc)) {
   debug = readnum(argv[arg]);
   }




  arg = getarg("-o",argc,argv);
  if ((arg != -1) && (arg < argc))  {
    outhandle = efopen(argv[arg],"w");  /* open output abc file */
  } 
  else {
    outhandle = stdout;
  };

  arg = getarg("-f", argc, argv);
  if (arg == -1) {
    arg = huntfilename(argc, argv);
  };

  if ((arg != -1) && (arg < argc)) {
    F = efopen(argv[arg],"rb");
/*    fprintf(outhandle,"%% input file %s\n", argv[arg]); */
  }
  else {
    printf("midistats version %s\n  usage :\n",VERSION);
    printf("midistats filename <options>\n");
    printf("         -ver version number\n");
    printf("         -d <number> debug parameter\n");
    printf(" The input filename is assumed to be any string not\n");
    printf(" beginning with a - (hyphen). It may be placed anywhere.\n");
    exit(0);
  };
  return arg;
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
  if(stats == 1)  midistats(argc,argv);
  return 0;
}

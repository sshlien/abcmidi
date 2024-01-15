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
 
#define VERSION "0.85 January 04 2024 midistats"

/* midistrats.c is a descendent of midi2abc.c which was becoming to
   large. The object of the program is to extract statistical characterisitic 
   of a midi file. It is mainly called by the midiexplorer.tcl application,
   but it now used to create some databases using runstats.tcl which
   comes with the midiexplorer package.

   By default the program produces a summary that is described in the
   midistats.1 man file. This is done by making a single pass through
   the midi file. If the program is called with one of the runtime
   options, the program extracts particular information by making more
   than one pass. In the first pass it creates a table of all the
   midievents which is stored in memory. The midievents are sorted in
   time, and the requested information is extracted by going through
   this table.
*/

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
float histogram_perplexity (int *histogram, int size); 
void stats_noteoff(int chan,int pitch,int vol);
void stats_eot ();
void keymatch();
#define max(a,b)  (( a > b ? a : b))
#define min(a,b) (( a < b ? a : b))

/* Global variables and structures */

extern long Mf_toberead;

static FILE *F;
static FILE *outhandle; /* for producing the abc file */

int tracknum=0;  /* track number */
int lasttrack = 0; /* lasttrack */
int division;    /* pulses per quarter note defined in MIDI header    */
int quietLimit;  /* minimum number of pulses with no activity */
long tempo = 500000; /* the default tempo is 120 quarter notes/minute */
int bpm = 120; /*default tempo */
long laston = 0; /* length of MIDI track in pulses or ticks           */
int key[12];
int sharps;
int trackno;
int maintrack;
int format; /* MIDI file type                                   */
int debug;
int pulseanalysis;
int percanalysis;
int percpattern;
int percpatternfor;
int percpatternhist;
int pitchclassanalysis;
int nseqfor;
int corestats;
int chordthreshold; /* number of maximum number of pulses separating note */
int beatsPerBar = 4; /* 4/4 time */
int divisionsPerBar;
int unitDivision;
int maximumPulse;
int lastBeat;
int hasLyrics = 0;


struct eventstruc {int onsetTime;
                   unsigned char channel;
		   unsigned char pitch;
		   unsigned char velocity;
                   ;} midievents[50000];

int lastEvent = 0;


int tempocount=0;  /* number of tempo indications in MIDI file */

int stats = 0; /* flag - gather and print statistics            */

int pulseanalysis = 0;
int percanalysis = 0;
int percpattern = 0;
int percpatternfor = 0;
int percpatternhist = 0;
int pitchclassanalysis = 0;
int nseqfor = 0;
int corestats = 0;


/* can cope with up to 64 track MIDI files */
int trackcount = 0;

int notechan[2048],notechanvol[2048]; /*for linking on and off midi
					channel commands            */
int lastTick[2048]; /* for getting last pulse number for chan (0-15) and pitch (0-127) in MIDI file */
int last_on_tick[17]; /* for detecting chords [SS] 2019-08-02 */
int channel_active[17]; /* for dealing with chords [SS] 2023-08-30 */
int channel_used_in_track[17]; /* for dealing with quietTime [SS] 2023-09-06 */

int histogram[256];
unsigned char drumpat[8000];
unsigned char pseq[8000];
int percnum;
int nseqchn;



/* The following variables are used by the -stats option
 * which is used by a separate application called midiexplorer.tcl.
 * The channel numbers go from 1 to 16 instead of 0 to 15
 */
struct trkstat {
  int notecount[17];
  int chordcount[17];
  int notemeanpitch[17];
  int notepitchmin[17];
  int notepitchmax[17];
  int notelength[17];
  int notelengthmin[17];
  int notelengthmax[17];
  int pitchbend[17];
  int pressure[17];
  int cntlparam[17];
  int program[17];
  int tempo[17];
  int npulses[17];
  int lastNoteOff[17];
  int quietTime[17];
  int rhythmpatterns[17];
  int numberOfGaps[17];
  int chanvol[17];
  float pitchEntropy[17];
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
int drumhistogram[100]; /* counts drum noteons */
int pitchhistogram[12]; /* pitch distribution for non drum notes */
int channel2prog[17]; /* maps channel to program */
int channel2nnotes[17]; /*maps channel to note count */
int chnactivity[17]; /* [SS] 2018-02-02 */
int trkactivity[40]; /* [SS] 2023-10-25 */
int progactivity[128]; /* [SS] 2018-02-02 */
int pitchclass_activity[12]; /* [SS] 2018-02-02 */
int chanpitchhistogram[204]; /* [SS] 2023-09-13 */


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

int pulseCounter[1024];
int pulseDistribution[24];

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
  lasttrack = ntrks; /* [SS] 2023-10-25 */
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
    trkdata.numberOfGaps[i] = 0; /* [SS] 2023-09-07 */
    trkdata.chanvol[i] = 0; /* [SS] 2023-10-30 */
    progcolor[i] = 0;
    channel2prog[i] = 0; /* [SS] 2023-06-25-8/
    channel2nnotes[i] = 0;
    chnactivity[i] = 0; /* [SS] 2018-02-02 */
    }
  for (i=0;i<100;i++) drumhistogram[i] = 0;
  for (i=0;i<12;i++) pitchhistogram[i] = 0; /* [SS] 2017-11-01 */
  for (i=0;i<12;i++) pitchclass_activity[i] = 0; /* [SS] 2018-02-02 */
  for (i=0;i<128;i++) progactivity[i] = 0; /* [SS] 2018-02-02 */
  for (i=0;i<40;i++) trkactivity[i]=0; /* [SS] 2023-10-25 */
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


/* [SS] 2023-10-30 */
void stats_interpret_pulseCounter () {
int i,j;
int maxcount,ncounts,npeaks,npositives,peaklimit;
int maxloc;
float threshold,peak;
int decimate;
float tripletsCriterion8,tripletsCriterion4;
int resolution = 12;
int nzeros;
threshold = 10.0/(float) division;
maxcount = 0;
ncounts = 0;
npeaks = 0;
for (i=0;i<division;i++) {
  ncounts = ncounts + pulseCounter[i];
  if (pulseCounter[i] > maxcount) {
       maxloc = i;
       maxcount = pulseCounter[i];
       } 
  } 
peaklimit = (int) (ncounts * 0.020);
for (i=0;i<division;i++) {
  if (pulseCounter[i] > peaklimit) npeaks++;
  }
for (i = 0; i < resolution; i++) pulseDistribution[i] = 0;
decimate = division/resolution;
for (i = 0; i < division; i++) {
   j = i/decimate;
   pulseDistribution[j] += pulseCounter[i];
  }

/* count zeros */
nzeros = 0;
for (i=0;i<resolution;i++) if((float) pulseDistribution[i]/(float) ncounts < 0.015 ) nzeros++;
npositives = resolution - nzeros;
if (nzeros > 3 && (float) pulseDistribution[resolution-1]/(float) ncounts < 0.1) {printf("clean_quantization\n");
 } else if ((float) pulseDistribution[resolution-1]/(float) ncounts > 0.09 ||
            npeaks > npositives) {printf("dithered_quantization\n");
 } else {
peak = (float) maxcount/ (float) ncounts;
if (peak < threshold)  printf("unquantized\n");
}

tripletsCriterion8 = (float) pulseDistribution[8]/ (float) ncounts;
tripletsCriterion4 = (float) pulseDistribution[4]/ (float) ncounts;
if (tripletsCriterion8 > 0.10 || tripletsCriterion4 > 0.10) printf("triplets\n");
if (pulseDistribution[0]/(float) ncounts > 0.95) printf("qnotes");
/*
printf("pulseDistribution:");
for (i=0;i<resolution;i++) printf("%6.3f",(float) pulseDistribution[i]/(float) ncounts);
printf("\n");
printf("nzeros = %d npeaks = %d \n",nzeros,npeaks);
*/
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
       progactivity[channel2prog[i]] += chnactivity[i]; /* [SS] 2023-06-25 */
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
for (i=35;i<100;i++) {
  if (drumhistogram[i] > 0) printf("%d ",i);
  }
printf("\ndrumhits ");
for (i=35;i<100;i++) {
  if (drumhistogram[i] > 0) printf("%d ",drumhistogram[i]);
  }

printf("\npitches "); /* [SS] 2017-11-01 */
for (i=0;i<12;i++) printf("%d ",pitchhistogram[i]);

keymatch();

printf("\npitchact "); /* [SS] 2018-02-02 */
if (npulses > 0)
  for (i=0;i<12;i++) printf("%5.2f ",pitchclass_activity[i]/(double) npulses);
else 
  for (i=0;i<12;i++) printf("%5.2f ",(double) pitchclass_activity[i]);
printf("\nchanvol "); /* [SS] 2023-10-30 */
for (i=1;i<17;i++) printf("%4d ",trkdata.chanvol[i]);
printf("\nchnact "); /* [SS] 2018-02-08 */
if (npulses > 0)
  for (i=1;i<17;i++) printf("%5.3f ",chnactivity[i]/(double) trkdata.npulses[0]);
else 
  for (i=0;i<17;i++) printf("%5.3f ",(double) chnactivity[i]);
printf("\ntrkact ");
  lasttrack++;
  for (i=0;i<lasttrack;i++) printf("% 5d",trkactivity[i]);
printf("\npitchperplexity %f\n",histogram_perplexity(pitchclass_activity,12));
printf("totalrhythmpatterns =%d\n",nrpatterns);
printf("collisions = %d\n",ncollisions);
if (hasLyrics) printf("Lyrics\n");
stats_interpret_pulseCounter ();
printf("\n");
}



float histogram_perplexity (int *histogram, int size) 
  {
  /* The perplexity is 2 to the power of the entropy */
  int i;
  int total;
  float entropy;
  float e,p;
  total = 0;
  entropy = 0.0;
  //printf("\nhistogram_entropy of:");
  for (i=0;i<size;i++) {
    total += histogram[i];
    //printf(" %d",histogram[i]);
    } 
  for (i=0;i<size;i++) {
    if (histogram[i] < 1) continue;
    p = (float) histogram[i]/total;
    e = p*log(p);
    entropy = entropy + e;
    } 
  //printf("\n");
  return pow(2.0,-entropy/log(2.0));
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
   /* [SS] 2023-08-22 */
   if (i != 10) printf("%d %d ",trkdata.notemeanpitch[i], trkdata.notelength[i]);
   else 
     printf("-1  0 ");
   printf("%d %d ",trkdata.cntlparam[i],trkdata.pressure[i]); /* [SS] 2022-03-04 */
   printf("%d %d ",trkdata.quietTime[i],trkdata.rhythmpatterns[i]);
   if (i != 10)  {printf("%d %d %d %d %d",trkdata.notepitchmin[i],  trkdata.notepitchmax[i] ,trkdata.notelengthmin[i],  trkdata.notelengthmax[i], trkdata.numberOfGaps[i]);
   printf(" %f",trkdata.pitchEntropy[i]);
    } else
     printf("-1 0");
   trkdata.quietTime[i] = 0; /* in case channel i is used in another track */
   trkdata.numberOfGaps[i] = 0;
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
     trkdata.notepitchmin[i] = 128;
     trkdata.notepitchmax[i] = 0;
     trkdata.notelength[i] = 0;
     trkdata.notelengthmin[i] = 10000;
     trkdata.notelengthmax[i] = 0;
     trkdata.chordcount[i] = 0;
     trkdata.cntlparam[i] = 0;
     last_on_tick[i] = -1;
     channel_active[i] = 0;
     }
  printf("trk %d \n",tracknum);

 for (i=0;i<2048;i++) lastTick[i] = -1;
 for (i=0;i<17;i++) channel_used_in_track[i] = 0; /* [SS] 2023-09-06 */
 for (i=0;i<204;i++) chanpitchhistogram[i] = 0;  /* [SS] 2023-09-13 */
}

void stats_trackend()
{
 int chan;
 int i;
 float entropy;
 if (trkdata.npulses[0] < Mf_currtime) trkdata.npulses[0] = Mf_currtime;
 for (chan = 1; chan < 17; chan++) /* [SS] 2023-09-06 */
   if (channel_used_in_track[chan] > 0) trkdata.quietTime[chan] += (trkdata.npulses[0] - trkdata.lastNoteOff[chan]); 
 for (chan=0;chan<16;chan++) {  /* 2023-09-13 */
   if (chan == 9 || channel_used_in_track[chan+1] == 0) continue;
   trkdata.pitchEntropy[chan+1] = histogram_perplexity(chanpitchhistogram +chan*12,11);
   }
 output_track_summary(); 
}



void stats_noteon(chan,pitch,vol)
int chan, pitch, vol;
{
 int delta;
 int barnum;
 int unit;
 int dithermargin; /* [SS] 2023-08-22 */
 int cpitch; /* [SS] 2023-09-13 */
 int pulsePosition;

 cpitch = pitch % 12;
 channel_used_in_track[chan+1]++; /* [SS] 2023-09-06 */
 dithermargin = unitDivision/2 - 1; 
 if (vol == 0) {
    /* treat as noteoff */
    stats_noteoff(chan,pitch,vol);
    trkdata.lastNoteOff[chan+1] = Mf_currtime; /* [SS] 2022.08.22 */
    return;
    }
 pulsePosition = Mf_currtime % division;
 pulseCounter[pulsePosition]++;
 if (pulsePosition >= 1023) {printf("pulsePosition = %d too large\n",pulsePosition);
     exit(1);
     }
 trkdata.notemeanpitch[chan+1] += pitch;
 trkdata.notepitchmax[chan+1] = max(trkdata.notepitchmax[chan+1],pitch);
 trkdata.notepitchmin[chan+1] = min(trkdata.notepitchmin[chan+1],pitch);
 if (trkdata.lastNoteOff[chan+1] >= 0) {
	 delta = Mf_currtime - trkdata.lastNoteOff[chan+1];
	 if (delta > quietLimit) {
		 trkdata.quietTime[chan+1] += delta;
                 trkdata.numberOfGaps[chan+1]++;
	         trkdata.lastNoteOff[chan+1] = -1; /* in case of chord */
	 }
    }
	 
 if (abs(Mf_currtime - last_on_tick[chan+1]) < chordthreshold) trkdata.chordcount[chan+1]++;
 else trkdata.notecount[chan+1]++; /* [SS] 2019-08-02 */
 lastTick[chan*128 + pitch] = Mf_currtime;
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
  unit = ((Mf_currtime+dithermargin) % divisionsPerBar)/unitDivision;
  //printf("unit = %d pattern = %d \n",unit,barChn[chan].rhythmPattern);
  barChn[chan].rhythmPattern = barChn[chan].rhythmPattern |= (1UL << unit);
  chanpitchhistogram[chan*12+cpitch]++;  /* [SS] 2023-09-13 */
  }



 if (chan == 9) {
   if (pitch < 0 || pitch > 100) 
         printf("****illegal drum value %d\n",pitch);
   else  drumhistogram[pitch]++;
   }
 else pitchhistogram[pitch % 12]++;  /* [SS] 2017-11-01 */

 channel_active[chan+1]++;
}


void stats_eot () {
trkdata.lastNoteOff[0] = Mf_currtime; /* [SS] 2022.08.24 */
}

void stats_noteoff(int chan,int pitch,int vol)
{
  int length;
  int program;
  /* ignore if there was no noteon */
  if (lastTick[chan*128+pitch] == -1) return;
  length = Mf_currtime - lastTick[chan*128+pitch];
  trkdata.notelength[chan+1] += length;
  trkdata.notelengthmax[chan+1] = max(trkdata.notelengthmax[chan+1],length);
  trkdata.notelengthmin[chan+1] = min(trkdata.notelengthmin[chan+1],length);
  //if (length < 3) printf("chan = %d  lasttick = %d currtime = %ld\n",chan,lastTick[chan*128+pitch],Mf_currtime);
  trkdata.lastNoteOff[chan+1] = Mf_currtime; /* [SS] 2022.08.22 */
  chnactivity[chan+1] += length;
  trkactivity[tracknum]++;
  if (chan == 9) return; /* drum channel */
  pitchclass_activity[pitch % 12] += length;
  program = trkdata.program[chan+1];
  progactivity[program] += length;
  channel_active[chan+1]--;
  /* [SS] 2018-04-18 */
  if(Mf_currtime > lastTick[chan*128+pitch] && channel_active[chan+1] == 0) 
    lastTick[chan*128+pitch] = Mf_currtime; /* [SS] 2023.08.30 handle chords */
    
  if (length > 4800) {
     lastTick[chan*128+pitch] = Mf_currtime; /* handle stuck note [SS] 2023.08.30 */
     channel_active[chan+1] = 0;
     }
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
  if (channel2prog[chan+1]== 0) channel2prog[chan+1] = program; /* [SS] 2023-06-25*/
}



void stats_parameter(chan,control,value)
int chan, control, value;
{
int chan1;
chan1 = chan+1;
/* There may be many volume commands for the same channel. Only
   record the first one.
*/
if (control == 7 && trkdata.chanvol[chan1] == 0) {
  /*printf("cntrlvolume %d %d \n",chan+1,value);*/
  trkdata.chanvol[chan1] = value; /* [SS] 2023-10-30 */
  }
trkdata.cntlparam[chan1]++;
}



void stats_metatext(type,leng,mess)
int type, leng;
char *mess;
{
int i; 
if (type == 5) hasLyrics = 1; /* [SS] 2023-10-30 */
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

void record_noteon(int chan,int pitch,int vol)
{
if (maximumPulse < Mf_currtime) maximumPulse = Mf_currtime;
if (vol < 1) return; /* noteoff ? */
midievents[lastEvent].onsetTime = Mf_currtime;
midievents[lastEvent].channel = chan;
midievents[lastEvent].pitch = pitch;
midievents[lastEvent].velocity = vol;
lastEvent++;
if (lastEvent > 49999) {printf("ran out of space in midievents structure\n");
	exit(1);
        }
channel_active[chan+1]++;
}

void record_noteoff(int chan,int pitch,int vol)
{
}
void record_trackend()
{
}
void record_tempo(long ltempo)
{ 
tempo = ltempo;
if (bpm == 120) bpm = 60000000.0/tempo;
tempocount++;
}

int int_compare_events(const void *a, const void *b) {
   struct eventstruc *ia  = (struct eventstruc *)a;
   struct eventstruc *ib  = (struct eventstruc *)b;
   if (ib->onsetTime > ia->onsetTime)
       return -1;
   else if (ia ->onsetTime > ib->onsetTime)
       return  1;
   else return 0;
   }

void load_header (int format, int ntrks, int ldivision)
{
  int i;
  division = ldivision;
  lasttrack = ntrks;
  for (i=0;i<17;i++) channel_active[i] = 0; /* for counting number of channels*/
}


void initfunc_for_stats()
{
    int i;
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
    for (i = 0; i< 1023; i++) pulseCounter[i] = 0;
}


void initfunc_for_loadNoteEvents()
{
    Mf_error = error;
    Mf_header = load_header;
    Mf_trackstart = no_op0;
    Mf_trackend = record_trackend;
    Mf_noteon = record_noteon;
    Mf_noteoff = record_noteoff;
    Mf_pressure = no_op3;
    Mf_parameter = no_op3;
    Mf_pitchbend = no_op3;
    Mf_program = no_op0;
    Mf_chanpressure = no_op3;
    Mf_sysex = no_op2;
    Mf_metamisc = no_op3;
    Mf_seqnum = no_op1;
    Mf_eot = no_op0;
    Mf_timesig = no_op4;
    Mf_smpte = no_op5;
    Mf_tempo = record_tempo;
    Mf_keysig = no_op2;
    Mf_seqspecific = no_op3;
    Mf_text = no_op3;
    Mf_arbitrary = no_op2;
}

void dumpMidievents (int from , int to)
{
int i;
for (i = from; i < to; i++) {
  printf("%5d %d %d %d\n",midievents[i].onsetTime, midievents[i].channel,
		  midievents[i].pitch, midievents[i].velocity);
  }
}

void load_finish()
{
qsort(midievents,lastEvent,sizeof(struct eventstruc),int_compare_events);
/*dumpMidievents(0,50);*/
}


void pulseHistogram() {
int i,j;
int pulsePosition;
int decimate;
float fraction;
int resolution = 12;
for (i = 0; i< 1023; i++) pulseCounter[i] = 0;
for (i = 0; i < lastEvent; i++) {
  pulsePosition = midievents[i].onsetTime % division;
  pulseCounter[pulsePosition]++;
  if (pulsePosition >= 1023) {printf("pulsePosition = %d too large\n",pulsePosition);
	     exit(1);
             }
  }
for (i = 0; i < resolution; i++) pulseDistribution[i] = 0;
  /*for (i = 0; i < 1023; i++) printf(" %d",pulseCounter[i]);
  printf("\n");
  */
decimate = division/resolution;
for (i = 0; i < division; i++) {
   j = i/decimate;
   pulseDistribution[j] += pulseCounter[i];
   }
for (i = 0; i < resolution; i++)
   {fraction = (float) pulseDistribution[i] / (float) lastEvent;
    if (i == 0)
      printf("%6.4f",fraction);
    else
      printf(",%6.4f",fraction);
   }
printf("\n");
}

void drumanalysis () {
int i;
int channel;
int pitch;
for (i=0;i<100;i++) drumhistogram[i] = 0;
for (i = 0; i < lastEvent; i++) {
  channel = midievents[i].channel;
  if (channel != 9) continue;
  pitch = midievents[i].pitch;
  if (pitch < 0 || pitch > 100) {
    printf("illegal percussion instrument %d\n",pitch);
    exit(1);
    }
  drumhistogram[pitch]++;
  }
}

void drumpattern (int perc) {
int i;
int channel;
int pitch;
int onset;
int index;
int quarter;
int remainder;
int part;
quarter = division/4;
for (i = 0; i<8000; i++) drumpat[i] = 0;
for (i = 0; i <lastEvent; i++) {
  channel = midievents[i].channel;
  if (channel != 9) continue;
  pitch = midievents[i].pitch;
  if (pitch != perc) continue;
  onset = midievents[i].onsetTime;
  index = onset/division;
  remainder = onset % division;
  part = remainder/quarter;
  if (index >= 8000) {printf("index too large in drumpattern\n");
	              break;
                      }
  part = 3 - part; /* order the bits going left to right */
  drumpat[index] = drumpat[index] |= 1 << part;
  }
}

static int pitch2noteseq[] = {
 0,  0,  1,  1,  2,  3,  3,  4,
 4,  5,  5,  6};

void  noteseqmap(int chn) {
int i;
int half;
int channel;
int pitchclass;
int onset;
int index;
int remainder;
int noteNum;
int part;
printf("noteseqmap %d\n",chn);
half = division/2;
for (i = 0; i<8000; i++) pseq[i] = 0;
for (i = 0; i <lastEvent; i++) {
  channel = midievents[i].channel;
  if (channel == 9) continue; /* ignore percussion channel */
  if (channel == chn || chn == -1) {
    pitchclass = midievents[i].pitch % 12;
    noteNum = pitch2noteseq[pitchclass];
    onset = midievents[i].onsetTime;
    index = onset/half;
    if (index >= 8000) {printf("index too large in drumpattern\n");
	              break;
                      }
    pseq[index] = pseq[index] |= 1 << noteNum;
    }
  /*printf("pitchclass = %d noteNum =%d index = %d pseq[index] %d \n",pitchclass, noteNum, index, pseq[index]); */
  }
printf("lastBeat = %d\n",lastBeat);
for (i=0;i<(lastBeat+1)*2;i++) {
   printf("%d ",pseq[i]);
   if (i >= 8000) break;
   }
printf("\n");
}

void dualDrumPattern (int perc1, int perc2) {
int i;
int channel;
int pitch;
int onset;
int index;
int quarter;
int remainder;
int part;
quarter = division/4;
for (i = 0; i<8000; i++) drumpat[i] = 0;
for (i = 0; i <lastEvent; i++) {
  channel = midievents[i].channel;
  if (channel != 9) continue;
  pitch = midievents[i].pitch;
  if (pitch != perc1  && pitch != perc2) continue;
  onset = midievents[i].onsetTime;
  index = onset/division;
  if (index >= 8000) {printf("index too large in drumpattern\n");
	              break;
                      }
  remainder = onset % division;
  part = remainder/quarter;
  part = 3 - part; /* order the bits from left to right */
  if (pitch == perc1) drumpat[index] = drumpat[index] |= 1 << part;
  else drumpat[index] = drumpat[index] |= 16 * (1 << part);
  }
}

void pitchClassAnalysis () {
int i;
int channel;
int pitch;
for (i=0;i<12;i++) pitchclass_activity[i] = 0;
for (i = 0; i < lastEvent; i++) {
  channel = midievents[i].channel;
  if (channel == 9) continue;
  pitch = midievents[i].pitch;
  pitchclass_activity[pitch % 12]++;
  }
}

void output_perc_pattern (int i) {
int left,right;
left = i/16;
right = i % 16;
printf(" (%d.%d)",left,right);
}


void drumPatternHistogram () {
int i;
for (i=0;i<256;i++) {
 histogram[i] = 0;
}
for (i=0;i<lastBeat;i++) {
  histogram[drumpat[i]]++;
  }
for (i=1;i<256;i++) {
  if (histogram[i] > 0) {
	  printf(" %d",i);
	  output_perc_pattern(i);
	  printf(" %d ", histogram[i]);
         }
    }
  printf("\n");
}


void output_drumpat () {
int i;
for (i=0;i<lastBeat;i++) printf("%d ",drumpat[i]);
/*for (i=0;i<lastBeat;i++) output_perc_pattern(drumpat[i]);*/
printf("\n");
}

/*
The key match algorithm is based on the work of Craig Sapp
Visual Hierarchical Key Analysis
 https://ccrma.stanford.edu/~craig/papers/05/p3d-sapp.pdf
published in Proceedings of the International Computer Music
Conference,2001,
and the work of Krumhansl and Schmukler.

 Craig Sapp's simple coefficients (mkeyscape)
 Major C scale

The algorithm correlates the pitch class class histogram with
the ssMj or ssMn coefficients trying all 12 key centers, and
looks for a maximum.

The algorithm returns the key, sf (the number of sharps or
flats), and the maximum peak which is relatable to the
level of confidence we have of the result.
*/
static float  ssMj[] = { 1.25, -0.75, 0.25, -0.75, 0.25, 0.25,
  -0.75, 1.25, -0.75, 0.25, -0.75, 0.25};

/* Minor C scale (3 flats)
*/
static float ssMn[] = { 1.25, -0.75, 0.25, 0.25, -0.75, 0.25,
  -0.75, 1.25, 0.25, -0.75, 0.25, -0.75};

static char *keylist[] = {"C", "C#", "D", "Eb", "E", "F",
  "F#", "G", "Ab", "A", "Bb", "B"};

static char *majmin[] = {"maj", "min"};

/* number of sharps or flats for major keys in keylist */
static int maj2sf[] = {0,  7,  2, -3,  4, -1,  6,  1, -4, 3, -2, 5};
static int min2sf[] = {-3, 4, -1, -6, -4,  3, -4  -2, -7, 0, -5, 2};

void keymatch () {
int i;
int r;
int k;
float c2M,c2m,h2,hM,hm;
float rmaj[12],rmin[12];
float hist[12];
float best;
int bestIndex,bestMode;
int sf; /* number of flats or sharps (flats negative) */ 
int total;
float fnorm;

c2M = 0.0;
c2m = 0.0;
h2 = 0.0;
best = 0.0;
bestIndex = 0;
bestMode = -1;
total =0;
for (i=0;i<12;i++) {
   total += pitchhistogram[i];
   }
for (i=0;i<12;i++) {
   hist[i] = (float) pitchhistogram[i]/(float) total;
   }
fnorm = 0.0;
for (i=0;i<12;i++) {
   fnorm = hist[i]*hist[i] + fnorm;
   }
fnorm = sqrt(fnorm);
for (i=0;i<12;i++) {
   hist[i] = hist[i]/fnorm;
   }

for (i=0;i<12;i++) {
  c2M += ssMj[i]*ssMj[i];
  c2m += ssMn[i]*ssMn[i];
  h2 += hist[i]*hist[i]; 
  }
if (h2 < 0.0001) {
  printf("zero histogram\n");
  return;
  }
for (r=0;r<12;r++) {
  hM = 0.0;
  hm = 0.0;
  for (i=0;i<12;i++) {
    k = (i - r) % 12;
    if (k < 0) k = k + 12;
    hM += hist[i]*ssMj[k];
    hm += hist[i]*ssMn[k];
    }
  rmaj[r] = hM/sqrt(h2*c2M);
  rmin[r] = hm/sqrt(h2*c2m);
  }

for (r=0;r<12;r++) {
  if(rmaj[r] > best) {
      best = rmaj[r];
      bestIndex = r;
      bestMode = 0;
      }
  if(rmin[r] > best) {
      best = rmin[r];
      bestIndex = r;
      bestMode = 1;
      }
  }
if (bestMode == 0) sf = maj2sf[bestIndex];
else sf = min2sf[bestIndex];
 
printf("\nkey %s%s %d %f",keylist[bestIndex],majmin[bestMode],sf,best);
printf("\nrmaj ");
for (r=0;r<12;r++) printf("%7.3f",rmaj[r]);
printf("\nrmin ");
for (r=0;r<12;r++) printf("%7.3f",rmin[r]);
}


void percsummary () {
int i;
int bassindex,snareindex;
int bassmax,snaremax;
int bassdrum[] = {35,36,41,45};
int snaredrum[] = {37,38,39,40};
bassmax = 0;
snaremax = 0;
for (i=0;i<4;i++) {
  if(drumhistogram[bassdrum[i]] > bassmax) {
      bassindex = bassdrum[i];
      bassmax = drumhistogram[bassindex];
      }
  if(drumhistogram[snaredrum[i]] > snaremax) {
      snareindex = snaredrum[i];
      snaremax = drumhistogram[snareindex];
      }
  }
if (bassmax && snaremax) {
	printf("bass %d %d\n",bassindex,bassmax);
	printf("snare %d %d\n",snareindex,snaremax);
   } else {
        printf("missing bass or snare\n");
	return;
   }
dualDrumPattern(bassindex,snareindex);
}

void outputPitchClassHistogram() {
int i;
float activity;
activity = 0;
for (i=0;i<12;i++) activity += (float) pitchclass_activity[i];
for (i=0;i<11;i++) printf("%5.3f,",pitchclass_activity[i]/activity);
printf("%5.3f",pitchclass_activity[11]/activity);
printf("\n");
}


  
void corestatsOutput() {
int i;
int nchannels;
nchannels = 0;
for (i=1;i<17;i++) 
  if (channel_active[i] > 0) nchannels++;
printf("%d\t%d\t%d\t%d\t%d\t%d\n",lasttrack,nchannels, division,bpm,lastEvent,lastBeat);
/*printf("%d\n",tempocount);*/
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
  arg = getarg("-pulseanalysis",argc,argv);
  if (arg != -1) {
	  pulseanalysis = 1;
	  stats = 0;
          }

  arg = getarg("-corestats",argc,argv);
  if (arg != -1) {
	  corestats = 1;
	  stats = 0;
          }

  arg = getarg("-panal",argc,argv);
  if (arg != -1) {
	  percanalysis = 1;
	  stats = 0;
          }

  arg = getarg("-ppat",argc,argv);
  if (arg != -1) {
	  percpattern = 1;
	  stats = 0;
          }

  arg = getarg("-ppatfor",argc,argv);
  if (arg != -1) {
	  percpatternfor = 1;
	  stats = 0;
	  if (arg != -1 && arg <argc) {
		  percnum = readnum(argv[arg]);
	          printf("percnum = %d\n",percnum);
                  }
          }

  arg = getarg("-nseqfor",argc,argv);
  if (arg != -1) {
	  nseqfor = 1;
	  stats = 0;
	  if (arg != -1 && arg <argc) {
		  nseqchn = readnum(argv[arg]);
	          printf("nseqch = %d\n",nseqchn);
                  }
          }

  arg = getarg("-nseq",argc,argv);
  if (arg != -1) {
	  nseqfor = 1;
	  stats = 0;
          nseqchn = -1;
          }

  arg = getarg("-ppathist",argc,argv);
  if (arg != -1) {
	  percpatternhist = 1;
	  stats = 0;
          }

  arg = getarg("-pitchclass",argc,argv);
  if (arg != -1) {
	  pitchclassanalysis = 1;
	  stats = 0;
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
    printf("         -corestats\n");
    printf("         -pulseanalysis\n");
    printf("         -panal\n");
    printf("         -ppat\n");
    printf("         -ppatfor pitch\n");
    printf("         -ppathist\n");
    printf("         -pitchclass\n");
    printf("         -nseq\n");
    printf("         -nseqfor channel\n");
    printf("         -ver version number\n");
    printf("         -d <number> debug parameter\n");
    printf(" The input filename is assumed to be any string not\n");
    printf(" beginning with a - (hyphen).\n");
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

void loadEvents() {
int i;
initfunc_for_loadNoteEvents();
Mf_getc = filegetc;
maximumPulse = 0;
mfread();
lastBeat = maximumPulse/division;
load_finish();
if (pulseanalysis) pulseHistogram(); 
if (percanalysis) {
	drumanalysis();
        for (i = 0; i< 100; i++) {
          if (drumhistogram[i] > 0) printf("%d %d\t",i,drumhistogram[i]);
        }
        printf("\n");
      }
if (percpattern) {
    drumanalysis();
    percsummary();
    output_drumpat();
    }
if (percpatternfor) {
    drumpattern(percnum);
    output_drumpat();
    }
if (percpatternhist) {
    drumanalysis();
    percsummary();
    drumPatternHistogram();
    }
if (nseqfor) {
    noteseqmap(nseqchn);
    }
if (corestats) corestatsOutput();
if (pitchclassanalysis) {
   pitchClassAnalysis();
   outputPitchClassHistogram(); 
   }
}



int main(argc,argv)
char *argv[];
int argc;
{
  FILE *efopen();
  int arg;

  arg = process_command_line_arguments(argc,argv);
  if(stats == 1)  midistats(argc,argv);
  if(pulseanalysis || corestats || percanalysis ||\
    percpatternfor || percpattern || percpatternhist ||\
    pitchclassanalysis || nseqfor) loadEvents();
  return 0;
}

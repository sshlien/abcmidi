/* abcmatch.c 
This contains the main program and core functions of the program to
compare abc files. 

The abc file may either be a single tune or a compilation
of tunes. The program searches for specific bars in the
file and returns its positions in the file. There are
various matching criteria ranging from exact to approximate
match. The program assumes there is a file called match.abc
containing a description of the bars to be matched. Though
you can run abcmatch as a stand alone program, it was 
really designed to run with a graphical user interface such
as runabc.tcl (version 1.59 or higher).


Limitations: 
tied notes longer than 8 quarter notes are ignored.

Road map to the code:

Matching:
 The templates derived from match.abc are stored in the
 global arrays tpmidipitch, tpnotelength, tpnotes, tpbars, and
 tpbarlineptr.

 If temporal quantization is selected, (i.e. qnt >0) then
 the template is represented by mpitch_samples[] and msamples[].
 The temporal quantization representation is created by the
 function make_bar_image ().  All the bars in the template
 file match.abc are converted and stored in mpitch_samples
 and msamples[] when match.abc is processed. However, the bars
 in the tunes compared are converted only as needed.

 match_notes() is called if there is no temporal quantization
 (exact matching).
 match_samples() is called if there is temporal quantization.

 difference_midipitch() is called if we are doing contour
 matching on the temporal quantized images. For exact matching
 (i.e. qnt == 0), when contouring is specified (con ==1) the
 differencing (and possibly quantization of the contour) is
 performed in the function match_notes.
 
 count_matched_tune_bars () is called if only want an overview
 (brief ==1).
 find_and_report_matching_bars() is called if we want the details
 (brief !=1);
*/



#define VERSION "1.82 June 14 2022 abcmatch"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "abc.h"
#include "parseabc.h"

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))

#define BAR -1000
#define RESTNOTE -2000
#define ANY -3000

enum ACTION
{
  none,
  cpitch_histogram,
  cwpitch_histogram,
  clength_histogram,
  cpitch_histogram_table,
  interval_pdf,
  interval_pdf_table
} action;


int *checkmalloc (int);
extern int getarg (char *, int, char **);
void free_feature_representation ();

/* variables shared with abcparse.c and abcstore.c */
/* many of these variables are used in event_int which has
   been moved to this file.
*/
extern int *pitch, *num, *denom, *pitchline;
extern char **atext, **words;
extern featuretype *feature;
extern int notes;
extern int note_unit_length;
extern int time_num, time_denom;
extern int sf, mi;
extern FILE *fp;
extern int xrefno;
extern int check, nowarn, noerror, verbose, maxnotes, xmatch, dotune;
extern int maxtexts, maxwords;
extern int voicesused;

/* midipitch: pitch in midi units of each note in the abc file. A pitch
   of zero is reserved for rest notes. Bar lines are signaled with BAR.
   If contour matching is used, then all the pitch differences are offsetted
   by 256 to avoid the likelihood of a zero difference mistaken for a
   rest.
   notelength: represents the length of a note. A quarter note is 24 units
   (assuming L:1/4 in abc file) and all other note sizes are proportional.
   Thus a half note is 48 units, a sixteenth note is 6 units.
   nnotes: the number of notes in the midipitch array.
   nbars:  the number of bar lines in the midipitch array.
   barlineptr: indicates the index in midipitch, notelength of the start
   of bar lines.
*/

/* data structure for input to matcher. */
int imidipitch[10000];		/* pitch-barline midi note representation of input tune */
int inotelength[10000];		/* notelength representation of input tune */
int innotes;			/*number of notes in imidipitch,inotelength representation */
int inbars;			/*number of bars in input tune */
int ibarlineptr[2000];		/*pointers to bar lines in imidipitch */
int itimesig_num, itimesig_denom;
int imaxnotes = 10000;		/* maximum limits of this program */
int imaxbars = 2000;
int resolution = 12;		/* default to 1/8 note resolution */
int anymode = 0;		/* default to matching all bars */
int ignore_simple = 0;		/* ignore simple bars */
int con = 0;			/* default for no contour matching */
int qnt = 0;			/* pitch difference quantization flag */
int brief = 0;			/* set brief to 1 if brief mode */
int cthresh = 3;		/* minimum number of common bars to report */
int fixednumberofnotes=0;       /* for -fixed parameter */
int fileindex = -1;
int wphist, phist, lhist;	/* flags for computing pitch or length histogram */
int pdftab;			/* flag for computing pitch pdf for each tune */
int qntflag = 0;		/* flag for turning on contour quantization */
int wpintv = 0;			/* flag for computing interval pdf */
int wpipdf  = 0;		/* flag for computing interval pdf for each tune*/
int norhythm = 0;		/* ignore note lengths */
int levdist = 0;		/* levenshtein distance */

char *templatefile;

/* data structure for matcher (template) (usually match.abc)*/
int tpmidipitch[1000];		/*pitch-barline representation for template */
int tpnotelength[1000];		/*note lengths for template */
int tpnotes;			/* number of notes in template */
int tpbars;			/* number of bar lines in template */
int tpbarlineptr[300];		/* I don't expect 300 bar lines, but lets play safe */
int tptimesig_num, tptimesig_denom;
int tpmaxnotes = 1000;		/* maximum limits of this program */
int tpmaxbars = 300;
unsigned char tpbarstatus[300];

int pitch_histogram[128];
int weighted_pitch_histogram[128];
int length_histogram[144];
int interval_histogram[128];
int lastpitch;

/* compute the midi offset for the key signature. Since
   we do not have negative indices in arrays, sf2midishift[7]
   corresponds to no flats/no sharps, ie. C major scale.
   If sf is the number of sharps (when positive) and the number of
   flats when negative, then sf2midishift[7+i] is the offset in
   semitones from C. Thus G is 7, D is 2, Bb is 10 etc.
*/
int sf2midishift[] = { 11, 6, 1, 8, 3, 10, 5, 0, 7, 2, 9, 4, 11, 6, 1, };


/* when using a resolution greater than 0, time resolution
 * is reduced by a certain factor. The resolution is reduced
 * in the arrays ipitch_samples and mpitch_samples. We need
 * more room in mpitch_samples for the template, since we
 * use this template over and over. Therefore we compute it
 * and store it for the entire template. On the other hand, the
 * input tune is always changing, so we only store one  bar at
 * a time for the input tune.
 */

int ipitch_samples[400], isamples;
int mpitch_samples[4000], msamples[160];	/* maximum number of bars 160 */

char titlename[48];
char keysignature[16];

int tpxref = 0; /* template reference number */
int tp_fileindex = 0; /* file sequence number for tpxref */


void
make_note_representation (int *nnotes, int *nbars, int maxnotes, int maxbars,
			  int *timesig_num, int *timesig_denom,
			  int *barlineptr, int *notelength, int *midipitch)
/* converts between the feature,pitch,num,denom representation to the
   midipitch,notelength,... representation. This simplification does
   not preserve chords, decorations, grace notes etc.
*/
{
  float fract;
  int i;
  int skip_rests, multiplier, inchord, ingrace;
  int maxpitch=0; /* [SDG] 2020-06-03 */
  *nnotes = 0;
  *nbars = 0;
  inchord = 0;
  ingrace = 0;
  skip_rests = 0;
  *timesig_num = time_num;
  *timesig_denom = time_denom;
  multiplier = (int) 24.0;
  barlineptr[*nbars] = 0;
  for (i = 0; i < notes; i++)
    {
      switch (feature[i])
	{
	case NOTE:
	  if (inchord)
	    maxpitch = MAX (maxpitch, pitch[i]);
	  else if (ingrace)
	    break;
	  else
	    {
	      midipitch[*nnotes] = pitch[i];
	      fract = (float) num[i] / (float) denom[i];
	      notelength[*nnotes] = (int) (fract * multiplier + 0.01);
	      (*nnotes)++;
	    }
	  break;
	case TNOTE:
	  midipitch[*nnotes] = pitch[i];
	  fract = (float) num[i] / (float) denom[i];
	  notelength[*nnotes] = (int) (fract * multiplier + 0.01);
	  (*nnotes)++;
	  skip_rests = 2;
	  break;
	case REST:
	  if (skip_rests > 0)
	    {
	      skip_rests--;
	      break;
	    }
	  else			/* for handling tied notes */
	    {
	      midipitch[*nnotes] = RESTNOTE;
	      fract = (float) num[i] / (float) denom[i];
	      notelength[*nnotes] = (int) (fract * multiplier + 0.01);
	      (*nnotes)++;
	      break;
	    }
	case CHORDON:
	  inchord = 1;
	  maxpitch = 0;
	  break;
	case CHORDOFF:
	  inchord = 0;
	  midipitch[*nnotes] = maxpitch;
	  fract = (float) num[i] / (float) denom[i];
	  notelength[*nnotes] = (int) (fract * multiplier + 0.01);
	  (*nnotes)++;
	  break;
	case GRACEON:
	  ingrace = 1;
	  break;
	case GRACEOFF:
	  ingrace = 0;
	  break;
	case DOUBLE_BAR:
	case SINGLE_BAR:
	case REP_BAR:
	case REP_BAR2:
	case BAR_REP:
/* bar lines at the beginning of the music can cause the
   bar numbering to be incorrect and loss of synchrony with
   runabc. If no notes were encountered and nbars = 0, 
   then we are at the beginning and we wish to bypass
   these bar line indications. Note bar numbering starts
   from 0. [SS] 2013-11-17
*/
          if (*nnotes > 0) {  /* [SS] 2021-11-25 */
	    midipitch[*nnotes] = BAR;
	    notelength[*nnotes] = BAR;
	    (*nnotes)++;
	    if (*nbars < maxbars) (*nbars)++;
            else printf("abcmatch too many bars\n");
            }
	  barlineptr[*nbars] = *nnotes;
	  break;
	case TIME:
	  *timesig_num = num[i];
	  *timesig_denom = denom[i];
           if (*timesig_num == 2 && *timesig_denom ==2) {
              *timesig_num = 4;
              *timesig_denom = 4;
              /* [SS] 2013-11-12 */
              }
	  break;
	default:
	  break;
	}
      if (*nnotes > imaxnotes)
	{
	  printf ("ran out of space for midipitch for xref %d\n",xrefno);
	  exit (0);
	}
      if (*nbars > imaxbars)
	{
	  printf ("ran out of space for barlineptr for xref %d\n",xrefno);
	  exit (0);
	}
    }
  midipitch[*nnotes + 1] = BAR;	/* in case a final bar line is missing */
/*printf("refno =%d  %d notes %d bar lines  %d/%d time-signature %d sharps\n"
,xrefno,(*nnotes),(*nbars),(*timesig_num),(*timesig_denom),sf);*/
#ifdef DEBUG
printf("nbars = %d\n",*nbars);
for (i=0;i<*nbars;i++) printf("barlineptr[%d] = %d\n",i,barlineptr[i]);
i = 0;
while (i < 50) {
  for (j=i;j<i+10;j++) printf("%d ",midipitch[j]); 
  printf("\n");
  i +=10;
  }
printf("\n\n");
#endif
}


/* This function is not used yet. */
int
quantize5 (int pitch)
{
  if (pitch < -4)
    return -2;
  if (pitch < -1)
    return -1;
  if (pitch > 4)
    return 2;
  if (pitch > 1)
    return 1;
  return 0;
}

int quantize7(int pitch)
{
if (pitch < -4)
   return -3;
if (pitch < -2)
   return -2;
if (pitch < 0)
   return -1;
if (pitch > 4)
   return 3;
if (pitch >2)
   return 2;
if (pitch >0)
   return 1;
return 0;
}
  


void
init_histograms ()
{
  int i;
  for (i = 0; i < 128; i++)
    pitch_histogram[i] = 0;
  for (i = 0; i < 128; i++)
    weighted_pitch_histogram[i] = 0;
  for (i = 0; i < 144; i++)
    length_histogram[i] = 0;
  for (i = 0; i < 128; i++)
    interval_histogram[i] = 0;
  lastpitch = 0;
}


void
compute_note_histograms ()
{
  int i, index, delta;
  for (i = 0; i < innotes; i++)
    {
      index = imidipitch[i];
      if (index < 0)
	continue;		/* [SS] 2013-02-26 */
      pitch_histogram[index]++;
      weighted_pitch_histogram[index] += inotelength[i];
      index = inotelength[i];
      if (index > 143)
	index = 143;
      length_histogram[index]++;
      if (lastpitch != 0) {delta = imidipitch[i] - lastpitch;
                           delta += 60;
                           if (delta > -1 && delta < 128) interval_histogram[delta]++;
                           }
      lastpitch = imidipitch[i];
    }
}


void
print_length_histogram ()
{
  int i;
  printf ("\n\nlength histogram\n");
  for (i = 0; i < 144; i++)
    if (length_histogram[i] > 0)
      printf ("%d %d\n", i, length_histogram[i]);
}


void
print_pitch_histogram ()
{
  int i;
  printf ("pitch_histogram\n");
  for (i = 0; i < 128; i++)
    if (pitch_histogram[i] > 0)
      printf ("%d %d\n", i, pitch_histogram[i]);
}

void
print_weighted_pitch_histogram ()
{
  int i;
  printf ("weighted_pitch_histogram\n");
  for (i = 0; i < 128; i++)
    if (weighted_pitch_histogram[i] > 0)
      printf ("%d %d\n", i, weighted_pitch_histogram[i]);
}

void
print_interval_pdf()
{
  int i;
  printf ("interval_histogram\n");
  for (i = 0; i < 128; i++)
    if (interval_histogram[i] > 0)
      printf ("%d %d\n", i-60, interval_histogram[i]);
}


void
make_and_print_pitch_pdf ()
{
  float pdf[12], sum;
  int i, j;
  for (i = 0; i < 12; i++)
    pdf[i] = 0.0;
  sum = 0.0;
  for (i = 1; i < 128; i++)
    {
      j = i % 12;
      pdf[j] += (float) weighted_pitch_histogram[i];
      sum += (float) weighted_pitch_histogram[i];
    }
  if (sum <= 0.000001)
    sum = 1.0;
  for (i = 0; i < 12; i++)
    {
      pdf[i] = pdf[i] / sum;
      printf ("%d %5.3f\n", i, pdf[i]);
    }
}

void
make_and_print_interval_pdf ()
{
  float pdf[24], sum;
  int i, j;
  for (i = 0; i < 24; i++)
    pdf[i] = 0.0;
  sum = 0.0;
  for (i = 48; i < 72; i++)
    {
      j = i-48;
      pdf[j] = (float) interval_histogram[i];
      sum += (float) interval_histogram[i];
    }
  if (sum <= 0.000001)
    sum = 1.0;
  for (i = 0; i < 24; i++)
    {
      pdf[i] = pdf[i] / sum;
      printf ("%d %5.3f\n", i-12, pdf[i]);
    }
}

void
difference_midipitch (int *pitch_samples, int nsamples)
{
  int i;
  int lastsample, newsample;
  lastsample = -1;
  for (i = 0; i < nsamples; i++)
    {
      newsample = pitch_samples[i];
      if (newsample == RESTNOTE)
	{
	  pitch_samples[i] = RESTNOTE;
	  continue;
	}

      else if (lastsample == -1)
	{
	  pitch_samples[i] = ANY;
	}
      else
	{
	  pitch_samples[i] = newsample - lastsample;
	  if (qntflag > 0)
	    pitch_samples[i] = quantize7 (pitch_samples[i]);
	}
      lastsample = newsample;
    }
}



int
make_bar_image (int bar_number, int resolution,
		int *barlineptr, int *notelength, int nnotes, int delta_pitch,
		int *midipitch, int *pitch_samples)
{
/* the function returns the midipitch at regular time interval
   for bar %d xref %d\n,bar_number,xrefnos
   (set by resolution) for a particular bar. Thus if you have
   the notes CDEF in a bar (L=1/8), then integrated_length
   will contain 12,24,36,48. If resolution is set to 6, then
   pitch_samples will return the pitch at time units 0, 6, 12,...
   mainly CCDDEEFF or 60,60,62,62,64,64,65,65 in midipitch.
   The pitch is shifted by delta_pitch to account for a different
   key signature in the matching template.

   input: midipitch[],notelength,resolution,delta_pitch,nnotes
          bar_number
   output: pitch_samples

   the function returns the number of pitch_samples i creates

*/
  int integrated_length[50];	/* maximum of 50 notes in a bar */
/* integrated_length is the number of time units in the bar after note i;
*/
  int offset, lastnote, lastpulse, lastsample;
  int i, j, t;
  offset = barlineptr[bar_number];
/* double bar is always placed at the beginning of the tune */

  i = 1;
  integrated_length[0] = notelength[offset];
  lastnote = 0;
  while (notelength[i + offset] != BAR)
    {
      if (notelength[i + offset] > 288)
	return -1;		/* don't try to handle notes longer than 2 whole */
      integrated_length[i] =
	integrated_length[i - 1] + notelength[i + offset];
      lastnote = i;
      i++;
      if (i + offset > nnotes)
	{
	  /* printf("make_bar_image -- running past last note for bar %d xref %d\n",bar_number,xrefno); */
	  break;
	}
      if (i > 49)
	{
	  printf ("make_bar_image -- bar %d has too many notes for xref %d\n",
		  bar_number, xrefno);
	  break;
	}
    }
  lastpulse = integrated_length[lastnote];
  i = 0;
  j = 0;
  t = 0;
  while (t < lastpulse)
    {
      while (t >= integrated_length[i])
	i++;
      while (t < integrated_length[i])
	{
	  if (midipitch[i + offset] == RESTNOTE)
	    pitch_samples[j] = RESTNOTE;	/* REST don't transpose */
	  else
	    pitch_samples[j] = midipitch[i + offset] + delta_pitch;
	  j++;
	  t += resolution;
	  if (j >= 400)
	    {
	      printf
		("make_bar_image -- pitch_sample is out of space for bar %d xrefno = %d\n",
		 bar_number, xrefno);
	      break;
	    }
	}
    }
  lastsample = j;
  t = 0;
  return lastsample;
}

/* for debugging */
void
print_bars (int mbar_number, int ibar_number)
{
  int i;
  int ioffset, moffset;
  ioffset = ibarlineptr[ibar_number];
  moffset = tpbarlineptr[mbar_number];
  i = 0;
  while (tpmidipitch[i + moffset] != BAR)
    {
      printf ("%d %d\n", tpmidipitch[i + moffset], imidipitch[i + ioffset]);
      i++;
    }
  printf ("\n");
}



#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))
 
int levenshtein(int *s1, int *s2, int s1len, int s2len) {
    int  x, y, lastdiag, olddiag; /* [SS] 2015-10-08 removed unsigned */
    int column[33];
    for (y = 1; y <= s1len; y++)
        column[y] = y;
    for (x = 1; x <= s2len; x++) {
        column[0] = x;
        for (y = 1, lastdiag = x-1; y <= s1len; y++) {
            olddiag = column[y];
            column[y] = MIN3(column[y] + 1, column[y-1] + 1, lastdiag + (s1[y-1] == s2[x-1] ? 0 : 1));
            lastdiag = olddiag;
        }
       if (column[x] >= levdist) return levdist;
    }
    if (column[s1len] < levdist && s1len/(2*levdist)>= 1) return 0;
    return(column[s1len]);
}

int perfect_match (int *s1, int *s2, int s1len) {
int i;
for (i=0;i<s1len;i++) {
  if (s1[i] != s2[i]) return -1;
  }
return 0;
}


/* absolute match - matches notes relative to key signature */
/* It is called if the resolution variable is set to 0.     */
/* -------------------------------------------------------- */

int
match_notes (int mbar_number, int ibar_number, int delta_pitch)
{
  int i, notes;
  int ioffset, moffset;
  int tplastnote,lastnote; /* for contour matching */
  int deltapitch,deltapitchtp;
  int string1[32],string2[32];

  ioffset = ibarlineptr[ibar_number];
  moffset = tpbarlineptr[mbar_number];
  /*printf("ioffset = %d moffset = %d\n",ioffset,moffset);*/ 
  i = 0;
  notes = 0;
  lastnote = 0;
  tplastnote =0;
  if (tpmidipitch[moffset] == BAR)
    return -1;			/* in case nothing in bar */
  while (tpmidipitch[i + moffset] != BAR)
    {
      /*printf("%d %d\n",imidipitch[i+ioffset],tpmidipitch[i+moffset]);*/
      if (imidipitch[i + ioffset] == RESTNOTE
	  && tpmidipitch[i + moffset] == RESTNOTE)
	{
	  i++;
	  continue;
	}			/* REST -- don't transpose */
      if (imidipitch[i + ioffset] == -1 || tpmidipitch[i + moffset] == -1)
	{
         printf("unexpected negative pitch at %d or %d for xref %d\n",i+ioffset,i+moffset,xrefno);
	  i++;
	  continue;
	}			/* unknown contour note */


      if (norhythm == 1) {
        inotelength[i + ioffset] =  0;
        tpnotelength[i + moffset] = 0;
        }

      if (con == 1) {
/* contour matching */
        if (tplastnote !=0) {
          deltapitchtp = tpmidipitch[i+moffset] - tplastnote;
          deltapitch = imidipitch[i+ioffset] - lastnote;
          tplastnote = tpmidipitch[i+moffset];
          lastnote = imidipitch[i+ioffset];
	  if (qntflag > 0) {
	     deltapitch = quantize7 (deltapitch);
             deltapitchtp = quantize7(deltapitchtp);
             }
          string1[notes] = 256*deltapitch + inotelength[i + ioffset];
          string2[notes] = 256*deltapitchtp + tpnotelength[i + moffset];
          if (notes < 32) notes++;
          else printf("notes > 32\n");

         /* printf("%d %d\n",deltapitch,deltapitchtp);*/
          }
       else { /* first note in bar - no lastnote */
         tplastnote = tpmidipitch[i+moffset];
         lastnote = imidipitch[i+ioffset];
         }
     } 
      else  {
/* absolute matching (with transposition) */
/*printf("%d %d\n",imidipitch[i+ioffset],tpmidipitch[i+moffset]-delta_pitch);*/
      string1[notes] = 256*imidipitch[i+ioffset] + inotelength[i + ioffset];
      string2[notes] = 256*(tpmidipitch[i+moffset] - delta_pitch) + tpnotelength[i + moffset];
      if (notes < 32) notes++;
      else printf("notes > 32\n");
      }
  i++;
  }    
  if (imidipitch[i + ioffset] != BAR)
    return -1;			/* in case template has fewer notes */

  if (notes < 2)
    return -1;
  if (levdist == 0) return perfect_match(string1,string2,notes);
  else return levenshtein(string1,string2,notes,notes);
}



/* absolute match - matches notes relative to key signature */
/* this matching algorithm ignores bar lines and tries to match
   a fixed number of notes set by fnotes. It is unnecessary for
   the time signatures in the template and tune to match.   */

int
fixed_match_notes (int fnotes, int mbar_number, int ibar_number, int delta_pitch)
{
  int i,j, notes;
  int ioffset, moffset;
  int tplastnote,lastnote; /* for contour matching */
  int deltapitch,deltapitchtp;
  int string1[32],string2[32];
  ioffset = ibarlineptr[ibar_number];
  moffset = tpbarlineptr[mbar_number];
  /*printf("ioffset = %d moffset = %d\n",ioffset,moffset);*/ 
  i = j = 0;
  notes = 0;
  lastnote = 0;
  tplastnote =0;
  while (notes < fnotes)
    {
      /*printf("%d %d\n",imidipitch[j+ioffset],tpmidipitch[i+moffset]);*/
      if (imidipitch[j + ioffset] == RESTNOTE
	  || imidipitch[j + ioffset] == BAR)
	{
          j++;
	  continue;
	}			/* pass over RESTS or BARS */
      if (tpmidipitch[i + moffset] == RESTNOTE
	  || tpmidipitch[i + moffset] == BAR)
	{
	  i++;
	  continue;
	}			/* pass over RESTS or BARS */

      if (imidipitch[j + ioffset] == -1 || tpmidipitch[i + moffset] == -1)
	{
         printf("unexpected negative pitch at %d or %d for xref %d\n",i+ioffset,i+moffset,xrefno);
	  i++;
          j++;
	  continue;
	}			/* unknown contour note */

      if (norhythm == 1) {
        inotelength[j + ioffset] =  0;
        tpnotelength[i + moffset] = 0;
        }

      if (con == 1) {
/* contour matching */
        if (tplastnote !=0) {
          deltapitchtp = tpmidipitch[i+moffset] - tplastnote;
          deltapitch = imidipitch[j+ioffset] - lastnote;
          tplastnote = tpmidipitch[i+moffset];
          lastnote = imidipitch[j+ioffset];
	  if (qntflag > 0) {
	     deltapitch = quantize7 (deltapitch);
             deltapitchtp = quantize7(deltapitchtp);
             }
          string1[notes] = 256*deltapitch + inotelength[j + ioffset];
          string2[notes] = 256*deltapitchtp + tpnotelength[i + moffset];
          if (notes < 32) notes++;
          else printf("notes > 32\n");
          
          /* printf("deltapitch  %d %d\n",deltapitch,deltapitchtp);
             printf("length %d %d\n",inotelength[j + ioffset],tpnotelength[i+moffset]);
          */
          if (deltapitch != deltapitchtp) 
            return -1;
/* match succeeded */
         /* printf("%d %d\n",deltapitch,deltapitchtp);*/
          } else { /* first note in bar - no lastnote */
       tplastnote = tpmidipitch[i+moffset];
       lastnote = imidipitch[j+ioffset];
       }
     } 
      else { 
/* absolute matching (with transposition) */
     /*printf("%d %d\n",imidipitch[j+ioffset],tpmidipitch[i+moffset]-delta_pitch);
       printf("%d %d\n",inotelength[j+ioffset],tpnotelength[i+moffset]);
     */
     
      string1[notes] = 256*imidipitch[j+ioffset] + inotelength[j + ioffset];
      string2[notes] = 256*(tpmidipitch[i+moffset] - delta_pitch) + tpnotelength[i + moffset];
      if (notes < 32) notes++;
      else printf("notes > 32\n");
      }

  i++;
  j++;
  }
  if (notes < 2)
    return -1;
  /*printf("ioffset = %d moffset = %d\n",ioffset,moffset);*/
  if (levdist == 0) return perfect_match(string1,string2,notes);
  else return levenshtein(string1,string2,notes,notes);
}



/* for debugging */
void
print_bar_samples (int mmsamples, int *mmpitch_samples)
{
  int i;
  for (i = 0; i < mmsamples; i++)
    printf ("%d  %d\n", mmpitch_samples[i], ipitch_samples[i]);
}


int
match_samples (int mmsamples, int *mmpitch_samples)
{
  int i;
  int changes;
  int last_sample;
  int string1[32],string2[32];
  int j;
  if (mmsamples != isamples)
    return -1;
  changes = 0;
  last_sample = ipitch_samples[0];	/* [SS] 2012-02-05 */
  j = 0;
  for (i = 0; i < mmsamples; i++)
    {
      if (ipitch_samples[i] == -1 || mmpitch_samples[i] == -1)
	continue;		/* unknown contour note (ie. 1st note) */
      string1[j] = ipitch_samples[i];
      string2[j] = mmpitch_samples[i];
      j++;
      if (last_sample != ipitch_samples[i])
	{
	  last_sample = ipitch_samples[i];
	  changes++;
	}
    }
    if (ignore_simple && changes < 3)
       return -1;
    if (levdist == 0) return perfect_match(string1,string2,j);
    else return levenshtein(string1,string2,j,j);
}



int
match_any_bars (int tpbars, int barnum, int delta_key, int nmatches)
{
/* This function reports any bars in match template  match a particular
 * bar (barnum) in the tune. If a match is found then barnum is 
 * reported.  It runs in one of two modes depending on the value
 * of the variable called resolution.

 * The template is not passed as an argument but is accessed from
 * the global arrays, tpmidipitch[1000] and tpnotelength[1000].
 */

  int kmatches;
  int moffset, j, dif;
/* for every bar in match sample */
  kmatches = nmatches;
  moffset = 0;
  if (resolution > 0)
    {
      isamples = make_bar_image (barnum, resolution, 
				 ibarlineptr, inotelength, innotes, delta_key,
				 imidipitch,ipitch_samples);
      if (isamples < 1)
	return kmatches;
      if (con == 1)
	difference_midipitch (ipitch_samples, isamples);
      for (j = 0; j < tpbars; j++)
	{

	  dif = match_samples (msamples[j], mpitch_samples + moffset);
#if 0 /* debugging */
          printf("bar %d\n",j);
          print_bar_samples(msamples[j],mpitch_samples+moffset);
          /*printf("dif = %d\n\n",dif);*/
#endif


	  moffset += msamples[j];
	  if (dif == 0)
	    {
              if (tpxref > 0) tpbarstatus[j] = 1;
	      kmatches++;
	      if (kmatches == 1)
		printf ("%d %d  %d ", fileindex, xrefno, barnum);
/* subtract one from bar because first bar always seems to be 2 */
	      else
		printf (" %d ", barnum );
	      break;
	    }
	}
    }
  else				/* exact match */
    {
      for (j = 0; j < tpbars; j++)
	{
          if (fixednumberofnotes) 
  	    dif = fixed_match_notes (fixednumberofnotes,j, barnum, delta_key);
          else
  	    dif = match_notes (j, barnum, delta_key);
  

	  if (dif == 0)
	    {
              if (tpxref > 0) tpbarstatus[j] = 1;
	      kmatches++;
	      if (kmatches == 1)
		printf ("%d %d  %d ", fileindex, xrefno, barnum);
	      else
		printf (" %d ", barnum );
	      break;
	    }			/* dif condition */
	}			/*for loop */
    }				/* resolution condition */
  return kmatches;
}


int
match_all_bars (int tpbars, int barnum, int delta_key, int nmatches)
{
/* This function tries to match all the bars in the match template
   with  the bars in the bars in the tune. All the template bars
   must match in the same sequence in order to be reported.
   It runs in one of two modes depending on the value of resolution.
*/
  int kmatches;
  int moffset, j, dif;
  int matched_bars[20];
/* for every bar in match sample */
  kmatches = 0;
  moffset = 0;
  if (resolution > 0)
    for (j = 0; j < tpbars; j++)
      {
	isamples = make_bar_image (barnum + j, resolution, 
				   ibarlineptr, inotelength, innotes,
				   delta_key, imidipitch ,ipitch_samples);
	if (con == 1)
	  difference_midipitch (ipitch_samples, isamples);
	dif = match_samples (msamples[j], mpitch_samples + moffset);
	moffset += msamples[j];
	if (dif != 0)
	  return nmatches;
	matched_bars[kmatches] = barnum + j ;
	kmatches++;
	if (j > 15)
	  break;
      }
  else
    for (j = 0; j < tpbars; j++)
      {
        if (fixednumberofnotes)
	  dif = fixed_match_notes (fixednumberofnotes,j, barnum + j, delta_key);
        else
	  dif = match_notes (j, barnum + j, delta_key);
	if (dif != 0)
	  return nmatches;
	matched_bars[kmatches] = barnum + j ;
        /*printf("matched %d\n",barnum+j-1);*/
	kmatches++;
	if (j > 15)
	  break;
      }

  if (nmatches == 0)
    printf ("%d %d ", fileindex, xrefno);
  for (j = 0; j < kmatches; j++)
    printf ("%d ", matched_bars[j]);
  return kmatches + nmatches;
}



void find_and_report_matching_bars
/* top level matching function. Distinguishes between,
   any, all and contour modes and calls the right functions
*/
  (int tpbars, int inbars, int transpose, int anymode, int con)
{
  int i, nmatches;
  if (con == 1)
    {
      transpose = 0;		/* not applicable here */
    }
  nmatches = 0;
  if (anymode == 1)		/* match any bars in template */

    for (i = 0; i < inbars; i++)
      {
	nmatches = match_any_bars (tpbars, i, transpose, nmatches);
      }

  else
    /* match all bars in template in sequence */
    for (i = 0; i < inbars - tpbars; i++)
      {
	nmatches = match_all_bars (tpbars, i, transpose, nmatches);
      }

  if (nmatches > 0)
    printf ("\n");
}


int
find_first_matching_tune_bar (int mbarnumber, int inbars, int transpose)
/* given a bar number in the template, the function looks for
 * the first bar in the tune which matches the template.
 * Unfortunately since we are only looking for single bars,
 * it may not be useful to play around with the resolution
 * parameter.
 */
{
  int i, dif;
  for (i = 1; i < inbars; i++)
    {
      dif = match_notes (mbarnumber, i, transpose);
      if (dif == 0)
	return i;
    }
  return -1;
}




int
find_first_matching_template_bar (int barnumber, int tpbars, int transpose)
/* given a bar number in the tune, the function looks for
 * the first bar in the template which matches the tune bar.
 */
{
  int i, dif;
  for (i = 1; i < tpbars; i++)
    {
      if (fixednumberofnotes)
         dif = fixed_match_notes (fixednumberofnotes,i, barnumber, transpose);
      else
         dif = match_notes (i, barnumber, transpose);
      if (dif == 0)
	return i;
    }
  return -1;
}

/* this function is not used */
int
count_matched_template_bars (int tpbars, int inbars, int transpose)
{
  int i, count, bar;
  count = 0;
  for (i = 0; i < tpbars; i++)
    {
      bar = find_first_matching_tune_bar (i, inbars, transpose);
      /*if (bar >= 0) printf ("bar %d matches %d\n",bar,i); */
      if (bar >= 0)
	count++;
    }
  return count;
}


int
count_matched_tune_bars (int tpbars, int inbars, int transpose)
/* used only by brief mode */
{
  int i, count, bar;
  count = 0;
  for (i=0;i<300;i++) tpbarstatus[i] = 0;
  for (i = 0; i < inbars; i++)
    {
      bar = find_first_matching_template_bar (i, inbars, transpose);
      /*if (bar >= 0) {printf ("bar %d matches %d\n",bar,i); 
         print_bars(bar,i);
         }
       */
      if (bar >= 0) {
	count++;
        tpbarstatus[bar] = 1;
        }
    }
  return count;
}

int count_matching_template_bars ()
{
int i;
int count;
count = 0;
for (i=0;i<300;i++)
  if (tpbarstatus[i] > 0) count++;
return count;
}  

void startfile(); /* links with matchsup.c */

int
analyze_abc_file (char *filename)
{
  FILE *fp;
  fp = fopen (filename, "rt");
  if (fp == NULL)
    {
      printf ("cannot open file %s\n", filename);
      exit (0);
    }
  init_histograms ();
  while (!feof (fp))
    {
      fileindex++;
      startfile ();
      parsetune (fp);
/*     printf("fileindex = %d xrefno =%d\n",fileindex,xrefno); */
/*     printf("%s\n",titlename); */
      if (notes < 10)
	continue;
      /*print_feature_list(); */
      make_note_representation (&innotes, &inbars, imaxnotes, imaxbars,
				&itimesig_num, &itimesig_denom, ibarlineptr,
				inotelength, imidipitch);
      compute_note_histograms ();
      if (action == cpitch_histogram_table)
	{
	  printf ("%4d %s %s\n%s\n", xrefno, filename, keysignature,
		  titlename);
	  make_and_print_pitch_pdf ();
	  init_histograms ();
	}
      if (action == interval_pdf_table)
	{
	  printf ("%4d %s %s\n%s\n", xrefno, filename, keysignature,
		  titlename);
	  make_and_print_interval_pdf ();
	  init_histograms ();
	}

    }
  fclose (fp);
  switch (action)
    {
    case cpitch_histogram:
      print_pitch_histogram ();
      break;
    case cwpitch_histogram:
      print_weighted_pitch_histogram ();
      break;
    case clength_histogram:
      print_length_histogram ();
      break;
    case interval_pdf:
      print_interval_pdf ();
      break;
    default:
      ;
    }
  return (0);
}


void
event_init (argc, argv, filename)
/* this routine is called first by abcparse.c */
     int argc;
     char *argv[];
     char **filename;
{
  int i,j;

  xmatch = 0;
  /* look for code checking option */
  if (getarg ("-c", argc, argv) != -1)
    {
      check = 1;
      nowarn = 0;
      noerror = 0;
    }
  else
    {
      check = 0;
    };
  if (getarg ("-ver", argc, argv) != -1)
    {
      printf ("%s\n", VERSION);
      exit (0);
    }
  /* look for verbose option */
  if (getarg ("-v", argc, argv) != -1)
    {
      verbose = 1;
    }
  else
    {
      verbose = 0;
    };
  j = getarg ("-r", argc, argv);
  if (j != -1 && argc >= j+1)  /* [SS] 2015-02-22 */
    sscanf (argv[j], "%d", &resolution);
  if (getarg ("-a", argc, argv) != -1)
    anymode = 1;
  if (getarg ("-ign", argc, argv) != -1)
    ignore_simple = 1;
  if (getarg ("-con", argc, argv) != -1)
    con = 1;
  if (getarg ("-qnt", argc, argv) != -1)
    {
      qntflag = 1;
      con = 1;
    }
  if (getarg ("-norhythm",argc,argv) != -1)
    {norhythm = 1;
     resolution = 0;
    }

  j = getarg("-lev",argc,argv);
  if (j != -1) {
    if (argv[j] == NULL) {
       printf("'error: expecting a number specifying the maximum levenshtein distance allowed\n");
       exit(0);
       }
       levdist = readnumf(argv[j]);
    }

  j = getarg("-tp",argc,argv);
  if (j != -1) {
    if (argv[j] == NULL) {
        printf ("error: expecting file name and optional reference number\n");
        exit(0);
        }
    free(templatefile);
    templatefile = addstring(argv[j]);
    if (*templatefile == '-') {
       printf ("error: -tp filename must not begin with a -\n");
       exit(0);
       }
    if (argv[j+1] != NULL && isdigit(*argv[j+1])) {
         tpxref = readnumf(argv[j+1]);
        }
    anymode = 1; /* only mode which makes sense for a entire tune template*/
    for (i=0;i<300;i++) tpbarstatus[i] = 0;
    }
 
 
  j = getarg ("-br", argc, argv);
  if (j != -1)
    {
      if (argv[j] == NULL)
	{
	  printf ("error: expecting number after parameter -br\n");
	  exit (0);
	}

      sscanf (argv[j], "%d", &cthresh);
      brief = 1;
    }
 
  j = getarg ("-fixed", argc, argv);
  if (j != -1)
    {
      if (argv[j] == NULL)
	{
	  printf ("error: expecting number after parameter -fixed\n");
	  exit (0);
	}

      sscanf (argv[j], "%d", &fixednumberofnotes);
   }

  wphist = getarg ("-wpitch_hist", argc, argv);
  phist = getarg ("-pitch_hist", argc, argv);
  lhist = getarg ("-length_hist", argc, argv);
  pdftab = getarg ("-pitch_table", argc, argv);
  wpintv = getarg ("-interval_hist",argc,argv);
  wpipdf = getarg ("-interval_table",argc,argv);  

  if (phist >= 0)
    {
      action = cpitch_histogram;
    }
  if (lhist >= 0)
    {
      action = clength_histogram;
    }
  if (wphist >= 0)
    {
      action = cwpitch_histogram;
    }
  if (pdftab >= 0)
    {
      action = cpitch_histogram_table;
    }
  if (wpintv >=0)
   {
     action = interval_pdf;
   }
  if (wpipdf >=0)
   {
     action = interval_pdf_table;
   }

  if (brief == 1)
    resolution = 0;		/* do not compute msamples in main() */
  maxnotes = 3000;
  /* allocate space for notes */
  if (fixednumberofnotes > 0) resolution = 0;
  pitch = checkmalloc (maxnotes * sizeof (int));
  num = checkmalloc (maxnotes * sizeof (int));
  denom = checkmalloc (maxnotes * sizeof (int));
  pitchline = checkmalloc (maxnotes * sizeof (int));
  feature = (featuretype *) checkmalloc (maxnotes * sizeof (featuretype));
  /* and for text */
  atext = (char **) checkmalloc (maxtexts * sizeof (char *));
  words = (char **) checkmalloc (maxwords * sizeof (char *));
  if ((getarg ("-h", argc, argv) != -1) || (argc < 2))
    {
      printf ("abcmatch version %s\n", VERSION);
      printf ("Usage : abcmatch <abc file> [-options] \n");
      printf ("        [reference number] selects a tune\n");
      printf ("        -c returns error and warning messages\n");
      printf ("        -v selects verbose option\n");
      printf ("        -r resolution for matching\n");
      printf ("        -fixed <n> fixed number of notes\n");
      printf ("        -con  pitch contour match\n");
      printf ("        -qnt contour quantization\n");
      printf ("        -lev use levenshtein distance\n");
      printf ("        -ign  ignore simple bars\n");
      printf ("        -a report any matching bars (default all bars)\n");
      printf ("        -br %%d only report number of matched bars when\n\
	    above given threshold\n");
      printf ("        -tp <abc file> [reference number]\n");
      printf ("        -ver returns version number\n");
      printf ("        -pitch_hist pitch histogram\n");
      printf ("        -wpitch_hist interval weighted pitch histogram\n");
      printf ("        -length_hist note duration histogram\n");
      printf ("        -interval_hist pitch interval histogram\n");
      printf ("        -pitch_table separate pitch pdfs for each tune\n");
      printf ("        -interval_table separate interval pdfs for each tune\n");
      exit (0);
    }
  else
    {
      *filename = argv[1];
    };
  /* look for user-supplied output filename */
  dotune = 0;
  parseroff ();
}



int
main (argc, argv)
     int argc;
     char *argv[];
{
  char *filename;
  int i;
  int ikey, mkey;
  int moffset;
  int transpose;
  int mseqno;
  /* sequence number of template (match.abc) 
   * mseqno can differ from xrefnum when running count_matched_tune_bars
   * because there is no guarantee the xref numbers are in
   * sequence in the input file. Hopefully fileindex matches sequence
   * number in script calling this executable.
   */

  int kfile, count;
  int kount;

/* initialization */
  action = none;
  templatefile = addstring("match.abc");
  event_init (argc, argv, &filename);
  init_abbreviations ();


/* get the search template from the file match.abc written
   in abc format. This file is automatically generated by
   runabc.tcl when you are using this search function.
*/

  if (action != none)
    analyze_abc_file (filename);

  else
    {				/* if not computing histograms */
      if (tpxref >0 ) xmatch = tpxref;/* get only tune with ref number xmatch*/
      parsefile (templatefile);
      if (tpxref != 0 && tpxref != xrefno) {
        printf("could not find X:%d in file %s\n",tpxref,filename);
        exit(0);
        }
      mkey = sf2midishift[sf + 7];
      mseqno = xrefno;		/* if -br mode, X:refno is file sequence number */
      /* xrefno was set by runabc.tcl to be file sequence number of tune */
      /*print_feature_list(); */
      make_note_representation (&tpnotes, &tpbars, tpmaxnotes, tpmaxbars,
				&tptimesig_num, &tptimesig_denom, tpbarlineptr,
				tpnotelength, tpmidipitch);

      /* trim off any initial bar lines 
      for (i = 0; i < tpbars; i++)
	{
	  j = i;
	  if (tpmidipitch[tpbarlineptr[i]] != BAR)
	    break;
	}
      for (i = j; i < tpbars; i++)
	tpbarlineptr[i - j] = tpbarlineptr[i];
      tpbars -= j;
     */

      moffset = 0;
/* if not exact match, i.e. resolution > 0 compute to an
   sample representation of the template.
*/
      if (resolution > 0)
	for (i = 0; i < tpbars; i++)
	  {
	    msamples[i] =
	      make_bar_image (i, resolution, 
			      tpbarlineptr, tpnotelength, tpnotes, 0,
			      tpmidipitch, mpitch_samples + moffset);
	    if (con == 1)
	      difference_midipitch (mpitch_samples + moffset, msamples[i]);
	    moffset += msamples[i];
	    if (moffset > 3900)
	      printf ("abcmatch: out of room in mpitch_samples\n");
	  }

/* now process the input file */


      xmatch = 0; /* we do not want to filter any reference numbers here */
      fp = fopen (filename, "rt");
      if (fp == NULL)
	{
	  printf ("cannot open file %s\n", filename);
	  exit (0);
	}

      kfile = 0;
      while (!feof (fp))
	{
	  fileindex++;
	  startfile ();
	  parsetune (fp);
          /*printf("fileindex = %d xrefno =%d\n",fileindex,xrefno); 
            printf("%s\n",titlename); */
          if (tpxref == xrefno) {
             tp_fileindex = fileindex;
             continue;
             }
	  if (notes < 10)
	    continue;
	  ikey = sf2midishift[sf + 7];
	  /*print_feature_list(); */
          if (voicesused) {/*printf("xref %d has voices\n",xrefno);*/
                           continue;
                          }
	  make_note_representation (&innotes, &inbars, imaxnotes, imaxbars,
				    &itimesig_num, &itimesig_denom,
				    ibarlineptr, inotelength, imidipitch);
	    {

/* ignore tunes which do not share the same time signature as the template */
	      if ((itimesig_num != tptimesig_num
		  || itimesig_denom != tptimesig_denom)
                 && fixednumberofnotes == 0)
		continue;


	      transpose = mkey - ikey;
/* we do not know whether to transpose up or down so we will
   go in the direction with the smallest number of steps
  [SS] 2013-11-12
*/
              if (transpose > 6) transpose = transpose -12;
              if (transpose < -6) transpose = transpose + 12;


/* brief mode is used by the grouper in runabc.tcl */
	      if (brief)
		{
		  if (mseqno == fileindex)
		    continue;	/* don't check tune against itself */
		  count = count_matched_tune_bars (tpbars, inbars, transpose);
                  kount = count_matching_template_bars();
		  /*if (count >= cthresh) [SS] 2013-11-26 */
		  if (kount >= cthresh)
		    {
		      if (kfile == 0)
			printf ("%d\n", tpbars);
		      printf (" %d %d %d\n", fileindex, count,kount);
		      kfile++;
		    }
		}

	      else
/* top level matching function if not brief mode */
		find_and_report_matching_bars (tpbars, inbars, transpose,
					       anymode, con);
	    }
	}
      fclose (fp);
      if (tpxref > 0) {
          printf ("%d %d ", tp_fileindex, tpxref);
          for (i = 0; i <tpbars ;i++) 
             if (tpbarstatus[i] != 0) printf("%d ",i);
          printf ("\n");
             }

          
    }
  free_abbreviations ();
  free_feature_representation ();
  return 0;
}

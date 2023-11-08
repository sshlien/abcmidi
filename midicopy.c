/*
 *  This program is a clone of midifile.c created by
 *  Thompson and  Czeiszperger, 1989. As a result there
 *  are many remnant variables and functions from that
 *  file. 
 *
 *  The program is designed to copy a selected part or whole
 *  midi file onto a new midi file. You can select a specific
 *  time interval or specific tracks or channels. The new
 *  midi file can then be played using a standard midi player.
 *  Thus one can listen to any selected voice and time
 *  time interval of a midi file.

 * In order to do this we must keep track of all notes running
 * at a specific time so that we can turn them off when we
 * come to the end of the time segment. The array notechan is
 * used to keep track of which notes are still playing.

 * When we start copying from the middle of a midi file,
 * we must ensure that the player is in the proper state (right
 * tempo, right programs) for that time. Many midi files
 * may change the tempo or instrument arrangements. To ensure
 * the midi file maintains this state we copy all messages  
 * except note_on/note_off prior to the beginning of the
 * start of the output file. The midi time (determined
 * by delta_time) is locked to zero until we reach the
 * start time for the output file.
 *
 * Overview of function calls.
 * ---------------------------
 * All the work is done by build_new_midi_file, which copies the
 * tracks individually thru the function mf_write_track_chunk.
 * mf_write_track_chunk either copies the whole track verbatim
 * or else parses the track and copies each command individually
 * using the function readtrack(). readtrack calls chanmessage()
 * or metaevent () depending on the nature ofthe command.
 *
 * We never use verbatim transfer as of 2019-06-29
 * so we can eliminate this flag.
 *
 * readtrack() reads a particular track selecting the data
 * that it wishes to save in the character array trackdata.
 * 
 * When you specify particular channels to copy, midicopy has
 * to search all the tracks for those channels since it does
 * not know which tracks contain those channels. (It is possible
 * that a channel appears in several tracks. For example, one
 * track contains the notes and another assigns the channel to
 * a program.) The tracks that do not contain the desired channels
 * are not saved to create a cleaner output file.
 */



#define VERSION "1.39 November 07 2023 midicopy"
#include "midicopy.h"
#define NULLFUNC 0
#define NULL 0

#ifndef PCCFIX
#include <stdlib.h>
#endif

#include <stdio.h>
#include <string.h>


/* Functions to be called while processing the MIDI file. */
int (*Mf_arbitrary) () = NULLFUNC;
int (*Mf_seqspecific) () = NULLFUNC;

/* Functions to implement in order to write a MIDI file */
int (*Mf_writetempotrack) () = NULLFUNC;

void mf_write_header_chunk (int, int, int);
void mf_write_tempo (long);
void mf_get_tempo_event (long tempo);
int mf_write_midi_event (int type, int chan, char *data, int size);
int mf_write_meta_event (int type, char *data, int size);
void mf_write_track_chunk (int which_track, FILE * fp);
void mferror (char *s);
int eputc (char c);
void append_to_string ();
void winamp_compatibility_measure ();
void writechanmsg_at_0 ();
void copy_noteoff (int chan, int c1, int c2);

int Mf_nomerge = 0;		/* 1 => continue'ed system exclusives are */
		       /* not collapsed. */
long Mf_currtime = 0L;		/* current time in delta-time units */
long Mf_toberead = 0L;
long delta_time;
float currentseconds = 0.0;
long max_currtime = 0;

long Mf_currcopytime = 0L;	/* time of last copied event */
char *trackdata = NULL;
long trackdata_length, trackdata_size;
/* char *trackstr[64];  [SS] 2017-10-20  2019-07-05*/
char *trackstr[150]; /* [SS] 2023-11-07 */
int trackstr_length[150]; /* [SS] 2017-10-20 2019-07-05* 2023-11-07*/
int trkid = 0;
int activetrack;
int nochanmsg = 1;
long newtempo = 0;		/* to handle tempo [SS] 2013-09-04 */
float speedfactor = 1.00;	/* to handle tempo [SS] 2013-09-06 */
int newspeed = 0;		/* signal application of speedfactor [SS] 2013-09-06 */
int selected_drum = 0;		/* [SS] 2013-09-07 */
int drumlevel = 0;		/* [SS] 2013-09-07 */
int nondrumlevel = 0;		/* [SS] 2013-09-15 */
int mutenodrum = 0;		/* [SS] 2013-09-15 */
int chosen_drum = 0;		/* [SS] 2013-10-01 */
int drumvelocity = 0;		/* [SS] 2013-10-01 */
int attenuation = 70;           /* [SS] 2017-11-27 */
int nobends = 0;                /* [SS] 2017-12-15 */
int nopressure = 0;             /* [SS] 2022-05-05 */
int nocntrl = 0;                /* [SS] 2022-05-05 */
int zerochannels = 0;           /* [SS] 2020-10-09 */

long Mf_numbyteswritten = 0L;
long readvarinum ();
long read32bit ();
long to32bit ();
int read16bit ();
int to16bit ();
char *msg ();
int seconds_to_tick (float seconds);
float tick_to_seconds (int tick);

/* following declaration added 27/8/96 JRA (James Allwright)*/
void badbyte (int);
void metaevent (int);
void sysex ();
void chanmessage (int, int, int);
void msginit ();
int msgleng ();
void msgadd ();
void biggermsg ();
void WriteVarLen (long);


/* tocopy, tfocus, hastempo extended to handle 64 tracks [SS] 2019-07-05*/
int notechan[2048];		/* keeps track of running voices */
int tocopy[64];			/* tracks to copy */
int ctocopy[16];		/* channels to copy */
int dtocopy[82];                /* drums to copy  2019-12-22 */
int tfocus[64];                 /* track focus    2017-11 27*/
int cfocus[16];                 /* channel focus  2017-11-27*/
int drmflag = 0;                /* flag indicating drums selected 2019-12-22 */
int verbatim = 0;		/* flag for verbatim transfer 2019-06-29 */
int haschannel[17];             /* for determining which channels are in use */
int hastempo[64];		/* indicates whether tempo command in track */

FILE *F_in, *fp;
int format, ntrks, division;
int start_tick, end_tick, flag_metaeot;
float start_seconds, end_seconds;
int use_seconds;
int use_beats;
int use_ticks;
int current_tempo = 500000;
float seconds_output;

struct tempostruc
{
  int tick;
  int tempo;
  float seconds;
} tempo_array[2000];
int temposize, tempo_index;

/*          Support stuff                         */


/* The next four functions are used to ensure that all
   notes still on are turned off at the end of the time
   segment.
*/
int
init_notechan ()
{
/* indicate that there are no active notes */
  int i;
  for (i = 0; i < 2048; i++)
    notechan[i] = -1;
  return 0;
}


int
open_note (int chan, int pitch)
/* When a note for a specific channel is turned on, we save the
   time when it was switched on.
*/
{
  int index;
  /*printf("%f %d\n",Mf_currtime/(float) division,pitch); */
  index = 128 * chan + pitch;
  if (index < 0 || index > 2047)
    printf ("illegal chan/pitch %d %d\n", chan, pitch);
  notechan[index] = Mf_currtime;
  return 0;
}


int
close_note (int chan, int pitch)
{
  int index;
  index = 128 * chan + pitch;
  if (notechan[index] == -1) return -1; /*[SS] 2020-01-05  already closed */
  if (index < 0 || index > 2047)
    printf ("illegal chan/pitch %d %d\n", chan, pitch);
  notechan[index] = -1;
  return 0;
}

/* send a noteoff channel command for all notes still playing. */
void
turn_off_all_playing_notes ()
{
  int i, chan, pitch;
  for (i = 0; i < 2048; i++)
    if (notechan[i] >= 0)
      {
	chan = i / 128;
	pitch = i % 128;
	copy_noteoff (chan, pitch, 0);
      }
}



void
append_to_string (int c)
{
  trackdata[trackdata_length] = c;
  trackdata_length++;
  if (trackdata_length > trackdata_size+6) /* [SS] 2019-06-05 */
    {
      printf ("trackdata %ld overflow at %ld for track %d\n",trackdata_length,Mf_currtime,activetrack);
      exit (1);
    }
}


void
replace_byte_in_trackdata (int loc, char val)
{
  trackdata[loc] = val;
}


void
error (char *s)
{
  fprintf (stderr, "Error: %s\n", s);
}


int
cut_beginning ()
{
  if (use_seconds) {
    if(currentseconds < start_seconds) return 1;
    else {return 0;}
  } else if (use_ticks) {
    if (Mf_currtime < start_tick) return 1;
    else {return 0;}
  }
return 0;
}


int
cut_ending ()
{
  if (use_seconds) {
    if(currentseconds > end_seconds) return 1;
    else {return 0;}
 } else if (use_ticks) {
  if (Mf_currtime > end_tick) return 1;
  else {return 0;}
 }
return 0;
}




/*      Midi Channel Commands                            */

void
copy_noteon (int chan, int c1, int c2)
{
  char data[2];
  int notestatus;
  if (cut_beginning ())
    return;
  notestatus = 0; /* [SS] 2020-01-05 */
  if (c2 > 0)
    open_note (chan, c1);
  else
    notestatus = close_note (chan, c1);
  if (notestatus == -1) return; /* [SS] 2020-01-05 already turned off */
  data[0] = (char) c1;
  data[1] = (char) c2;
  mf_write_midi_event (0x90, chan, data, 2);
}



void
copy_noteoff (int chan, int c1, int c2)
{
  char data[2];
  int notestatus; /* [SS] 2020-01-05 */
  if (cut_beginning ())
    return;
  notestatus = close_note (chan, c1);
  if (notestatus == -1) return; /* [SS] 2020-01-05 note already not on */
  data[0] = (char) c1;
  data[1] = (char) c2;
  mf_write_midi_event (0x80, chan, data, 2);
}



void
copy_pressure (int chan, int c1, int c2)
{
  char data[2];
  if (cut_beginning ())
    return;
  data[0] = (char) c1;
  data[1] = (char) c2;
  mf_write_midi_event (0xa0, chan, data, 2);
}


void
copy_parameter (int chan, int c1, int c2)
{
  char data[2];
  if (cut_beginning ())
    return;
  data[0] = (char) c1;
  data[1] = (char) c2;
  mf_write_midi_event (0xb0, chan, data, 2);
}

void
copy_program (int chan, int c1)
{
  char data[1];
  data[0] = (char) c1;
  mf_write_midi_event (0xc0, chan, data, 1);
}

void
copy_chanpressure (int chan, int c1)
{
  char data[1];
  data[0] = (char) c1;
  mf_write_midi_event (0xd0, chan, data, 1);
}

void
copy_pitchbend (int chan, int c1, int c2)
{
  char data[2];
  if (cut_beginning ())
    return;
  data[0] = c1;
  data[1] = c2;
  mf_write_midi_event (0xe0, chan, data, 2);
}




/*       Meta commands are handled here  */

void
copy_metatext (int type, int length, char *m)
{
  mf_write_meta_event (type, m, length);
}

void
midicopy_timesig (c1, c2, c3, c4)
int c1, c2, c3, c4;
{
  char data[4];
  data[0] = (char) c1;
  data[1] = (char) c2;
  data[2] = (char) c3;
  data[3] = (char) c4;
  mf_write_meta_event (0x58, data, 4);
}

void
copy_keysig (c1, c2)
int c1, c2;
{
  char data[2];
  data[0] = (char) c1;
  data[1] = (char) c2;
  mf_write_meta_event (0x59, data, 2);
}


/* Metaevents */
/* 2015-11-14 [SS] */
void
copy_metaseqnum (c1,  c2)
unsigned char c1,c2;
{
  char data[2];
  data[0] = (char) c1;
  data[1] = (char) c2;
  mf_write_meta_event (0x00, (char *) data, 2);
}


void
copy_metaspecial (int length, char *m)
{
  mf_write_meta_event (0x7f, m, length);
}

void
copy_metamisc (int type, int length, char *m)
{
  mf_write_meta_event (type, m, length);
}



void
copy_metaeot ()
{
  void WriteVarLen ();
  WriteVarLen (10);
  Mf_currcopytime += 10;
  eputc ((char) 0xff);
  eputc ((char) 0x2f);
  eputc ((char) 0x00);
}



void
copy_sysex (int length, char *s)
{
  int i;
/* readtrack in midifile3 copies the starting 0xf0 sys command
   followed by the message. In order to copy the input, we
   need to also copy the length of the message. The midi
   specs seem to be not clear, on whether length of message
   should be included, or whether f0 f7 completely delineate
   the sysex command.
*/
  WriteVarLen (Mf_currtime - Mf_currcopytime);
  if (!cut_beginning ())
    Mf_currcopytime = Mf_currtime;
/*printf("delta_time = %d\n",delta_time);*/
  eputc ((char) 0xf0);
  WriteVarLen (length - 1);	/* don't count the starting 0xf0 byte */
  for (i = 1; i < length; i++)
    eputc (s[i]);
}



/* The midi standard requires that we precede the stream
   of midi commands in a track with a number indicating the
   number of bytes in the track. Since we don't know this
   in advance, we just append everything into a long string
   and count the number of bytes in the string before writing
   it to the output file. It uses more memory, but memory
   is now cheap and there are not many midi files one megabyte
   long.
*/

void
alloc_trackdata ()
{
  if (trackdata != NULL)
    free (trackdata);
/* double it since running status is not preserved [SS] 2013-10-08 */
/* add another two bytes to cover winamp_compatibility
/* for very short tracks. [SS] 2017-09-12 */
/* There is no penalty for allocating to much. */
  if (Mf_toberead < 2) Mf_toberead = 64; /* to handle MIDI header */
  trackdata = (char *) malloc (Mf_toberead + Mf_toberead + 2);
  trackdata_size = Mf_toberead + Mf_toberead + 2;
  trackdata_length = 0;
  trackdata[0] = 0;
}






int
readmt (s)			/* read through the "MThd" or "MTrk" header string */
     char *s;
{
  int n = 0;
  char *p = s;
  int c;

  while (n++ < 4 && (c = getc (F_in)) != EOF)
    {
      if (c != *p++)
	{
	  char buff[32];
	  (void) strcpy (buff, "expecting ");
	  (void) strcat (buff, s);
	  mferror (buff);
	}
    }
  return (c);
}




static int
egetc ()
{				/* read a single character and abort on EOF */
  int c = getc (F_in);

  /* if (c == EOF)                    [SS] 2017-09-12 */
  if (c == EOF && Mf_toberead > 0) /* [SS] 2017-09-12 */
    mferror ("premature EOF");
  Mf_toberead--;
  return (c);
}





void
readheader ()
{				/* read a header chunk */

  if (readmt ("MThd") == EOF)
    return;

  Mf_toberead = read32bit ();
  format = read16bit ();
  ntrks = read16bit ();
  division = read16bit ();


  /* flush any extra stuff, in case the length of header is not 6 */
  while (Mf_toberead > 0)
    (void) egetc ();
}


void
append_rest_of_track ()
{
  int err;
  err = fread (trackdata + trackdata_length, 1, Mf_toberead, F_in);
  trackdata_length += err;
  if (err != Mf_toberead)
    error ("fread error in append_rest_of_track()");
  Mf_toberead = 0;
}


void
ignore_rest_of_track ()
{
  while (Mf_toberead > 0)
    egetc ();
}


void
copytrack_verbatim ()
{
  if (readmt ("MTrk") == EOF)
    return;
  Mf_toberead = read32bit ();
  alloc_trackdata ();
  append_rest_of_track ();
}



int
readtrack ()
{				/* read a track chunk */
  /* This array is indexed by the high half of a status byte.  It's */
  /* value is either the number of bytes needed (1 or 2) for a channel */
  /* message, or 0 (meaning it's not  a channel message). */
  static int chantype[] = {
    0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 through 0x70 */
    2, 2, 2, 2, 1, 1, 2, 0	/* 0x80 through 0xf0 */
  };
  long lookfor;
  int c, c1, type;
  int sysexcontinue = 0;	/* 1 if last message was an unfinished sysex */
  int running = 0;		/* 1 when running status used */
  int status = 0;		/* status value (e.g. 0x90==note-on) */
  int laststatus;		/* for running status [SS] 2013-09-10 */
  int needed;
  long varinum;
  int i;                        /* [SS] 2017-10-19 */
  double delta_seconds;

  for (i=0;i<17;i++) haschannel[i] = 0;

  laststatus = 0;		/* [SS] 2013-09-10 */
  tempo_index = 0;

  if (readmt ("MTrk") == EOF)
    return (0);

  Mf_toberead = read32bit ();
  Mf_currtime = 0;
  Mf_currcopytime = 0;
  currentseconds = 0.0;

  alloc_trackdata ();

  /* ignore anything before -from pulses */
  if (cut_beginning ())
    {
      delta_time = 0;
      Mf_currcopytime = start_tick;	/* to avoid long gap at begining */
    }

  while (Mf_toberead > 0)
    {

      delta_time = readvarinum ();
      /* [SS] 2019-06-30 */
      delta_seconds = (double) (60000*delta_time)/(double) (current_tempo * division);
      currentseconds += delta_seconds;
      Mf_currtime += delta_time;
      if (cut_ending ())
	{
	  flag_metaeot = 1;
	  /*   delta_time += end_tick - Mf_currtime; */
	  /*    Mf_currtime = end_tick; [SS] 2005-08-17 */
	  break;
	}

      c = egetc ();

      if (sysexcontinue && c != 0xf7)
	mferror ("didn't find expected continuation of a sysex");

/* if bit 7 not set, there is no status byte following the
*  delta time, so it must a running status and we assume the
*  last status occuring in the preceding channel message.   */
      if ((c & 0x80) == 0)
	{			/* running status? */
	  if (status == 0)
	    mferror ("unexpected running status");
	  running = 1;
	}
      else
	{
	  if (c >> 4 != 0x0f)
	    {			/* [SS] 2013-09-10 */
	      laststatus = c;
	    }
	  status = c;
	  running = 0;
	}

      /* [SS] 2013-09-10 */
      if (running)
	needed = chantype[(laststatus >> 4) & 0xf];
      else
	needed = chantype[(status >> 4) & 0xf];

      if (needed)
	{			/* ie. is it a channel message? */

	  /*if (Mf_currtime > Mf_currcopytime) accumulating = 1; */

	  /* [SS] 2013-09-10 */
/* chanmessages gathers all the channel messages which
   includes note on/off, program changes, bends etc.
*/
	  if (running)
	    {
	      c1 = c;
	      chanmessage (laststatus, c1, (needed > 1) ? egetc () : 0);
	    }
	  else
	    {
	      c1 = egetc ();
	      chanmessage (status, c1, (needed > 1) ? egetc () : 0);
	    }
	  continue;;
	}

      switch (c)
	{

	case 0xff:		/* meta event */

	  type = egetc ();
	  varinum = readvarinum ();
	  lookfor = Mf_toberead - varinum;
	  msginit ();

	  while (Mf_toberead > lookfor)
	    msgadd (egetc ());

	  metaevent (type);
	  break;

	case 0xf0:		/* start of system exclusive */

	  varinum = readvarinum ();
	  lookfor = Mf_toberead - varinum;
	  msginit ();
	  msgadd (0xf0);

	  while (Mf_toberead > lookfor)
	    msgadd (c = egetc ());

	  if (c == 0xf7 || Mf_nomerge == 0)
	    sysex ();
	  else
	    sysexcontinue = 1;	/* merge into next msg */
	  break;

	case 0xf7:		/* sysex continuation or arbitrary stuff */

	  varinum = readvarinum ();
	  lookfor = Mf_toberead - varinum;

	  if (!sysexcontinue)
	    msginit ();

	  while (Mf_toberead > lookfor)
	    msgadd (c = egetc ());

	  if (!sysexcontinue)
	    {
	      if (Mf_arbitrary)
		(*Mf_arbitrary) (msgleng (), msg ());
	    }
	  else if (c == 0xf7)
	    {
	      sysex ();
	      sysexcontinue = 0;
	    }
	  break;
	default:
	  badbyte (c);
	  break;
	}
    }
  if (max_currtime < Mf_currtime)
    max_currtime = Mf_currtime;
  return max_currtime;
}


int
get_tempo_info_from_track_1 ()
{
/* most of this code has been grabbed from readtrack().
   The function is called to extract all the meta tempo
   commands from track-1 and to store this in an array.
*/

  /* This array is indexed by the high half of a status byte.  It's */
  /* value is either the number of bytes needed (1 or 2) for a channel */
  /* message, or 0 (meaning it's not  a channel message). */
  static int chantype[] = {
    0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 through 0x70 */
    2, 2, 2, 2, 1, 1, 2, 0	/* 0x80 through 0xf0 */
  };
  long lookfor;
  int c, c1, type;
  int sysexcontinue = 0;	/* 1 if last message was an unfinished sysex */
  int running = 0;		/* 1 when running status used */
  int status = 0;		/* status value (e.g. 0x90==note-on) */
  int laststatus;		/* [SS] 2013-09-10 */
  int needed;
  long varinum;
  int position;
  char *m;

  laststatus = 0;
  position = ftell (F_in);
  /*printf("position = %d\n",position); */

  if (readmt ("MTrk") == EOF)
    return (0);

  Mf_toberead = read32bit ();
  Mf_currtime = 0;

  /*alloc_trackdata(); */


  while (Mf_toberead > 0)
    {

      delta_time = readvarinum ();
      Mf_currtime += delta_time;

      c = egetc ();

      if (sysexcontinue && c != 0xf7)
	mferror ("didn't find expected continuation of a sysex");

/* if bit 7 not set, there is no status byte following the
*  delta time, so it must a running status and we assume the
*  last status occuring in the preceding channel message.   */
      if ((c & 0x80) == 0)
	{			/* running status? */
	  if (status == 0)
	    mferror ("unexpected running status");
	  running = 1;
	}
      else
	{
/* if it is not a meta event, save the status byte */
	  if (c >> 4 != 0x0f)
	    {
	      laststatus = c;	/* [SS] 2013-09-10 */
	    }
	  status = c;
	  running = 0;
	}

      /* [SS] 2011-09-10 */
      if (running)
	needed = chantype[(laststatus >> 4) & 0xf];
      else
	needed = chantype[(status >> 4) & 0xf];

      if (needed)
	{			/* ie. is it a channel message? */


	  if (running)
	    c1 = c;
	  else
	    c1 = egetc ();
	  /*chanmessage(status, c1, (needed > 1) ? egetc() : 0); */
	  continue;;
	}

      switch (c)
	{

	case 0xff:		/* meta event */

	  type = egetc ();
	  varinum = readvarinum ();
	  lookfor = Mf_toberead - varinum;
	  msginit ();

	  while (Mf_toberead > lookfor)
	    msgadd (egetc ());

/*	    metaevent(type); */
	  if (type == 0x51)
	    {
	      m = msg ();
	      mf_get_tempo_event (to32bit (0, m[0], m[1], m[2]));
	    }
	  break;

	case 0xf0:		/* start of system exclusive */

	  varinum = readvarinum ();
	  lookfor = Mf_toberead - varinum;
	  msginit ();
	  msgadd (0xf0);

	  while (Mf_toberead > lookfor)
	    msgadd (c = egetc ());

	  if (c == 0xf7 || Mf_nomerge == 0)
	    /*sysex();[SS] 2006-08-08 */
	    break;		/* [SS] 2006-08-08 */
	  else
	    sysexcontinue = 1;	/* merge into next msg */
	  break;

	case 0xf7:		/* sysex continuation or arbitrary stuff */

	  varinum = readvarinum ();
	  lookfor = Mf_toberead - varinum;

	  if (!sysexcontinue)
	    msginit ();

	  while (Mf_toberead > lookfor)
	    msgadd (c = egetc ());

	  if (!sysexcontinue)
	    {
	      if (Mf_arbitrary)
		(*Mf_arbitrary) (msgleng (), msg ());
	    }
	  else if (c == 0xf7)
	    {
/*		sysex(); */
	      sysexcontinue = 0;
	    }
	  break;
	default:
          /* since we are only interested in tempo meta commands, */
          /* we do not care if we miss a byte.                    */
	  /*badbyte (c); [SS] 2017-09-18                          */
          /*printf("ignoring byte :0x%02x\n",c);  [SS] 2017-09-18 */
	  break;
	}
    }
  fseek (F_in, position, SEEK_SET);
  return (0);
}



void
badbyte (c)
     int c;
{
  char buff[32];

  (void) sprintf (buff, "unexpected byte: 0x%02x", c);
  mferror (buff);
}




void
metaevent (int type)
{
  int leng;
  unsigned char *m;
  long qnote;			/* [SS] 2013-09-06 */

  leng = msgleng ();
  m = msg ();
  switch (type)
    {
    case 0x00:
      /* 2015-11-14 [SS] */
      copy_metaseqnum (m[0], m[1]);
      break;
    case 0x01:			/* Text event */
    case 0x02:			/* Copyright notice */
    case 0x03:			/* Sequence/Track name */
    case 0x04:			/* Instrument name */
    case 0x05:			/* Lyric */
    case 0x06:			/* Marker */
    case 0x07:			/* Cue point */
    case 0x08:
    case 0x09:
    case 0x0a:
    case 0x0b:
    case 0x0c:
    case 0x0d:
    case 0x0e:
    case 0x0f:
      /* These are all text events */
      copy_metatext (type, leng, m);
      break;
    case 0x2f:			/* End of Track */
      copy_metaeot ();
      break;
    case 0x51:			/* Set tempo */
      hastempo[activetrack] = 1;  /* [SS] 2017-12-14 */
      if (newtempo > 0)		/* [SS] 2013-09-04 */
	mf_write_tempo (newtempo);
      else if (newspeed)
	{
	  qnote = m[2] + m[1] * 256 + m[0] * 65536;
	  qnote = (int) ((float) qnote / speedfactor);
	  mf_write_tempo (qnote);
	}
      else
	mf_write_tempo (to32bit (0, m[0], m[1], m[2]));
      break;
    case 0x54:
      mf_write_meta_event (0x54, m, 5);
      break;
    case 0x58:
      midicopy_timesig (m[0], m[1], m[2], m[3]);
      break;
    case 0x59:
      copy_keysig (m[0], m[1]);
      break;
    case 0x7f:
      copy_metaspecial (leng, m);
      break;
    default:
      copy_metamisc (type, leng, m);
    }
}





void
sysex ()
{
  copy_sysex (msgleng (), msg ());
}




void
chanmessage (int status, int c1, int c2)
{
  int chan = status & 0xf;
  if (zerochannels) chan = 0; /* [SS] 2020-10-09 */
  haschannel[chan] = 1;

  if (!cut_beginning ())
    nochanmsg = 0;

/* only process the channels that we want */
  if (ctocopy[chan]) {
   if (drmflag == 0 || dtocopy[c1] == 1 && chan == 9 || chan != 9)  
	   /* [SS] 2019-12-25 */
    switch (status & 0xf0)
      {
      case 0x80:
	copy_noteoff (chan, c1, c2);
	break;
      case 0x90:
	if (chan == 9 && selected_drum != 0 && c1 != selected_drum)
	  c2 = drumlevel;	/* [SS] 2013-09-07 */
	if (chan != 9 && mutenodrum)
          if (c2 > nondrumlevel) 
	     c2 = nondrumlevel;	/*[SS] 2013-09-15 */
        if (chan == 9 && chosen_drum != 0 && c1 == chosen_drum)
             c2 = drumvelocity; /* [SS] 2013-10-01 */
        if (cfocus[chan] == 1) {   /* [SS] 2017-11-27 */
             c2 = c2 - attenuation;
             if (c2 <0) c2 = 0;
             }
        if (tfocus[activetrack] == 1) {   /* [SS] 2017-11-27 */
             c2 = c2 - attenuation;
             if (c2 <0) c2 = 0;
             }
	copy_noteon (chan, c1, c2);
	break;
      case 0xa0:
        if (nopressure == 0) copy_pressure (chan, c1, c2);
	break;
      case 0xb0:
        if (nocntrl == 0)
	copy_parameter (chan, c1, c2);
	break;
      case 0xe0:
        if (nobends == 0)
	  copy_pitchbend (chan, c1, c2);
	break;
      case 0xc0:
	copy_program (chan, c1);
	break;
      case 0xd0:
        if (nopressure == 0) copy_chanpressure (chan, c1);
	break;
      }
   }
}



/* readvarinum - read a varying-length number, and return the */
/* number of characters it took. */
long
readvarinum ()
{
  long value;
  int c;

  c = egetc ();
  value = c;
  if (c & 0x80)
    {
      value &= 0x7f;
      do
	{
	  c = egetc ();
	  value = (value << 7) + (c & 0x7f);
	}
      while (c & 0x80);
    }
  return (value);
}



long
to32bit (c1, c2, c3, c4)
     int c1, c2, c3, c4;
{
  long value = 0L;

  value = (c1 & 0xff);
  value = (value << 8) + (c2 & 0xff);
  value = (value << 8) + (c3 & 0xff);
  value = (value << 8) + (c4 & 0xff);
  return (value);
}




int
to16bit (c1, c2)
     int c1, c2;
{
  return ((c1 & 0xff) << 8) + (c2 & 0xff);
}



long
read32bit ()
{
  int c1, c2, c3, c4;

  c1 = egetc ();
  c2 = egetc ();
  c3 = egetc ();
  c4 = egetc ();
  return to32bit (c1, c2, c3, c4);
}



int
read16bit ()
{
  int c1, c2;
  c1 = egetc ();
  c2 = egetc ();
  return to16bit (c1, c2);
}



void
mferror (char *s)
{
  fprintf (stderr, "Error: %s\n", s);
  exit (1);
}




/* The code below allows collection of a system exclusive message of */
/* arbitrary length.  The Msgbuff is expanded as necessary.  The only */
/* visible data/routines are msginit(), msgadd(), msg(), msgleng(). */

#define MSGINCREMENT 128
static char *Msgbuff = NULL;	/* message buffer */
static int Msgsize = 0;		/* Size of currently allocated Msg */
static int Msgindex = 0;	/* index of next available location in Msg */

void
msginit ()
{
  Msgindex = 0;
}


char *
msg ()
{
  return (Msgbuff);
}


int
msgleng ()
{
  return (Msgindex);
}


void
msgadd (int c)
{
  /* If necessary, allocate larger message buffer. */
  if (Msgindex >= Msgsize)
    biggermsg ();
  Msgbuff[Msgindex++] = c;
}


void
biggermsg ()
{
/*   char *malloc(); */
  char *newmess;
  char *oldmess = Msgbuff;
  int oldleng = Msgsize;

  Msgsize += MSGINCREMENT;
  newmess = (char *) malloc ((unsigned) (sizeof (char) * Msgsize));

  if (newmess == NULL)
    mferror ("malloc error!");

  /* copy old message into larger new one */
  if (oldmess != NULL)
    {
      register char *p = newmess;
      register char *q = oldmess;
      register char *endq = &oldmess[oldleng];

      for (; q != endq; p++, q++)
	*p = *q;
      free (oldmess);
    }
  Msgbuff = newmess;
}



void list_channels_in_use (int n) {
int i;
printf("trk = %d  ",n);
for (i=0;i<16;i++) printf(" %d",haschannel[i]);
printf("\n");
}




/*
 * mfwrite() - The only fuction you'll need to call to write out
 *             a midi file.
 *
 * format      0 - Single multi-channel track
 *             1 - Multiple simultaneous tracks
 *             2 - One or more sequentially independent
 *                 single track patterns                
 * ntracks     The number of tracks in the file.
 * division    This is kind of tricky, it can represent two
 *             things, depending on whether it is positive or negative
 *             (bit 15 set or not).  If  bit  15  of division  is zero,
 *             bits 14 through 0 represent the number of delta-time
 *             "ticks" which make up a quarter note.  If bit  15 of
 *             division  is  a one, delta-times in a file correspond to
 *             subdivisions of a second similiar to  SMPTE  and  MIDI
 *             time code.  In  this format bits 14 through 8 contain
 *             one of four values - 24, -25, -29, or -30,
 *             corresponding  to  the  four standard  SMPTE and MIDI
 *             time code frame per second formats, where  -29
 *             represents  30  drop  frame.   The  second  byte
 *             consisting  of  bits 7 through 0 corresponds the the
 *             resolution within a frame.  Refer the Standard MIDI
 *             Files 1.0 spec for more details.
 * fp          This should be the open file pointer to the file you
 *             want to write.  
 */

void
build_new_midi_file (format, ntracks, division, fp)
     int format, ntracks, division;
     FILE *fp;
{
  int i,j;
  void mf_write_track_chunk ();
  float track_time;

  seconds_output = 0.0;

  get_tempo_info_from_track_1 ();

  if (ntracks > 149) {printf("too many tracks\n"); exit(1); }

  /* The rest of the file is a series of tracks */
  for (i = 0; i < ntracks; i++)
    {
      activetrack = i;
      nochanmsg = 1;
      if (verbatim)		/* 2013-09-15 */
	copytrack_verbatim ();	/*not necessary to read in detail */
      else
	{
	  flag_metaeot = 0;
	  init_notechan ();
	  track_time = (float) readtrack ();
	  if (track_time > seconds_output)
	    seconds_output = track_time;
	  turn_off_all_playing_notes ();
/*
 * test
	  if (i > 1 && nochanmsg)
	    winamp_compatibility_measure ();
*/
          
	  if (flag_metaeot)
	    copy_metaeot ();	/*need end of track message */
	  ignore_rest_of_track ();
	}
      if (tocopy[i] == 1)
        /*list_channels_in_use(i); */
        /* [SS] 2017-10-19] don't copy redundant tracks */
        /* [SS] 2017-12-24 but make sure tracks with tempo is copied */
        for (j=0;j<16;j++) {
           if((ctocopy[j] == 1  && haschannel[j] == 1) || hastempo[i]) {
              /* printf("writing track %d\n",i); */
	      mf_write_track_chunk (i, fp);
              break;  /* only write the track once */
             }
           }
    }
/* printf("%d tracks saved\n",trkid); */
}


void
winamp_compatibility_measure ()
{
/* for some reason Winamp on Windows refuses to play short
 * MIDI files where one of the tracks is very
 * short in time. The problem files were found
 * on the site http://www.jsbchorales.net/
 * An example of such a file is 000603b.
 * This command expands this track by putting
 * a channel noteon/noteoff command. We use channel 15,
 * MIDI pitch 0 and  midi volume 0 so that it has least effect.
 * We only check for tracks with no channel messages.
 *
 */
  writechanmsg_at_0 ();
}

void
writechanmsg_at_0 ()
{
  char c, data[2];
  int delta;
  delta = end_tick - start_tick;
  if (delta < 0)
    delta = 1000;
  data[0] = (char) 0;		/* pitch zero */
  data[1] = (char) 1;		/* volume 1 so not confused with noteoff */
                                /* [SS] 2020-01-05 */
  c = 0x9f;			/* MIDI ON channel 15 */
  WriteVarLen (0);
  eputc (c);
  eputc (data[0]);
  eputc (data[1]);
  c = 0x8f;			/* MIDI off channel 15 */
  data[1] = (char) 0;
  WriteVarLen (delta);
  eputc (c);
  eputc (data[0]);
  eputc (data[1]);
}



void
replace_byte_in_file (int trknum, int loc, char val, FILE * fp, int ntracks)
{
  int i;
  for (i = 0; i < ntracks; i++)
    {
      copytrack_verbatim ();
      if (i == trknum)
	replace_byte_in_trackdata (loc, val);
      mf_write_track_chunk (i, fp);
    }
}




void
mf_write_track_chunk (which_track, fp)
     int which_track;
     FILE *fp;
/* We may not know how many tracks that we will output, so
 * each track is transfered to a separate string in the
 * array trackstr[]. At the end these strings will be
 * recorded in the output file.
*/
{
  long trkhdr, trklength;
  void write16bit (), write32bit ();
  void WriteVarLen ();


  trkhdr = MTrk;
  trklength = trackdata_length;
  /*ier = fwrite (trackdata, 1, trackdata_length, fp); */
  trkid++;
  trackstr[trkid] = (char *) malloc(trklength+2);
  /* trackstr[trkid] = strcpy(trackstr[trkid],trackdata); */
  memcpy(trackstr[trkid],trackdata,trklength);
  trackstr_length[trkid] = trklength;
/*  printf("  %d bytes written\n",ier); */
}



void
mf_write_header_chunk (format, ntracks, division)
     int format, ntracks, division;
{
  long ident, length;
  void write16bit (), write32bit ();

  ident = MThd;			/* Head chunk identifier                    */
  length = 6;			/* Chunk length                             */

  /* individual bytes of the header must be written separately
     to preserve byte order across cpu types :-( */
  write32bit (ident);
  write32bit (length);
  write16bit (format);
  write16bit (ntracks);
  write16bit (division);
}				/* end gen_header_chunk() */





/*
 * mf_write_midi_event()
 * 
 * Library routine to mf_write a single MIDI track event in the standard MIDI
 * file format. The format is:
 *
 *                    <delta-time><event>
 *
 * In this case, event can be any multi-byte midi message, such as
 * "note on", "note off", etc.      
 *
 * delta_time - the time in ticks since the last event.
 * type - the type of meta event.
 * chan - The midi channel.
 * data - A pointer to a block of chars containing the META EVENT,
 *        data.
 * size - The length of the meta-event data.
 */
int
mf_write_midi_event (int type, int chan, char *data, int size)
{
  int i;
  void WriteVarLen ();
  char c;

  WriteVarLen (Mf_currtime - Mf_currcopytime);
  if (!cut_beginning ())
    Mf_currcopytime = Mf_currtime;
  /* all MIDI events start with the type in the first four bits,
     and the channel in the lower four bits */
  c = (char) (type | chan);

  if (chan > 15)
    {
      mferror ("error: MIDI channel greater than 16");
    };

  eputc (c);

  /* write out the data bytes */
  for (i = 0; i < size; i++)
    eputc (data[i]);

  return (size);
}				/* end mf_write MIDI event */





/*
 * mf_write_meta_event()
 *
 * Library routine to mf_write a single meta event in the standard MIDI
 * file format. The format of a meta event is:
 *
 *          <delta-time><FF><type><length><bytes>
 *
 * delta_time - the time in ticks since the last event.
 * type - the type of meta event.
 * data - A pointer to a block of chars containing the META EVENT,
 *        data.
 * size - The length of the meta-event data.
 */
int
mf_write_meta_event (int type, char *data, int size)
{
  int i;
  void WriteVarLen ();

  WriteVarLen (Mf_currtime - Mf_currcopytime);
  if (!cut_beginning ())
    Mf_currcopytime = Mf_currtime;

  /* This marks the fact we're writing a meta-event */
  eputc ((char) meta_event);

  /* The type of meta event */
  eputc ((char) type);

  /* The length of the data bytes to follow */
  WriteVarLen ((long) size);

  for (i = 0; i < size; i++)
    {
/*  if(eputc((data[i] & 0xff)) != (data[i] & 0xff)) */
/*      return(-1);                                 */
      eputc (data[i]);
    }
  return (size);
}				/* end mf_write_meta_event */



void
mf_get_tempo_event (tempo)
     long tempo;
{
  tempo_array[temposize].seconds
    = (float) (Mf_currtime - tempo_array[temposize - 1].tick) *
    (float) current_tempo
    / (division * 1000000.0f) + tempo_array[temposize - 1].seconds;
  tempo_array[temposize].tick = Mf_currtime;
  tempo_array[temposize].tempo = tempo;
  current_tempo = tempo;
  if (temposize < 1999)
    temposize++;
}


void
mf_write_tempo (tempo)
     long tempo;
{
  /* Write tempo */
  /* all tempos are written as 120 beats/minute, */
  /* expressed in microseconds/quarter note     */

  void WriteVarLen ();

  if (Mf_currtime - Mf_currcopytime < 0)
    eputc (0);
  else
    {
      WriteVarLen (Mf_currtime - Mf_currcopytime);
      Mf_currcopytime = Mf_currtime;
    }

  eputc ((char) meta_event);
  eputc (set_tempo);

  eputc (3);
  eputc ((char) (0xff & (tempo >> 16)));
  eputc ((char) (0xff & (tempo >> 8)));
  eputc ((char) (0xff & tempo));

/* fudge to avoid large time gap at beginning */
/* It assumes a constant tempo. */
  if (use_seconds && start_tick == -1 && start_seconds > 0) /* [SS] 2019-06-29*/
    {
      start_tick = (int) (start_seconds * 1000000.0 * division / tempo);
      Mf_currcopytime = start_tick;
    }
}









/*
 * Write multi-length bytes to MIDI format files
 */
void
WriteVarLen (value)
     long value;
{
  long buffer;

  if (value < 0)
    value = 0;			/* [SS] 2013-05-10 */

  buffer = value & 0x7f;
  while ((value >>= 7) > 0)
    {
      buffer <<= 8;
      buffer |= 0x80;
      buffer += (value & 0x7f);
    }
  while (1)
    {
      eputc ((char) (buffer & 0xff));

      if (buffer & 0x80)
	buffer >>= 8;
      else
	return;
    }
}				/* end of WriteVarLen */








/*
 * write32bit()
 * write16bit()
 *
 * These routines are used to make sure that the byte order of
 * the various data types remains constant between machines. This
 * helps make sure that the code will be portable from one system
 * to the next.  It is slightly dangerous that it assumes that longs
 * have at least 32 bits and ints have at least 16 bits, but this
 * has been true at least on PCs, UNIX machines, and Macintosh's.
 *
 */
void
write32bit (data)
     long data;
{
  putc ((char) ((data >> 24) & 0xff), fp);
  putc ((char) ((data >> 16) & 0xff), fp);
  putc ((char) ((data >> 8) & 0xff), fp);
  putc ((char) (data & 0xff), fp);
}

void
write16bit (data)
     int data;
{
  putc ((char) ((data & 0xff00) >> 8), fp);
  putc ((char) (data & 0xff), fp);
}





/* write a single character and abort on error */
int
eputc (char c)
{

  append_to_string (c);
  Mf_numbyteswritten++;
  return (0);
}


int
seconds_to_tick (float seconds)
{
  int i, ind=0; /* [SDG] 2020-06-02 */
  float tick, fraction;
  for (i = 0; i < temposize; i++)
    {
      ind = i;
      if (seconds < tempo_array[ind].seconds)
	break;
    }

  if (seconds < tempo_array[ind].seconds)
    {
      fraction = (seconds - tempo_array[ind - 1].seconds)
	/ (tempo_array[ind].seconds - tempo_array[ind - 1].seconds);
      tick = tempo_array[ind - 1].tick + fraction * (tempo_array[ind].tick -
						     tempo_array[ind -
								 1].tick);
      return (int) tick;
    }

  tick = tempo_array[ind].tick +
    (seconds - tempo_array[ind].seconds) * division * 1000000.0f /
    tempo_array[ind].tempo;
  return (int) tick;
}


float
tick_to_seconds (int tick)
{
  int i, ind;
  float seconds;
  float delta;
  long tempo;
  for (i = 0; i < temposize; i++)
    {
      ind = i;
      if (tick < tempo_array[ind].tick)
	break;
    }
  if (tick < tempo_array[ind].tick && ind == 0)
    return 0.0;
  if (tick < tempo_array[ind].tick)
    {
      tempo = tempo_array[ind - 1].tempo;	/* [SS] 2013-09-08 and etc. */
      if (newtempo)
	tempo = newtempo;
      if (newspeed)
	tempo = (int) ((float) tempo / speedfactor);
      delta = (float) (tick - tempo_array[ind - 1].tick) *
	(float) tempo / ((float) (division * 1000000.0));
      seconds = tempo_array[ind - 1].seconds + delta;
      return seconds;
    }
  tempo = tempo_array[ind].tempo;	/* [SS] 2013-09-08 and etc. */
  if (newtempo)
    tempo = newtempo;
  if (newspeed)
    tempo = (int) ((float) tempo / speedfactor);
  delta = (float) (tick - tempo_array[ind].tick) *
    (float) tempo / ((float) (division * 1000000.0));
  seconds = tempo_array[ind].seconds + delta;
  return seconds;
}





/*  midicopy.c

 program to modify and copy a selected portion of
   a midi file.

*/


FILE *fp;
FILE *F_in;
extern int format, ntrks, division;

int
getarg (char *option, int argc, char *argv[])
/* look for argument 'option' in command line */
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





int
main (int argc, char *argv[])
{
  int arg;
  int trk[16], mtrks;
  int chn[16], chns;
  int drm[20];  /* 2019-12-22 */
  int xtrks; /* [SS] 2013-10-28 */
  int xchns; /* [SS] 2017-12-06 */
  int xdrms; /* [SS] 2019-12-22 */
  int i;
  int byteloc, trknum;
  int repflag;
  char val;
  float start_beat, end_beat;
  int beats_per_minute = 0;	/* [SS] 2013-09-04 */
  long trkhdr; /* [SS] 2017-10-20 */

  for (i = 0; i < 32; i++)
    tocopy[i] = 1;
  for (i = 0; i < 16; i++)
    ctocopy[i] = 1;
  for (i=0;i<32;i++) tfocus[i] = 0;  /* [SS] 2017-11-27 */
  for (i=0;i<16;i++) cfocus[i] = 0;  /* [SS] 2017-11-27 */
  for (i=0;i<32;i++) hastempo[i] = 0; /* [SS] 2017-12-14 */
  mtrks = 0;
  xtrks = 0;
  chns = 0;
  xchns = 0;
  start_tick = -1;
  end_tick = -1;
  start_seconds = -1.0;   /* [SS] 2019-06-30 */
  end_seconds = -1.0; /* [SS] 2019-06-30 */
  use_seconds = 0;
  use_beats = 0;
  use_ticks = 0;

  if (getarg ("-ver", argc, argv) >= 0)
    {
      printf ("%s\n", VERSION);
      exit (0);
    }

  arg = getarg ("-h", argc, argv);
  if (arg > 0 || argc < 3)
    {
      printf ("midicopy version %s\n\n", VERSION);
      printf ("midicopy copies selected tracks, channels, time \n");
      printf ("interval of the input midi file to the output file.\n\n");
      printf ("usage:\nmidicopy <options> input.mid output.mid\n\n");
      printf ("options:\n");
      printf ("-ver  version information\n");
      printf ("-trks n1,n2,..(starting from 1)\n");
      printf ("-xtrks n1,n2,.. (tracks to exclude)\n"); /* [SS] 2013-10-27 */
      printf ("-xchns n1,n2,.. (channels to exclude)\n"); /* [SS] 2022-11-12 */
      printf ("-chns n1,n2,..(starting from 1)\n");
      printf ("-from n (in midi ticks)\n");
      printf ("-to n   (in midi ticks)\n");
      printf ("-fromsec %%f (in seconds)\n");
      printf ("-tosec %%f (in seconds)\n");
      printf ("-frombeat %%f\n");
      printf ("-tobeat %%f\n");
      printf ("-replace trk,loc,val\n");
      printf ("-tempo n (in quarter notes/min)\n");	/* [SS] 2013-09-04 */
      printf ("-speed %%f (between 0.1 and 10.0)\n");	/* [SS] 2013-09-06 */
      printf ("-drumfocus n (35 - 81) m (0 - 127)\n");	/* [SS] 2013-09-07 */
      printf ("-mutenodrum [level] \n");	/* [SS] 2013-09-15 */
      printf ("-setdrumloudness n (35-81) m (0 -127)\n"); /* [SS] 2013-10-01 */
      printf ("-focusontracks n1,n2,... \n"); /* [SS] 2017-11-27 */
      printf ("-focusonchannels n1,n2,...\n"); /* [SS] 2017-11-27 */
      printf ("-attenuation n\n"); /* [SS] 2017-11-27 */
      printf ("-nobends\n"); /* [SS] 2017-11-27 */
      printf ("-nopressure\n"); /* [SS] 2022-05-05 */
      printf ("-nocntrl\n"); /* [SS] 2022-05-05 */
      printf ("-indrums n1,n2,... (drums to include)\n"); /* [SS] 2019-12-22 */
      printf ("-xdrums n1,n2,... (drums to exclude)\n"); /* [SS] 2019-12-22 */
      printf ("-onlydrums (only channel 10)\n"); /* [SS] 2019-12-22 */
      printf ("-nodrums (exclude channel 10)\n"); /* [SS] 2019-12-22 */
      printf ("-zerochannels  set all channel numbers to zero\n"); /* [SS] 2020-10-09 */
      exit (1);
    }

/* if -replace option is chosen then byte loc in track trk is replaced
   with the value val. This is used for changing the program assignment
   in a midi file. If -replace option is chosen, all other options are
   ignored.
*/
  arg = getarg ("-trks", argc, argv);
  if (arg >= 0)
    mtrks = sscanf (argv[arg], "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
		    &trk[0], &trk[1], &trk[2], &trk[3], &trk[4],
		    &trk[5], &trk[6], &trk[7], &trk[8], &trk[9], &trk[10],
		    &trk[11],&trk[12],&trk[13]);
  if (mtrks > 0)
    {
      /* printf("%d tracks specified\n", mtrks); */
      for (i = 0; i < 32; i++)
	tocopy[i] = 0;
      for (i = 0; i < mtrks; i++)
	if (trk[i] < 32 && trk[i] >= 0)
	  tocopy[trk[i] - 1] = 1;
    }

  arg = getarg ("-xtrks", argc, argv); /* [SS] 2013-10-27 */
  if (arg >= 0)
    xtrks = sscanf (argv[arg], "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
		    &trk[0], &trk[1], &trk[2], &trk[3], &trk[4],
		    &trk[5], &trk[6], &trk[7], &trk[8], &trk[9], &trk[10],
		    &trk[11]);
  if (xtrks > 0)
    {
      /* printf("%d tracks specified\n", mtrks); */
      for (i = 0; i < 32; i++)
	tocopy[i] = 1;
      for (i = 0; i < xtrks; i++)
	if (trk[i] < 32 && trk[i] > 0)
	  tocopy[trk[i] - 1] = 0;
    }

  arg = getarg("-xchns",argc,argv); /* [SS] 2017-12-06 */
  if (arg >= 0)
    xchns = sscanf (argv[arg], "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
		   &chn[0], &chn[1], &chn[2], &chn[3], &chn[4], &chn[5],
		   &chn[6], &chn[7], &chn[8], &chn[9], &chn[10], &chn[11],
                   &chn[12],&chn[13]);
  if (xchns > 0)
    {
      for (i = 0; i < 16; i++)
	ctocopy[i] = 1;
      for (i = 0; i < xchns; i++)
	if (chn[i] > 0 && chn[i] < 17)
	  ctocopy[chn[i] - 1] = 0;
    }

  arg = getarg ("-chns", argc, argv);
  if (arg >= 0)
    chns = sscanf (argv[arg], "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
		   &chn[0], &chn[1], &chn[2], &chn[3], &chn[4], &chn[5],
		   &chn[6], &chn[7], &chn[8], &chn[9], &chn[10], &chn[11],
                   &chn[12],&chn[13]);
  if (chns > 0)
    {
      for (i = 0; i < 16; i++)
	ctocopy[i] = 0;
      for (i = 0; i < chns; i++)
	if (chn[i] > 0 && chn[i] < 17)
	  ctocopy[chn[i] - 1] = 1;
    }

  arg = getarg ("-from", argc, argv);
  if (arg >= 0)
    {
      sscanf (argv[arg], "%d", &start_tick);
      use_seconds = 0; /* [SS] 2019-06-29 */
      use_beats = 0;
      use_seconds = 0;
      use_ticks = 1; /* [SS] 2019-06-30 */
    }

  arg = getarg ("-to", argc, argv);
  if (arg >= 0)
    {
      sscanf (argv[arg], "%d", &end_tick);
      use_seconds = 0; /* [SS] 2019-06-29 */
      use_beats = 0;
      use_ticks = 1; /* [SS] 2019-06-30 */
    }

  arg = getarg ("-fromsec", argc, argv);
  if (arg >= 0)
    {
      sscanf (argv[arg], "%f", &start_seconds);
      use_seconds = 1;
      use_ticks = 0;
      use_beats = 0;
    }
  arg = getarg ("-tosec", argc, argv);
  if (arg >= 0)
    {
      sscanf (argv[arg], "%f", &end_seconds);
      use_seconds = 1;
      use_ticks = 0; /* [SS] 2019-06-30 */
      use_beats = 0;
    }

  arg = getarg ("-frombeat", argc, argv);
  if (arg >= 0)
    {
      sscanf (argv[arg], "%f", &start_beat);
      use_beats = 1;
      use_seconds = 0; /* [SS] 2019-06-29 */
    }

  arg = getarg ("-tobeat", argc, argv);
  if (arg >= 0)
    {
      sscanf (argv[arg], "%f", &end_beat);
      use_beats = 1;
      use_seconds = 0; /* [SS] 2019-06-29 */
    }

/* [SS] 2013-09-04 */
  arg = getarg ("-tempo", argc, argv);
  if (arg >= 0)
    {
      sscanf (argv[arg], "%d", &beats_per_minute);
      if (beats_per_minute > 0)
	{
	  newtempo = 60 * 1000000 / beats_per_minute;
	}
    }

/* [SS] 2013-09-06 */
  arg = getarg ("-speed", argc, argv);
  if (arg >= 0)
    {
      sscanf (argv[arg], "%f", &speedfactor);
      if (speedfactor > 10.0)
	speedfactor = 10.0;
      if (speedfactor < 0.05)
	speedfactor = 0.05;
      newspeed = 1;
    }

/* [SS] 2013-09-07 */
  arg = getarg ("-drumfocus", argc, argv);
  if (arg >= 0)
    {
      sscanf (argv[arg], "%d", &selected_drum);
      if (selected_drum < 35 || selected_drum > 81)
	{
	  printf ("drumfocus selected_drum must be between 35 and 81\n");
	  selected_drum = 0;
	}
      sscanf (argv[arg + 1], "%d", &drumlevel);
      if (drumlevel < 0 || drumlevel > 127)
	{
	  printf
	    ("drumfocus drumlevel (2nd arg) must be between 0 and 127\n");
	  drumlevel = 20;
	}
    }

/* [SS] 2013-09-15 */
  arg = getarg ("-mutenodrum", argc, argv);
  if (arg >= 0)
    {
      mutenodrum = 1;
      sscanf (argv[arg], "%d", &nondrumlevel);
    }

/* [SS] 2013-10-01 */
  arg = getarg ("-setdrumloudness", argc, argv);
  if (arg >= 0)
    {
      sscanf (argv[arg], "%d", &chosen_drum);
      if (chosen_drum < 35 || chosen_drum > 81)
	{
	  printf ("setdrumloudness chosen_drum must be between 35 and 81\n");
	  chosen_drum = 0;
	}
      sscanf (argv[arg + 1], "%d", &drumvelocity);
      if (drumvelocity < 0 || drumvelocity > 127)
	{
	  printf
	    ("setdrumloudness drumvelocity (2nd arg) must be between 0 and 127\n");
	  drumvelocity = 90;
	}
    }

/* [SS] 2017-11-27 */
  mtrks = 0;
  arg = getarg("-focusontracks",argc,argv);
  if (arg >= 0)
     {
     mtrks = sscanf (argv[arg], "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
		    &trk[0], &trk[1], &trk[2], &trk[3], &trk[4],
		    &trk[5], &trk[6], &trk[7], &trk[8], &trk[9], &trk[10],
		    &trk[11]);
     if (mtrks > 0)
     {
      /* printf("%d tracks specified\n", mtrks); */
      for (i = 0; i < 32; i++)
	tfocus[i] = 1; /* attenuation flag */
      for (i = 0; i < mtrks; i++) {
	if (trk[i] < 32 && trk[i] >= 0)
          tfocus[trk[i]-1] = 0; /* track numbers start from 1*/
        }
      }
    }

/* [SS] 2017-11-27 */
  arg = getarg("-focusonchannels",argc,argv);
  if (arg >=0) {
  chns = sscanf (argv[arg], "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
		   &chn[0], &chn[1], &chn[2], &chn[3], &chn[4], &chn[5],
		   &chn[6], &chn[7], &chn[8], &chn[9], &chn[10], &chn[11]);
  if (chns > 0)
    {
      for (i = 0; i < 16; i++)
	cfocus[i] = 1;  /* attenuation flag */
      for (i = 0; i < chns; i++)
	if (chn[i] > 0 && chn[i] < 17)
	  cfocus[chn[i] - 1] = 0; /* channel numbers start from 1 */
     }
  }

  arg = getarg("-attenuation",argc,argv);
  if (arg >= 0)
    sscanf (argv[arg], "%d", &attenuation);

  arg = getarg("-nobends",argc,argv);
  if (arg >=0) nobends=1;

  /* [SS] 2022-05-05 */
  arg = getarg("-nopressure",argc,argv);
  if (arg >=0) nopressure=1;

  arg = getarg("-nocntrl",argc,argv);
  if (arg >=0) nocntrl = 1;

  repflag = getarg ("-replace", argc, argv);
  if (repflag >= 0)
    sscanf (argv[repflag], "%d,%d,%c", &trknum, &byteloc, &val);

  arg = getarg ("-indrums", argc, argv); /* [SS] 2019-12-22 */
  if (arg >= 0) {
    xdrms = sscanf (argv[arg], "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
		    &drm[0], &drm[1], &drm[2], &drm[3], &drm[4],
		    &drm[5], &drm[6], &drm[7], &drm[8], &drm[9], &drm[10],
		    &drm[11], &drm[12], &drm[13], &drm[14], &drm[15],
		    &drm[16], &drm[17], &drm[18], &drm[19]);
  if (xdrms > 0)
    {
      for (i = 0; i< 82; i++) 
         dtocopy[i] = 0; /* percussion types to transfer */
      for (i = 0; i<xdrms; i++)
         if (drm[i] > 34 && drm[i] < 82)
            dtocopy[drm[i]] = 1;
    }
    drmflag = 1;
}

  arg = getarg ("-xdrums", argc, argv); /* [SS] 2019-12-22 */
  if (arg >= 0) {
    xdrms = sscanf (argv[arg], "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
		    &drm[0], &drm[1], &drm[2], &drm[3], &drm[4],
		    &drm[5], &drm[6], &drm[7], &drm[8], &drm[9], &drm[10],
		    &drm[11], &drm[12], &drm[13], &drm[14], &drm[15],
		    &drm[16], &drm[17], &drm[18], &drm[19]);
    if (xdrms > 0) {
      for (i = 0; i< 82; i++) 
         dtocopy[i] = 1; /* percussion types to transfer */
      for (i = 0; i<xdrms; i++)
         if (drm[i] > 34 && drm[i] < 82)
            dtocopy[drm[i]] = 0;
    }
    drmflag = 1;
  }

  arg = getarg("-onlydrums",argc,argv); /* [SS] 2019-12-22 */
  if (arg >=0) {
    for (i=0;i<16;i++) ctocopy[i] = 0;
    ctocopy[9] = 1;
    }

  arg = getarg("-nodrums",argc,argv); /* [SS] 2019-12-22 */
  if (arg >=0) ctocopy[9] = 0;     

  arg = getarg("-zerochannels",argc,argv); /* [SS] 2020-10-09 */
  if (arg >=0) zerochannels = 1;

  F_in = fopen (argv[argc - 2], "rb");
  if (F_in == NULL)
    {
      printf ("cannot open input file %s\n", argv[argc - 2]);
      exit (1);
    }
  fp = fopen (argv[argc - 1], "wb");
  if (fp == NULL)
    {
      printf ("cannot open out file %s\n", argv[argc - 1]);
      exit (2);
    }



  if (newspeed)
    newtempo = 0;		/* [SS] 2013-09-06 */



  readheader ();

  temposize = 0;               /* [SS] 2019-06-29 */
  tempo_array[temposize].tempo = current_tempo;
  tempo_array[temposize].tick = 0;
  tempo_array[temposize].seconds = 0.0;
  temposize++;

  if (use_beats) /* [SS] 2019-06-29 */
    {
      use_ticks = 1; /* [SS] 2019-06-30 */
      if (start_beat < 0.0)
	start_tick = -1;
      else
	start_tick = (int) (division * start_beat);
      if (end_beat < 0.0)
	end_tick = -1;
      else
	end_tick = (int) (division * end_beat);
    }

  else if (use_seconds) /* [SS] 2019-06-29 */
   {
   if (start_seconds >= 0.0)
     start_tick = seconds_to_tick (start_seconds);
   if (end_seconds >= 0.0)
     end_tick = seconds_to_tick (end_seconds);
   }

   else { /* start_tick and end_tick already given */
        }

  if (mtrks == 0)
    mtrks = ntrks;

  if (xtrks > 0)
    mtrks = ntrks - xtrks;

  /*mf_write_header_chunk (format, mtrks, division);*/

  if (repflag >= 0)
    replace_byte_in_file (trknum, byteloc, val, fp, mtrks);
  else
    build_new_midi_file (format, ntrks, division, fp);


/* output new midi file and free up allocated space 
 *[SS] 2017-10-20
 * Now we know the number of tracks (trkid). mv_write_header_chunk
 * writes directly to the output file.
 */ 
  mf_write_header_chunk (format, trkid, division);
  for (i=1;i<=trkid;i++) {
    /* write32bit writes directly to the file handle fp */
    trkhdr = MTrk;
    write32bit (trkhdr);
    write32bit (trackstr_length[i]);
    fwrite(trackstr[i],1,trackstr_length[i],fp);
    free(trackstr[i]);
    }
  free (trackdata);
  fclose (F_in);
  fclose (fp);


  /* if (end_tick < 0) [SS] 2019-06-29 */
  end_tick = max_currtime;
  start_seconds = tick_to_seconds (start_tick);
  end_seconds = tick_to_seconds (end_tick);
  seconds_output = end_seconds - start_seconds;
  printf ("%f\n", seconds_output);
  return (0);
}

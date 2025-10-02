/*
 * mftext
 * 
 * Convert a MIDI file to verbose text.
 *
 * modified for portability 22/6/98 - error checking on file open failure
 * removed.
 */

#include <stdio.h>
#include <stdlib.h>
#ifdef ANSILIBS
#include <ctype.h>
#endif
#include "midifile.h"

void initfuncs();
void midifile();
extern char* crack(int argc, char *argv[], char *flags, int ign);
extern FILE *efopen(char *name, char *mode);

static FILE *F;
int SECONDS;      /* global that tells whether to display seconds or ticks */
int division;        /* from the file header */
long tempo = 500000; /* the default tempo is 120 beats/minute */

int filegetc()
{
  return(getc(F));
}

/* for crack */
extern int arg_index;

int main(int argc, char *argv[])
{
  char *ch;

  SECONDS = 0;

  while((ch = crack(argc,argv,"s",0)) != NULL){
      switch(*ch){
    case 's' : SECONDS = 1; break;
    }
      }

  if ((argc < 2 && !SECONDS) || (argc < 3 && SECONDS))
      F = stdin;
  else
      F = efopen(argv[arg_index],"rb");

  initfuncs();
  Mf_getc = filegetc;
  midifile();
  fclose(F);
  exit(0);
}

FILE *
efopen(char *name, char *mode)
{
  FILE *f;

  if ( (f=fopen(name,mode)) == NULL ) {
    fprintf(stderr,"Error - Cannot open file %s\n",name);
    exit(0);
  }
  return(f);
}

void error(char *s)
{
  fprintf(stderr,"Error: %s\n",s);
}

void prtime()
{
    if(SECONDS)
  printf("Time=%f   ",mf_ticks2sec(Mf_currtime,division,tempo));
    else
  printf("Time=%ld  ",Mf_currtime);
}

void txt_header(int format, int ntrks, int ldivision)
{
        division = ldivision; 
  printf("Header format=%d ntrks=%d division=%d\n",format,ntrks,division);
}

void txt_trackstart()
{
  printf("Track start\n");
}

void txt_trackend()
{
  printf("Track end\n");
}

void txt_noteon(int chan, int pitch, int vol)
{
  prtime();
  printf("Note on, chan=%d pitch=%d vol=%d\n",chan+1,pitch,vol);
}

void txt_noteoff(int chan, int pitch, int vol)
{
  prtime();
  printf("Note off, chan=%d pitch=%d vol=%d\n",chan+1,pitch,vol);
}

void txt_pressure(int chan, int pitch, int press)
{
  prtime();
  printf("Pressure, chan=%d pitch=%d press=%d\n",chan+1,pitch,press);
}

void txt_parameter(int chan, int control, int value)
{
  prtime();
  printf("Parameter, chan=%d c1=%d c2=%d\n",chan+1,control,value);
}


/* [SS] 2017-11-16 submitted by Jonathan Hough (msb,lsb interchanged) */
void txt_pitchbend(int chan, int lsb, int msb)
{
  prtime();
  printf("Pitchbend, chan=%d lsb=%d msb=%d\n",chan+1,msb,lsb);
}

void txt_program(int chan, int program)
{
  prtime();
  printf("Program, chan=%d program=%d\n",chan+1,program);
}

void txt_chanpressure(int chan, int pitch, int press)
{
  prtime();
  printf("Channel pressure, chan=%d pressure=%d\n",chan+1,press);
}

void txt_sysex(int leng, char *mess)
{
  prtime();
  printf("Sysex, leng=%d\n",leng);
}

void txt_metamisc(int type, int leng, char *mess)
{
  prtime();
  printf("Meta event, unrecognized, type=0x%02x leng=%d\n",type,leng);
}

void txt_metaspecial(int type, int leng, char *mess)
{
  prtime();
  printf("Meta event, sequencer-specific, type=0x%02x leng=%d\n",type,leng);
}

void txt_metatext(int type, int leng, char *mess)
{
  static char *ttype[] = {
    NULL,
    "Text Event",    /* type=0x01 */
    "Copyright Notice",  /* type=0x02 */
    "Sequence/Track Name",
    "Instrument Name",  /* ...       */
    "Lyric",
    "Marker",
    "Cue Point",    /* type=0x07 */
    "Unrecognized"
  };
  int unrecognized = (sizeof(ttype)/sizeof(char *)) - 1;
  register int n, c;
  register char *p = mess;

  if ( type < 1 || type > unrecognized )
    type = unrecognized;
  prtime();
  printf("Meta Text, type=0x%02x (%s)  leng=%d\n",type,ttype[type],leng);
  printf("     Text = <");
  for ( n=0; n<leng; n++ ) {
    c = (*p++) & 0xff;
    printf( (isprint(c)||isspace(c)) ? "%c" : "\\0x%02x" , c);
  }
  printf(">\n");
}

void txt_metaseq(int num)
{
  prtime();
  printf("Meta event, sequence number = %d\n",num);
}

void txt_metaeot()
{
  prtime();
  printf("Meta event, end of track\n");
}

void txt_keysig(int sf, int mi)
{
  prtime();
  printf("Key signature, sharp/flats=%d  minor=%d\n",sf,mi);
}

void txt_tempo(long ltempo)
{
  tempo = ltempo;
  prtime();
  printf("Tempo, microseconds-per-MIDI-quarter-note=%ld\n",tempo);
}

void txt_timesig(int nn, int dd, int cc, int bb)
{
  int denom = 1;
  while ( dd-- > 0 )
    denom *= 2;
  prtime();
  printf("Time signature=%d/%d  MIDI-clocks/click=%d  32nd-notes/24-MIDI-clocks=%d\n",
    nn,denom,cc,bb);
}

void txt_smpte(int hr, int mn, int se, int fr, int ff)
{
  prtime();
  printf("SMPTE, hour=%d minute=%d second=%d frame=%d fract-frame=%d\n",
    hr,mn,se,fr,ff);
}

void txt_arbitrary(int leng, char *mess)
{
  prtime();
  printf("Arbitrary bytes, leng=%d\n",leng);
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
  Mf_chanpressure =  txt_chanpressure;
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

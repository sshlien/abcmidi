/* sizes.h                                                        */
/* part of YAPS - abc to PostScript converter                     */
/* defines sizes for musical symbols                              */
/* Copyright James Allwright 2000 */
/* May be copied under the terms of the GNU public license */

/* full region in points (1/72 inch) */
/* A4 is 8.25 x 11.75 inches */
#define A4_PAGEWIDTH 594
#define A4_PAGELEN 846
/* U.S. Letter is 8.5 x 11 inches */
#define US_LETTER_PAGEWIDTH 612
#define US_LETTER_PAGELEN 792
/* margins are not printed in */
#define XMARGIN 40
#define YMARGIN 50

#define TUNE_SCALING 0.7

/* maximum acceptable horizontal gap between notes */
/* if spacing is too great, notes are not spread out to fill stave */
#define MAXGAP 40

/* note spacing on stave - half the gap between 2 consecutive stave lines */
#define TONE_HT 3

/* X offset of accidental (double)sharp/(double)flat/natural */
/* relative to note */
#define ACC_OFFSET 9.6
#define ACC_OFFSET2 7.1
/* height of accidental symbols */
#define NAT_UP 8
#define NAT_DOWN 8
#define FLT_UP 9
#define FLT_DOWN 4
#define SH_UP 8
#define SH_DOWN 9
/* X offset of note stem relative to centre of dot head */
#define HALF_HEAD 3.5
#define GRACE_HALF_HEAD 2.45
#define HALF_BREVE 6.0
/* X width of tail for 1/8th, 1/16th, 1/32th note */
#define TAILWIDTH 5.0
/* X offset of dots relative to each other and from note head */
#define DOT_SPACE 4.0
/* default note stem length */
#define STEMLEN 20.0
#define GRACE_STEMLEN 14.0
#define TEMPO_STEMLEN 14.0

/* Y offsets for placing of tuples above and below beams */
#define TUPLE_UP 5
#define TUPLE_DOWN -14
/* Y space requirement for tuple drawn with half-brackets */
#define HTUPLE_HT 10

/* Decorator spacings */
#define SMALL_DEC_HT 6
#define BIG_DEC_HT 13
/* Offset values define y=0 for the decorators */
#define STC_OFF 0
#define HLD_OFF 1
#define GRM_OFF 4
#define CPU_OFF 0
#define CPD_OFF 0
#define UPB_OFF 0
#define DNB_OFF 0
#define EMB_OFF 1
#define TRL_OFF 2

/* Height of various fonts used */
#define TITLE1_HT 20
#define TITLE2_HT 20
#define TEXT_HT 16
#define COMP_HT 16
#define LYRIC_HT 13
#define CHORDNAME_HT 12
#define INSTRUCT_HT 12
#define WORDS_HT 12

/* height of 1st and 2nd ending markers */
#define END_HT 9.0

/* vertical spacing between consecutive lines of music */
#define VERT_GAP 10

/* Note tails for 1/8, 1/16, 1/32 notes */
/* Defines width of tail and spacing between 2 consecutive tails */
#define TAIL_WIDTH 2.6
#define TAIL_SEP 5.3

/* width of a clef symbols */
#define TREBLE_LEFT 15
#define TREBLE_RIGHT 10
#define CCLEF_LEFT 6
#define CCLEF_RIGHT 10
#define BASS_LEFT 15
#define BASS_RIGHT 10
#define TREBLE_UP 33
#define TREBLE_DOWN 9
#define CLEFNUM_HT 10

struct font {
  int pointsize;
  int space;
  int default_num;
  int special_num;
  char* name;
  int defined;
};

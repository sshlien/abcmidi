/* parseabc.h - interface file for abc parser */
/* used by abc2midi, abc2abc and yaps */

/* abc.h must be #included before this file */
/* functions and variables provided by parseabc.c */

/* for Microsoft Visual C++ 6 */
#ifdef _MSC_VER
#define KANDR
#endif

#include "music_utils.h"

/* the arg list to event_voice keeps growing; if we put the args into a structure
and pass that around, routines that don't need the new ones need not be altered.
NB. event_voice is *called* from parseabc.c, the actual procedure is linked
in from the program-specific file */
/* added middle= stuff */
#define V_STRLEN 256 /* [SS] 2017-10-11 increase from 64 */
struct voice_params {
  int gotclef;
  int gotoctave;
  int gottranspose;
  int gotname;
  int gotsname;
  int gotmiddle;
  int gotother;  /* [SS] 2011-04-18 */
  int octave;
  int transpose;
  char clefname[V_STRLEN+1];
  cleftype_t new_clef;
  char namestring[V_STRLEN+1];
  char snamestring[V_STRLEN+1];
  char middlestring[V_STRLEN+1];
        char other[V_STRLEN+1]; /* [SS] 2011-04-18 */
  };

typedef struct voice_context {
  char label[31];
  int expect_repeat;
  int repeat_count;
  timesig_details_t timesig;
  cleftype_t clef;
  int unitlen; /* unitlen value currently active in voice */
} voice_context_t;

#define MAX_VOICES 30

/* holds a fraction */
struct fraction {
  int num;
  int denom;
};

/* non-zero values for append in words_fn() */
#define W_PLUS_FIELD 1
#define PLUS_FIELD 2

#ifndef KANDR
extern int readnump(char **p);
extern int readsnump(char **p);
extern int readnumf(char *num);
extern void skipspace(char **p);
extern int readsnumf(char *s);
extern void readstr(char out[], char **in, int limit);
extern int getarg(char *option, int argc, char *argv[]);
extern int *checkmalloc(int size);
extern char *addstring(char *s);
extern char *concatenatestring(char *s1, char *s2);
extern char *lookup_abbreviation(char symbol);
extern int ismicrotone(char **p, int dir);
extern void event_normal_tone(void);
extern void print_inputline(void);
extern void print_inputline_nolinefeed(void);
extern void init_timesig(timesig_details_t *timesig);
extern void copy_timesig(timesig_details_t *destination, timesig_details_t *source);
#else
extern int readnump();
extern int readsnump();
extern int readnumf();
extern void skipspace();
extern int readsnumf();
extern void readstr();
extern int getarg();
extern int *checkmalloc();
extern char *addstring();
extern char *concatenatestring();
extern char *lookup_abbreviation();
extern int ismicrotone();
extern void event_normal_tone();
extern void print_inputline_nolinefeed();
extern void init_timesig();
extern void copy_timesig();
#endif
extern void parseron();
extern void parseroff();

/* variables exported by the parser */
extern int lineno;
extern int repcheck; /* allows backend to enable/disable repeat checking */
extern voice_context_t voicecode[MAX_VOICES];
extern timesig_details_t master_timesig;
extern int voicenum;
extern int parserinchord;

/* event_X() routines - these are called from parseabc.c       */
/* the program that uses the parser must supply these routines */
#ifndef KANDR
extern void event_init(int argc, char *argv[], char **filename);
extern void event_text(char *s);
extern void event_reserved(char p);
extern void event_tex(char *s);
extern void event_score_linebreak(char ch);
extern void event_linebreak(void);
extern void event_startmusicline(void);
extern void event_endmusicline(char endchar);
extern void event_eof(void);
extern void event_comment(char *s);
extern void event_specific(char *package, char *s);
extern void event_specific_in_header(char *package, char *s);
extern void event_startinline(void);
extern void event_closeinline(void);
extern void event_field(char k, char *f);
extern void event_words(char *p, int append, int continuation);
extern void event_part(char *s);


extern void event_voice(int n, char *s, struct voice_params *params);
extern void event_length(int n);
extern void event_default_length(int n);
extern void event_blankline(void);
extern void event_refno(int n);
extern void event_tempo(int n, int a, int b, int rel, char *pre, char *post);
extern void event_timesig(timesig_details_t *timesig);
extern void event_octave(int num, int local);
extern void event_info_key(char *key, char *value);
extern void event_info(char *s);
extern void event_key(int sharps, char *s, int modeindex, 
               char modmap[7], int modmul[7], struct fraction modmicro[7],
               int gotkey, int gotclef, char *clefname, cleftype_t *new_clef,
               int octave, int transpose, int gotoctave, int gottranspose,
               int explict);
extern void event_microtone(int dir, int a, int b);
extern void event_graceon(void);
extern void event_graceoff(void);
extern void event_rep1(void);
extern void event_rep2(void);
extern void event_playonrep(char *s);
extern void event_tie(void);
extern void event_slur(int t);
extern void event_sluron(int t);
extern void event_sluroff(int t);
extern void event_rest(int decorators[DECSIZE],int n,int m,int type);
extern void event_mrest(int n,int m,char c);
extern void event_spacing(int n, int m);
extern void event_bar(int type, char *replist);
extern void event_space(void);
extern void event_lineend(char ch, int n);
extern void event_broken(int type, int mult);
extern void event_tuple(int n, int q, int r);
extern void event_chord(void);
extern void event_chordon(int chorddecorators[DECSIZE]);
extern void event_chordoff(int, int);
extern void event_instruction(char *s);
extern void event_gchord(char *s);
extern void event_note(int decorators[DECSIZE], cleftype_t *clef,
                       char accidental, int mult, 
                       char note, int xoctave, int n, int m);
extern void event_abbreviation(char symbol, char *string, char container);
extern void event_acciaccatura();
extern void event_start_extended_overlay();
extern void event_stop_extended_overlay();
extern void event_split_voice();
extern void event_temperament();
extern void print_voicecodes(void);
extern void init_abbreviations();
extern void free_abbreviations();
extern void parsefile();
extern int parsetune();
#else
extern void event_init();
extern void event_text();
extern void event_reserved();
extern void event_tex();
extern void event_score_linebreak();
extern void event_linebreak();
extern void event_startmusicline();
extern void event_endmusicline();
extern void event_eof();
extern void event_comment();
extern void event_specific();
extern void event_specific_in_header();
extern void event_startinline();
extern void event_closeinline();
extern void event_field();
extern void event_words();
extern void event_part();
extern void event_voice();
extern void event_length();
extern void event_default_length();
extern void event_blankline();
extern void event_refno();
extern void event_tempo();
extern void event_timesig();
extern void event_octave();
extern void event_info_key();
extern void event_info();
extern void event_key();
extern void event_microtone();
extern void event_graceon();
extern void event_graceoff();
extern void event_rep1();
extern void event_rep2();
extern void event_playonrep();
extern void event_tie();
extern void event_slur();
extern void event_sluron();
extern void event_sluroff();
extern void event_rest();
extern void event_mrest();
extern void event_spacing();
extern void event_bar();
extern void event_space();
extern void event_lineend();
extern void event_broken();
extern void event_tuple();
extern void event_chord();
extern void event_chordon();
extern void event_chordoff();
extern void event_instruction();
extern void event_gchord();
extern void event_note();
extern void event_ignore();
extern void event_abbreviation();
extern void event_acciaccatura();
extern void event_start_extended_overlay();
extern void event_stop_extended_overlay();
extern void event_split_voice();
extern void print_voicecodes();
extern void init_abbreviations();
extern void free_abbreviations();
extern void parsefile();
extern int parsetune();
#endif

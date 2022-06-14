/* structs.h                                  */
/* Part of YAPS - abc to PostScript converter */
/* Copyright James Allwright 2000 */
/* May be copied under the terms of the GNU public license */

/* definitions of structures used to hold abc tune data */
/* The voice data structure also holds various state variables */
/* to allow processing of voice data as it is read in */

#include "music_utils.h"

enum tail_type {nostem, single, startbeam, midbeam, endbeam};

/* holds a fraction */
struct fract {
  int num;
  int denom;
};
extern void reducef(struct fract *f);
extern void setfract(struct fract *f, int a, int b);

/* holds a key signature */
struct key {
  char* name;
  int sharps;
  char map[7];
  int mult[7];
};

/* holds a tempo specifier */
struct atempo {
  int count;
  struct fract basenote;
  int relative;
  char *pre;
  char *post;
};

/* holds a general tuple specifier */
struct tuple {
  int n;
  int q;
  int r;
  int label;
  int beamed;
  float height;
};

/* data relating to a chord */
struct chord {
  int ytop;
  int ybot;
  enum tail_type beaming;
  float stemlength;
  int stemup;
  int base;
  int base_exp;
};

/* elemental unit of a linked list */
struct el {
  struct el* next;
  void* datum;
};

/* high-level structure for linked list */
struct llist {
  struct el* first;
  struct el* last;
  struct el* place;
};

/* all information relating to a note */
struct note {
  char* accents;
  char accidental;
  int fliphead;
  int acc_offset;
  int mult;
  int octave;
  char pitch;
  int tuplenotes;
  int y;
  int base_exp;
  int base;
  int dots;
  int stemup;
  enum tail_type beaming;
  struct llist* syllables;
  struct llist* gchords;
  struct llist* instructions;
  struct fract len;
  float stemlength;
};

struct dynamic {
  char color;
  };

/* elemental unit of a voice list */
/* item points to a note, bar-line or other feature */
struct feature {
  struct feature* next;
  featuretype type;
  float xleft, xright, ydown, yup;
  float x;
  /* [JA] 2020-10-27 */
  union {
    void *voidptr;
    int number;
  }item;
};

/* structure used by both slurs and ties */
struct slurtie {
  struct feature* begin;
  struct feature* end;
  int crossline;
};  

enum linestate {header, midline, newline};

/* holds calculated vertical spacing for one stave line */
/* associated with PRINTLINE */
struct vertspacing {
  float height;
  float descender;
  float yend;
  float yinstruct;
  float ygchord;
  float ywords;
};

/* all variables relating to one voice */
#define MAX_TIES 20
#define MAX_SLURS 20
struct voice {
  struct feature* first;
  struct feature* last;
  int voiceno;
  int octaveshift;
  int changetime;
  struct fract unitlen;
  struct fract tuplefactor;
  int tuplenotes;
  struct tuple* thistuple;
  int inslur, ingrace;
  int inchord, chordcount;
  struct fract chord;
  struct fract barlen;
  struct fract barcount;
  int barno;
  int barchecking;
  int brokentype, brokenmult, brokenpending;
  int tiespending;
  struct feature* tie_place[MAX_TIES];
  int tie_status[MAX_TIES];
  int slurpending;
  int slurcount;
  struct slurtie* slur_place[MAX_SLURS];
  struct feature* lastnote;
  struct feature* laststart;
  struct feature* lastend;
  int more_lyrics;
  int lyric_errors;
  struct feature* thisstart;
  struct feature* thisend;
  struct llist* gchords_pending;
  struct llist* instructions_pending;
  /* variables to handle tuples */
  int beamed_tuple_pending;
  int tuple_count;
  int tuplelabel;
  float tuple_xstart;
  float tuple_xend;
  float tuple_height;
  /* variables for assigning syllables to notes */
  struct feature* linestart;
  struct feature* lyrics_end;
  struct feature* lineend;
  struct chord* thischord;
  struct feature* chordplace;
  enum linestate line;
  /* following fields are initially inherited from tune */
  cleftype_t* clef;
  struct key* keysig;
  timesig_details_t timesig;
  struct atempo* tempo;
  /* place used while printing to keep track of progress through voice */
  struct feature* lineplace;
  int atlineend;
  struct feature* place;
  int inmusic;
  struct fract time;
  /* following are used to determine stem direction of beamed set */
  struct feature* beamroot;
  struct feature* beamend;
  struct feature* gracebeamroot;
  struct feature* gracebeamend;
  int beammin, beammax;
};

/* structure for the entire tune */
struct tune {
  int no; /* from X: field */
  int octaveshift;
  struct llist title;
  char* composer;
  char* origin;
  char* parts;
  struct llist notes;
  timesig_details_t timesig;
  int barchecking;
  struct key* keysig;
  struct atempo* tempo;
  struct llist voices;
  struct voice* cv;
  struct fract unitlen;
  cleftype_t clef;
  struct llist words;
};

/* structure for a rest */
struct rest {
  struct fract len;
  int base_exp;
  int base;
  int dots;
  int multibar; /* 0 indicates normal rest */
  struct llist* gchords;
  struct llist* instructions;
};

/* list manipulation functions */
void init_llist(struct llist* l);
struct llist* newlist();
void addtolist(struct llist* p, void* item);
void* firstitem(struct llist* p);
void* nextitem(struct llist* p);
void freellist(struct llist* l);

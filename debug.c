/* debug.c */
/* part of yaps - abc to PostScript converter */
/* print tune data structure to screen for debugging purposes only */

#include <stdio.h>
#include "abc.h"
#include "structs.h"

void showfeature(struct feature *ft)
{
  char* astring;
  struct fract* afract;
  struct key* akey;
  struct rest* arest;
  struct tuple* atuple;
  struct note* anote;
  cleftype_t* theclef; /* [JA] 2020-10-19 */
    switch (ft->type) {
    case SINGLE_BAR:  printf("SINGLE_BAR\n");
      break;
    case DOUBLE_BAR:  printf("DOUBLE_BAR\n");
      break;
    case BAR_REP:  printf("BAR_REP\n");
      break;
    case REP_BAR:  printf("REP_BAR\n");
      break;
    case REP1:  printf("REP1\n");
      break;
    case REP2:  printf("REP2\n");
      break;
    case PLAY_ON_REP:
      printf("PLAY_ON_REP %s\n", (char*)ft->item.voidptr);
      break;
    case BAR1:  printf("BAR1\n");
      break;
    case REP_BAR2:  printf("REP_BAR2\n");
      break;
    case DOUBLE_REP:  printf("DOUBLE_REP\n");
      break;
    case THICK_THIN:  printf("THICK_THIN\n");
      break;
    case THIN_THICK:  printf("THIN_THICK\n");
      break;
    case PART:  printf("PART\n");
      astring = ft->item.voidptr;
      break;
    case TEMPO:  printf("TEMPO\n");
      break;
    case TIME:
      afract = ft->item.voidptr;
      printf("TIME %d / %d\n", afract->num, afract->denom);
      break;
    case KEY:  printf("KEY\n");
      akey = ft->item.voidptr;
      break;
    case REST:  printf("REST\n");
      arest = ft->item.voidptr;
      break;
    case TUPLE:  printf("TUPLE\n");
      atuple = ft->item.voidptr;
      break;
    case NOTE:
      anote = ft->item.voidptr;
      printf("NOTE %c%c %d / %d\n", anote->accidental, anote->pitch,
               anote->len.num, anote->len.denom);
      if (anote->gchords != NULL) {
        astring = firstitem(anote->gchords);
        while (astring != NULL) {
          printf("gchord: %s\n", astring);
          astring = nextitem(anote->gchords);
        };
      };
      if (anote->syllables != NULL) {
        astring = firstitem(anote->syllables);
        while (astring != NULL) {
          printf("syllable: %s\n", astring);
          astring = nextitem(anote->syllables);
        };
      };
      printf("stemup=%d beaming=%d base =%d base_exp=%d  x=%.1f left=%.1f right=%.1f\n", 
              anote->stemup, anote->beaming, anote->base,  anote->base_exp,
              ft->x, ft->xleft, ft->xright);
      break;
    case CHORDNOTE: 
      anote = ft->item.voidptr;
      printf("CHORDNOTE %c%c %d / %d\n", anote->accidental, anote->pitch,
               anote->len.num, anote->len.denom);
      printf("stemup=%d beaming=%d base =%d base_exp=%d x=%.1f left=%.1f right=%.1f\n", 
              anote->stemup, anote->beaming, anote->base, anote->base_exp,
              ft->x, ft->xleft, ft->xright);
      printf("x=%.1f\n", ft->x);
      break;
    case NONOTE: printf("NONOTE\n");
      break;
    case OLDTIE:  printf("OLDTIE\n");
      break;
    case TEXT:  printf("TEXT\n");
      break;
    case SLUR_ON:  printf("SLUR_ON\n");
      break;
    case SLUR_OFF:  printf("SLUR_OFF\n");
      break;
    case TIE:  printf("TIE\n");
      break;
    case CLOSE_TIE:  printf("CLOSE_TIE\n");
      break;
    case TITLE:  printf("TITLE\n");
      break;
    case CHANNEL:  printf("CHANNEL\n");
      break;
    case TRANSPOSE:  printf("TRANSPOSE\n");
      break;
    case RTRANSPOSE:  printf("RTRANSPOSE\n");
      break;
    case GRACEON: printf("GRACEON\n");
      break;
    case GRACEOFF: printf("GRACEOFF\n");
      break;
    case SETGRACE:  printf("SETGRACE\n");
      break;
    case SETC:  printf("SETC\n");
      break;
    case GCHORD:  printf("GCHORD\n");
      break;
    case GCHORDON:  printf("GCHORDON\n");
      break;
    case GCHORDOFF:  printf("GCHORDOFF\n");
      break;
    case VOICE:  printf("VOICE\n");
      break;
    case CHORDON:  printf("CHORDON\n");
      break;
    case CHORDOFF:  printf("CHORDOFF\n");
      break;
    case SLUR_TIE:  printf("SLUR_TIE\n");
      break;
    case TNOTE:  printf("TNOTE\n");
      break;
    case LT:  printf("LT\n");
      break;
    case GT:  printf("GT\n");
      break;
    case DYNAMIC:  printf("DYNAMIC\n");
      break;
    case LINENUM:  
      printf("LINENUM %d\n", ft->item.number);
      break;
    case MUSICLINE:  printf("MUSICLINE\n");
      break;
    case PRINTLINE: printf("PRINTLINE\n");
      break;
    case MUSICSTOP:  printf("MUSICSTOP\n");
      break;
    case WORDLINE:  printf("WORDLINE\n");
      break;
    case WORDSTOP:  printf("WORDSTOP\n");
      break;
    case INSTRUCTION: printf("INSTRUCTION\n");
      break;
    case NOBEAM: printf("NOBEAM\n");
      break;
    case CLEF: printf("CLEF\n");
      theclef = ft->item.voidptr;
      break;
    case SPLITVOICE: printf("SPLITVOICE\n");
    default:
      printf("unknown type: %d\n", (int)ft->type);
      break;
    };
}


static int showline(v)
struct voice* v;
/* draws one line of music from specified voice */
{
  struct feature* ft;
/*  struct note* anote;
  struct key* akey;
  char* astring;
  struct fract* afract;
  struct rest* arest;
  struct tuple* atuple;
  cleftype_t* theclef; /* [JA] 2020-10-19 */
  int sharps;
  struct chord* thischord;
  int chordcount;
  int printedline;

  if (v->place == NULL) {
    return(0);
  };
  chordcount = 0;
  v->beamed_tuple_pending = 0;
  thischord = NULL;
  if (v->keysig == NULL) {
    event_error("Voice has no key signature");
  } else {
    sharps = v->keysig->sharps;
  };
  ft = v->place;
  printedline = 0;
  while ((ft != NULL) && (!printedline)) {
    /* printf("type = %d\n", ft->type); */
    showfeature(ft);
    ft = ft->next;
  };
  v->place = ft;
  return(1);
}

void showtune(struct tune* t)
/* print abc structure to the screen */
{
  char* atitle;
  int notesdone;
  struct voice* thisvoice;

  notesdone = 0;
  atitle = firstitem(&t->title);
  while (atitle != NULL) {
    printf("TITLE: %s\n", atitle);
    atitle = nextitem(&t->title);
  };
  if (t->composer != NULL) {
    printf("COMPOSER: %s\n", t->composer); /* [SDG] 2020-06-03 */
  };
  if (t->origin != NULL) {
    printf("ORIGIN: %s\n", t->origin);
  };
  if (t->parts != NULL) {
    printf("PARTS: %s\n", t->parts);
  };
  thisvoice = firstitem(&t->voices);
  while (thisvoice != NULL) {
    thisvoice->place = thisvoice->first;
    thisvoice = nextitem(&t->voices);
  };
/*
  doneline = 1;
  while(doneline) {
*/
    thisvoice = firstitem(&t->voices);
    while (thisvoice != NULL) {
      printf("-----------(voice %d)-------\n", thisvoice->voiceno);
      showline(thisvoice);
      thisvoice = nextitem(&t->voices);
    };
/*  }; */
}


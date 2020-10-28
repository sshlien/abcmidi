/*
 * yaps - program to convert abc files to PostScript.
 * Copyright (C) 1999 James Allwright
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
 *
 */

/* position.c */
/* part of YAPS - abc to PostScript converter */
/* This file contains routines to calculate symbol positions within */
/* a line of music. */

/* for Microsoft Visual C++ 6.0 and higher */
#ifdef _MSC_VER
#define ANSILIBS
#endif

#include <stdio.h>
#ifdef ANSILIBS
#include <stdlib.h>
#endif
#include "abc.h"
#include "structs.h"
#include "sizes.h"

extern struct tune thetune;
extern double scaledwidth;

static void addfract(f, n, m)
struct fract* f;
int n, m;
/* add n/m to fraction pointed to by f */
/* like addunits(), but does not use unitlength */
{
  f->num = n*f->denom + m*f->num;
  f->denom = m*f->denom;
  reducef(f);
}

static void mulfract(f, n, m)
struct fract* f;
int n, m;
/* multiply  n/m to fraction pointed to by f */
/* like addunits(), but does not use unitlength */
{
  f->num =   n*f->num;
  f->denom = m*f->denom;
  reducef(f);
}

static void advance(struct voice* v, int phase, int* items, double* itemspace, double x)
/* move on one symbol in the specified voice */
{
  struct feature* p;
  struct rest* arest;
  struct note* anote;
  struct fract tuplefactor, notelen;
  int done;
  int stepon;
  int zerotime, newline;

  switch(phase) {
  case 1:
    zerotime = 1;
    newline=0;
    break;
  case 2:
    zerotime = 0;
    newline=0;
    break;
  case 3:
    zerotime = 0;
    newline=1;
    break;
  default:
    printf("Internal error: phase = %d\n", phase);
    exit(1);
    break;
  };

  *itemspace = 0.0;
  *items = 0;
  p = v->place;
  if (p == NULL) {
    v->atlineend = 1;
  };
  done = 0;
  while ((p != NULL) && (done==0)) {
    p->x = (float) (x + p->xleft);
    stepon = 1;
    switch(p->type) {
    case MUSICLINE:
      v->inmusic = 1;
      break;
    case PRINTLINE:
      v->inmusic = 0;
      done = 1;
      if (!newline) {
        v->atlineend = 1;
        stepon = 0;
      };
      break;
    case CLEF:
    case KEY:
    case TIME:
      if (!v->inmusic) {
        break;
      };
    case SINGLE_BAR:
    case DOUBLE_BAR:
    case BAR_REP:
    case REP_BAR:
    case BAR1:
    case REP_BAR2:
    case DOUBLE_REP:
    case THICK_THIN:
    case THIN_THICK:
      *itemspace = *itemspace +  p->xleft + p->xright;
      *items = *items + 1;
      if (!newline) {
        done = 1;
      };
      break;
    case REST:
    case NOTE:
      tuplefactor = v->tuplefactor;
      if ((zerotime==1) && (!v->ingrace)) {
        done = 1;
        stepon = 0;
      } else {
        if ((!v->inchord)&&(!newline)) {
          done = 1;
        };
        *itemspace = *itemspace +  p->xleft + p->xright;
        *items = *items + 1;
        if (p->type == REST) {
          arest = p->item.voidptr;
          addfract(&v->time, arest->len.num, arest->len.denom);
        };
        if ((p->type == NOTE) && (!v->ingrace)) {
          anote = p->item.voidptr;
	  notelen = anote->len;

 	 if (anote->tuplenotes >0) {
		  mulfract(&notelen,tuplefactor.num,tuplefactor.denom);
	           }


          addfract(&v->time, notelen.num, notelen.denom);
/*	  printf("%c %d/%d %d/%d\n",anote->pitch,notelen.num,notelen.denom,
			  v->time.num,v->time.denom);
*/
        };
      };
      break;
    case CHORDON:
      if ((zerotime==1)&&(!v->ingrace)) {
        done = 1;
        stepon = 0;
      } else {
        v->inchord = 1;
      };
      break;
    case CHORDOFF:
      if (v->inchord == 1) {
        v->inchord = 0;
        if ((!v->ingrace)&&(!newline)) {
          done = 1;
        };
      };
      break;
    case GRACEON:
      v->ingrace = 1;
      break;
    case GRACEOFF:
      v->ingrace = 0;
      break;
    default:
      break;
    };
    if (stepon) {
      p = p->next;
    } else {
      done = 1;
    };
  };
  v->place = p;
  if (p == NULL) {
    v->atlineend = 1;
  };
}

static int gefract(struct fract* a, struct fract* b)
/* compare two fractions  a greater than or equal to b */
/* returns   (a >= b)                                  */
{
  if ((a->num*b->denom) >= (b->num*a->denom)) {
    return(1);
  } else {
    return(0);
  };
}

static int gtfract(struct fract* a, struct fract* b)
/* compare two fractions  a greater than b */
/* returns   (a > b)                       */
{
  if ((a->num*b->denom) > (b->num*a->denom)) {
    return(1);
  } else {
    return(0);
  };
}

static int spacemultiline(struct fract* mastertime, struct tune* t)
/* calculate spacing for one line (but possibly multiple voices) */
{
  int i;
  int items, thisitems, maxitems;
  int totalitems;
  double thiswidth, maxwidth;
  double totalwidth;
  double x, gap;
  int done;
  struct voice* v;
  struct fract minlen;

  /* two passes - on the second pass, inter-symbol spacing is */
  /* known so elements can be given their correct x position */
  gap = 0.0;
  for (i=0; i<2; i++) {
    setfract(mastertime, 0, 1);
    v = firstitem(&t->voices);
    while (v != NULL) {
      v->place = v->lineplace;
      v->ingrace = 0;
      v->atlineend = 0;
      setfract(&v->time, 0, 1);
      v = nextitem(&t->voices);
    };
    done = 0;
    items = 0;
    x = 0.0;
    totalitems = 0;
    totalwidth = 0.0;
    /* count up items in a line */
    while (done == 0) {
      maxitems = 0;
      maxwidth = 0.0;
      /* first do zero-time symbols */
      v = firstitem(&t->voices);
      while (v != NULL) {
        if ((!v->atlineend)&&(gefract(mastertime, &v->time))) {
          advance(v, 1, &thisitems, &thiswidth, x);
          if (thisitems > maxitems) {
            maxitems = thisitems;
          };
          if (thiswidth > maxwidth) {
            maxwidth = thiswidth;
          };
        };
        v = nextitem(&t->voices);
      };
      if (maxitems == 0) {
        /* now try moving forward in time */
        /* advance all voices at or before mastertime */
        v = firstitem(&t->voices);
        while (v != NULL) {
          if ((!v->atlineend)&&(gefract(mastertime, &v->time))) {
            advance(v, 2, &thisitems, &thiswidth, x);
            if (thisitems > maxitems) {
              maxitems = thisitems;
            };
            if (thiswidth > maxwidth) {
              maxwidth = thiswidth;
            };
          };
          v = nextitem(&t->voices);
        };
        /* calculate new mastertime */
        v = firstitem(&t->voices);
        setfract(&minlen, 0, 1);
        done = 1;
        while (v != NULL) {
          if (!v->atlineend) {
            done = 0;
            if (minlen.num == 0) {
              setfract(&minlen, v->time.num, v->time.denom);
            } else {
              if (gtfract(&minlen, &v->time)) {
                setfract(&minlen, v->time.num, v->time.denom);
              };
            };
          };
          v = nextitem(&t->voices);
        };
        setfract(mastertime, minlen.num, minlen.denom);
      };
      totalitems = totalitems + maxitems;
      totalwidth = totalwidth + maxwidth;
      if (maxitems > 0) {
        x = x + maxwidth + gap;
      };
    };
    /* now calculate inter-symbol gap */
    if (totalitems > 1) {
      gap = (scaledwidth - totalwidth)/(totalitems-1);
    } else {
      gap = 1.0;
    };
    if (gap < 0.0) {
      event_error("Overfull music line");
    };
    if (gap > MAXGAP) {
      event_error("Underfull music line");
      gap = MAXGAP;
    };
  };
  if (totalitems == 0) {
    return(1);
  } else {
    return(0);
  };
}

void spacevoices(struct tune* t)
{
  struct fract mastertime;
  int donelines;
  struct voice* v;
  int items;
  double x1;

  /* initialize voices */
  v = firstitem(&t->voices);
  while (v != NULL) {
    v->lineplace = v->first;
    v->inmusic=0;
    v = nextitem(&t->voices);
  };
  donelines = 0;
  while(donelines == 0) {
    donelines = spacemultiline(&mastertime, t);
    v = firstitem(&t->voices);
    while (v != NULL) {
      v->lineplace = v->place;
      advance(v, 3, &items, &x1, 0.0);
      v->lineplace = v->place;
      v = nextitem(&t->voices);
    };
  };
}

static int spaceline(struct voice* v)
/* allocate spare space across the width of a single stave line  */
/* thereby fixing the x position of all notes and other elements */
/* returns 0 when the end of the voice is reached, 1 otherwise   */
{
  struct feature* p;
  double x, lastx = 0.0;  /* [SDG] 2020-06-03 */
  int inmusic, items;
  double itemspace;
  double gap;

  itemspace = 0.0;
  items = 0;
  inmusic = 0;
  p = v->place;
  while ((p != NULL) && (p->type != PRINTLINE)) {
    switch(p->type) {
    case MUSICLINE:
      inmusic = 1;
      break;
    case PRINTLINE:
      inmusic = 0;
      break;
    case CLEF:
    case KEY:
    case TIME:
      if (!inmusic) {
        break;
      };
    case SINGLE_BAR:
    case DOUBLE_BAR:
    case BAR_REP:
    case REP_BAR:
    case BAR1:
    case REP_BAR2:
    case DOUBLE_REP:
    case THICK_THIN:
    case THIN_THICK:
    case REST:
    case NOTE:
      itemspace = itemspace +  p->xleft + p->xright;
      items = items + 1;
      break;
    default:
      break;
    };
    p = p->next;
  };
  if (items > 1) {
    gap = (scaledwidth - itemspace)/((double)(items-1));
  } else {
    gap = 1.0;
  };
  if (gap < 0.0) {
    event_error("Overfull music line");
  };
  if (gap > MAXGAP) {
    event_error("Underfull music line");
    gap = MAXGAP;
  };
  /* now assign positions */
  x = 0.0; 
  p = v->place;
  inmusic = 0;
  while ((p != NULL) && (p->type != PRINTLINE)) {
    switch(p->type) {
    case MUSICLINE:
      inmusic = 1;
      break;
    case PRINTLINE:
      inmusic = 0;
      break;
    case CHORDNOTE:
      p->x = (float) lastx;
      break;
    case CLEF:
    case KEY:
    case TIME:
      if (!inmusic) {
        break;
      };
    case SINGLE_BAR:
    case DOUBLE_BAR:
    case BAR_REP:
    case REP_BAR:
    case BAR1:
    case REP_BAR2:
    case DOUBLE_REP:
    case THICK_THIN:
    case THIN_THICK:
    case REST:
    case NOTE:
      x = x + p->xleft;
      p->x = (float) x;
      lastx = x;
      x = x + p->xright + gap;
      break;
    default:
      break;
    };
    p = p->next;
  };
  while ((p!=NULL)&&((p->type == PRINTLINE)||(p->type==LINENUM))) {
    p = p->next;
  };
  v->place = p;
  if (p == NULL) {
    return(0);
  } else {
    return(1);
  };
}

void monospace(struct tune* t)
{
  int doneline;
  struct voice* v;

  v = firstitem(&t->voices);
  while (v != NULL) {
    doneline = 1;
    v->place = v->first;
    while (doneline == 1) {
      doneline = spaceline(v);
    };
    v = nextitem(&t->voices);
  };
}

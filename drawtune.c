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

/* drawtune.c */
/* This file contains routines for generating final PostScript Output */
/* There are 2 stages to this process. First the symbol positions are */
/* calculated, then the PostScript symbols are generated.             */

#ifdef _MSC_VER
#define ANSILIBS 1
#define snprintf _snprintf
#endif

#include <stdio.h>
#ifdef ANSILIBS
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#endif

#include "abc.h"
#include "structs.h"
#include "sizes.h"
#include "drawtune.h"

/* external functions  and variables */
extern struct tune thetune;
extern int debugging;
extern int pagenumbering;
extern int barnums, nnbars;
extern int make_open();
extern void printlib();
extern int count_dots(int *base, int *base_exp, int n, int m);

extern void monospace(struct tune* t);
extern void spacevoices(struct tune* t);

/* provided by debug.c */
extern void showtune(struct tune *t);

struct key* newkey(char* name, int sharps, char accidental[], int mult[]);
cleftype_t* newclef(cleftype_t* clef);
char* addstring(char *s);
extern int lineno;
extern int separate_voices;
extern int print_xref;
extern int landscape;

static void pagebottom();
static void setfont(int size, int num);

FILE* f = NULL;
struct feature* beamset[64];
struct feature* gracebeamset[32];
int beamctr, gracebeamctr;;
int rootstem;
int fontsize, fontnum;
int donemeter;
int inchord;
int chordcount;
static int ingrace; /* [SDG] 2020-06-03 */
struct feature* chordhead;

double scale, descend, totlen, oldplace;
int pagecount = 1;
int pagelen, pagewidth, xmargin, ymargin;
double scaledlen, scaledwidth;
int staffsep;
int eps_out;
int titleleft = 0;
int titlecaps = 0;
int gchords_above = 1;
int redcolor; /* [SS] 2013-11-04*/

enum placetype {left, right, centre};
struct font textfont;
struct font titlefont;
struct font subtitlefont;
struct font wordsfont;
struct font composerfont;
struct font vocalfont;
struct font gchordfont;
struct font partsfont;

/* arrays giving character widths for characters 32-255 */

/* font for lyrics : 13 point Times-Bold */
double timesbold_width[224] = {
3.249, 4.328, 7.214, 6.499, 6.499, 12.999, 10.828, 4.328, 
4.328, 4.328, 6.499, 7.409, 3.249, 7.409, 3.249, 3.613, 
6.499, 6.499, 6.499, 6.499, 6.499, 6.499, 6.499, 6.499, 
6.499, 6.499, 4.328, 4.328, 7.409, 7.409, 7.409, 6.499, 
12.089, 9.385, 8.67, 9.385, 9.385, 8.67, 7.942, 10.113, 
10.113, 5.056, 6.499, 10.113, 8.67, 12.271, 9.385, 10.113, 
7.942, 10.113, 9.385, 7.227, 8.67, 9.385, 9.385, 12.999, 
9.385, 9.385, 8.67, 4.328, 3.613, 4.328, 7.552, 6.499, 
4.328, 6.499, 7.227, 5.771, 7.227, 5.771, 4.328, 6.499, 
7.227, 3.613, 4.328, 7.227, 3.613, 10.828, 7.227, 6.499, 
7.227, 7.227, 5.771, 5.056, 4.328, 7.227, 6.499, 9.385, 
6.499, 6.499, 5.771, 5.121, 2.859, 5.121, 6.759, 3.249, 
3.249, 3.249, 3.249, 3.249, 3.249, 3.249, 3.249, 3.249, 
3.249, 3.249, 3.249, 3.249, 3.249, 3.249, 3.249, 3.249, 
3.613, 4.328, 4.328, 4.328, 4.328, 4.328, 4.328, 4.328, 
4.328, 3.249, 4.328, 4.328, 3.249, 4.328, 4.328, 4.328, 
3.249, 4.328, 6.499, 6.499, 6.499, 6.499, 2.859, 6.499, 
4.328, 9.71, 3.899, 6.499, 7.409, 4.328, 9.71, 4.328, 
5.199, 7.409, 3.899, 3.899, 4.328, 7.227, 7.019, 3.249, 
4.328, 3.899, 4.289, 6.499, 9.749, 9.749, 9.749, 6.499, 
9.385, 9.385, 9.385, 9.385, 9.385, 9.385, 12.999, 9.385, 
8.67, 8.67, 8.67, 8.67, 5.056, 5.056, 5.056, 5.056, 
9.385, 9.385, 10.113, 10.113, 10.113, 10.113, 10.113, 7.409, 
10.113, 9.385, 9.385, 9.385, 9.385, 9.385, 7.942, 7.227, 
6.499, 6.499, 6.499, 6.499, 6.499, 6.499, 9.385, 5.771, 
5.771, 5.771, 5.771, 5.771, 3.613, 3.613, 3.613, 3.613, 
6.499, 7.227, 6.499, 6.499, 6.499, 6.499, 6.499, 7.409, 
6.499, 7.227, 7.227, 7.227, 7.227, 6.499, 7.227, 6.499, 
};

/* font for chord names : 12 point Helvetica */
double helvetica_width[224] = {
3.335, 3.335, 4.259, 6.671, 6.671, 10.667, 8.003, 2.651, 
3.995, 3.995, 4.667, 7.007, 3.335, 3.995, 3.335, 3.335, 
6.671, 6.671, 6.671, 6.671, 6.671, 6.671, 6.671, 6.671, 
6.671, 6.671, 3.335, 3.335, 7.007, 7.007, 7.007, 6.671, 
12.179, 8.003, 8.003, 8.663, 8.663, 8.003, 7.331, 9.335, 
8.663, 3.335, 5.999, 8.003, 6.671, 9.995, 8.663, 9.335, 
8.003, 9.335, 8.663, 8.003, 7.331, 8.663, 8.003, 11.327, 
8.003, 8.003, 7.331, 3.335, 3.335, 3.335, 5.627, 6.671, 
2.663, 6.671, 6.671, 5.999, 6.671, 6.671, 3.335, 6.671, 
6.671, 2.663, 2.663, 5.999, 2.663, 9.995, 6.671, 6.671, 
6.671, 6.671, 3.995, 5.999, 3.335, 6.671, 5.999, 8.663, 
5.999, 5.999, 5.999, 4.007, 3.119, 4.007, 7.007, 3.335, 
3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 
3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 
3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 
3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 
3.335, 3.995, 6.671, 6.671, 2.003, 6.671, 6.671, 6.671, 
6.671, 2.291, 3.995, 6.671, 3.995, 3.995, 5.999, 5.999, 
3.335, 6.671, 6.671, 6.671, 3.335, 3.335, 6.443, 4.199, 
2.663, 3.995, 3.995, 6.671, 11.999, 11.999, 3.335, 7.331, 
3.335, 3.995, 3.995, 3.995, 3.995, 3.995, 3.995, 3.995, 
3.995, 3.335, 3.995, 3.995, 3.335, 3.995, 3.995, 3.995, 
11.999, 3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 
3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 
3.335, 11.999, 3.335, 4.439, 3.335, 3.335, 3.335, 3.335, 
6.671, 9.335, 11.999, 4.379, 3.335, 3.335, 3.335, 3.335, 
3.335, 10.667, 3.335, 3.335, 3.335, 3.335, 3.335, 3.335, 
2.663, 7.331, 11.327, 7.331, 3.335, 3.335, 3.335, 3.335, 
};

static int ISOencoding(char ch1, char ch2)
/* converts the 2 characters following '\' into an ISO Latin 1 Font */
/* character code in the range 128-255 */
{
  int code;

  code = 0;
  switch (ch1) {
  case '`':
    switch(ch2) {
    case 'A': code = 0300; break;
    case 'E': code = 0310; break;
    case 'I': code = 0314; break;
    case 'O': code = 0322; break;
    case 'U': code = 0331; break;
    case 'a': code = 0340; break;
    case 'e': code = 0350; break;
    case 'i': code = 0354; break;
    case 'o': code = 0362; break;
    case 'u': code = 0371; break;
    default:
      break;
    };
    break;
  case '\'':
    switch(ch2) {
    case 'A': code = 0301; break;
    case 'E': code = 0311; break;
    case 'I': code = 0315; break;
    case 'U': code = 0332; break;
    case 'Y': code = 0335; break;
    case 'a': code = 0341; break;
    case 'e': code = 0351; break;
    case 'i': code = 0355; break;
    case 'o': code = 0363; break;
    case 'u': code = 0372; break;
    case 'y': code = 0375; break;
    default:
      break;
    };
    break;
  case '^':
    switch(ch2) {
    case 'A': code = 0302; break;
    case 'E': code = 0312; break;
    case 'I': code = 0316; break;
    case 'O': code = 0324; break;
    case 'U': code = 0333; break;
    case 'a': code = 0342; break;
    case 'e': code = 0352; break;
    case 'i': code = 0356; break;
    case 'o': code = 0364; break;
    case 'u': code = 0373; break;
    default:
      break;
    };
    break;
  case '"':
    switch(ch2) {
    case 'A': code = 0304; break;
    case 'E': code = 0313; break;
    case 'I': code = 0317; break;
    case 'O': code = 0326; break;
    case 'U': code = 0334; break;
    case 'a': code = 0344; break;
    case 'e': code = 0353; break;
    case 'i': code = 0357; break;
    case 'o': code = 0366; break;
    case 'u': code = 0374; break;
    case 'y': code = 0377; break;
    default:
      break;
    };
    break;
  case '~':
    switch(ch2) {
    case 'A': code = 0303; break;
    case 'N': code = 0321; break;
    case 'O': code = 0325; break;
    case 'a': code = 0343; break;
    case 'n': code = 0361; break;
    case 'o': code = 0365; break;
    default:
      break;
    };
    break;
  case 'c':
    switch(ch2) {
    case 'C': code = 0307; break;
    case 'c': code = 0347; break;
    default:
      break;
    };
    break;
  case 'o':
    switch(ch2) {
    case 'A': code = 0305; break;
    case 'a': code = 0345; break;
    default:
      break;
    };
    break;
  case 'A':
    if (ch2=='A') {
      code = 0305;
    };
    if (ch2=='E') {
      code = 0305;
    };
    break;
  case 'a':
    if (ch2=='a') {
      code = 0345;
    };
    if (ch2=='e') {
      code = 0346;
    };
    break;
  case 's':
    if (ch2=='s') {
      code = 0337;
    };
    break;
  default:
    break;
  };
  return(code);
}

static int getISO(char s[], int j, int* code)
/* convert input into 8-bit character code */
/* s[] is input string, j is place in string */
/* returns new place in string and writes 8-bit value to code */
{
  int i;
  int count;

  i =j;
  if (s[i]=='\\') {
    /* look for 2 character code */
    *code = ISOencoding(s[i+1], s[i+2]);
    if (*code != 0) {
      i = i+3;
    } else {
      /* look for octal code */
      i = i+1;
      *code = 0;
      count = 0;
      while ((s[i]>='0')&&(s[i]<='7')&&(count<3)) {
        *code = ((*code)*8+(int)s[i]-'0')%256;
        count = count+1;
        i = i+1;
      };
      if (*code == 0) {
        *code = '\\';
      };
    };
  } else {
    *code = (int)s[i] & 0xFF;
    i = i + 1;
  };
  return(i);
}

static void ISOdecode(char s[], char out[])
/* convert coded characters to straight 8-bit ascii */
{
  int i, j, len;
  int code;
  
  len = strlen(s);
  i = 0;
  j = 0;
  while (i<len) {
    i = getISO(s, i, &code);
    out[j] = (char)code;
    j = j + 1;
  };
  out[j] = '\0';
}

static void ISOfprintf(char s[])
/* interpret special characters and print to file */
{
  int i, len, ch;
  int code;
  
  len = strlen(s);
  i = 0;
  while (i<len) {
    switch(s[i]) {
    case '(':
      fprintf(f, "\\(");
      i = i+1;
      break;
    case ')':
      fprintf(f, "\\)");
      i = i+1;
      break;
    case '\\':
      i = getISO(s, i, &code);
      fprintf(f, "\\%03o", code);
      break;
    default:
      ch = 0xFF & (int)s[i];
      if ((ch > 31) && (ch < 128)) {
        fprintf(f, "%c", s[i]);
      } else {
        fprintf(f, "\\%03o", 0xFF & (int)s[i]);
      };
      i = i+1;
      break;
    };
  };
}

static double stringwidth(char* str, double ptsize, int fontno)
/* calculate width of string */
{
  int i, len, ch;
  double width;
  int code;
  double baseptsize;
  double* charwidth;

  if (fontno == 3) {
    charwidth = &helvetica_width[0];
    baseptsize = 12.0;
  } else {
    charwidth = &timesbold_width[0];
    baseptsize = 13.0;
  };
  width = 0.0;
  len = strlen(str);
  i = 0;
  while (i < len) {
    i = getISO(str, i, &code);
    ch = code - 32;
    if ((ch >= 0) && (ch < 224)) {
      width = width + charwidth[ch];
    } else {
      width = width + charwidth[0]; /* approximate with space */
    };
  };
  return(width*ptsize/baseptsize);
}

void setscaling(char* s)
/* scaling value from string following -s in arglist */
{
  double f;

  f = -1.0;
  sscanf(s, "%lf", &f);
  if (f < 0.001) {
    f = TUNE_SCALING;
  };
  scale = f;
}

void setmargins(s)
char* s;
/* set margin values from string following -M in arglist */
{
  int i, j;

  xmargin = XMARGIN;
  ymargin = YMARGIN;
  if (s != NULL) {
    i = -1;
    j = -1;
    sscanf(s, "%dx%d", &i, &j);
    if ((i != -1) && (j != -1)) {
      xmargin = i;
      ymargin = j;
    };
  };
}

void setpagesize(s)
char* s;
/* set page size from string following -P in arglist */
{
  int i, j;

  pagelen = A4_PAGELEN - 2 * ymargin;
  pagewidth = A4_PAGEWIDTH - 2 * xmargin;
  if (s != NULL) {
    i = 0;
    j = 0;
    sscanf(s, "%dx%d", &i, &j);
    if (i == 0) {
      pagelen = A4_PAGELEN - 2 * ymargin;
      pagewidth = A4_PAGEWIDTH - 2 * xmargin;
    };
    if (i == 1) {
      pagelen = US_LETTER_PAGELEN - 2 * ymargin;
      pagewidth = US_LETTER_PAGEWIDTH - 2 * xmargin;
    };
    if ((i > 100) && (i > 2*xmargin) && (j > 100) && (j > 2*ymargin)) {
      pagewidth = i - 2 * xmargin;
      pagelen = j - 2 * ymargin;
    };
  };
  if (landscape) {
    i = xmargin;
    xmargin = ymargin;
    ymargin = i;
    i = pagelen;
    pagelen = pagewidth;
    pagewidth = i;
  };
  scaledlen = ((double)(pagelen))/scale;
  scaledwidth = ((double)(pagewidth))/scale;
}

static void set_space(afont, s)
/* set vertical space to appear above a line in the given font */
/* s is a string specifying space in points (default), centimetres */
/* or inches */
struct font* afont;
char* s;
{
  int count;
  double x;
  char units[40];

  count = sscanf(s, "%lf%s", &x, units);
  if (count > 0) {
    if ((count >= 2) && (strncmp(units, "cm", 2) == 0)) {
      x = x*28.3;
    };
    if ((count >= 2) && (strncmp(units, "in", 2) == 0)) {
      x = x*72.0 ;
    };
    afont->space = (int)x;
  };
}

static void init_font(afont, ptsize, spce, defnum, specialnum)
/* initialize font structure with given values */
struct font* afont;
int ptsize, spce, defnum, specialnum;
{
  afont->pointsize = ptsize;
  afont->space = spce;
  afont->default_num = defnum;
  afont->special_num = specialnum;
  afont->name = NULL;
  afont->defined = 0;
}

static void startpage()
/* Encapsulated PostScript for a page header */
{
  fprintf(f, "%%%%Page: %d %d\n", pagecount, pagecount);
  fprintf(f, "%%%%BeginPageSetup\n");
  fprintf(f, "gsave\n");
  if (landscape) {
    fprintf(f, "90 rotate %d %d T\n", xmargin, -ymargin);
  } else {
    fprintf(f, "%d %d T\n", xmargin, ymargin+pagelen);
  };
  fprintf(f, "0.8 setlinewidth 0 setlinecap\n");
  fprintf(f, "%.3f %.3f scale\n", scale, scale);
  fprintf(f, "%%%%EndPageSetup\n\n");
  fontnum = 0;
  fontsize = 0;
  totlen = 0.0;
  oldplace = 0.0;
  descend = 0.0;
}

static void closepage()
/* Encapsulated PostScript for page end */
{

  if (pagenumbering) {
    setfont(12, 3);
    pagebottom();
    fprintf(f, "(%d) %.1f 0 M cshow\n", pagecount, scaledwidth/2.0);
  };
  fprintf(f, "%%%%PageTrailer\n");
  fprintf(f, "grestore\n");
  fprintf(f, "showpage\n\n");
  pagecount = pagecount + 1;
}

void newpage()
/* create PostScript for a new page of paper */
/* everything will now be printed on this fresh sheet */
/* ignore the command if the sheet is completely blank */
{
  if (totlen > 0.0) {
    closepage();
    startpage();
  };
}

static void pagebottom()
/* move to the bottom of the page */
{
  fprintf(f, "0 %.1f T\n", -(scaledlen - totlen));
  totlen = scaledlen;
  descend = 0.0;
}

static void newblock(double height, double descender)
/* The page is modelled as a series of vertical bands */
/* This routine finds room for a fresh band below the current one */
/* It also does a translation to set a new y=0 line */
/* Subsequent calls may draw up to height units above or */
/* descender units below the line y=0 */
{
  if (totlen+(descend+height+descender) > scaledlen - (double)(pagenumbering*12)) {
    newpage();
  };
  fprintf(f, "0 %.1f T\n", - descend - height);
  totlen = totlen + (descend + height);
  descend = descender;
}

static void staveline()
/* draw 5 lines of a stave */
{
  fprintf(f, "%.1f staff\n", scaledwidth);
}

static void printclef (cleftype_t * t, double x, double yup,
                       double ydown)
/* draw a clef of the specified type */
{
  double y;
  int num_to_show;
  double clef_y;
  double clef_ytop;
  double clef_ybot;

  y = (TONE_HT * 2) * (t->staveline - 1);
  switch (t->basic_clef) {
    case basic_clef_treble:
    case basic_clef_auto:
    case basic_clef_perc:
    case basic_clef_none:
      clef_y = y - (TONE_HT * 2);
      fprintf(f, "gsave 0 %.1f T\n", clef_y);
      fprintf(f, "%.1f tclef grestore\n", x);
      clef_ytop = clef_y + (TONE_HT * 11);
      clef_ybot = clef_y - (TONE_HT * 4);
      break;
    case basic_clef_bass:
      clef_y = y - (TONE_HT * 6);
      fprintf(f, "gsave 0 %.1f T\n", clef_y);
      fprintf(f, "%.1f bclef grestore\n", x);
      clef_ytop = clef_y + (TONE_HT * 8);
      clef_ybot = clef_y - (TONE_HT * 0);
      break;
    case basic_clef_alto:
      clef_y = y - (TONE_HT * 4);
      fprintf(f, "gsave 0 %.1f T\n", clef_y);
      fprintf(f, "%.1f cclef grestore\n", x);
      clef_ytop = clef_y + (TONE_HT * 8);
      clef_ybot = clef_y - (TONE_HT * 0);
      break;
    default:
      break;
  }
  /* make sure we don't draw within staves */
  if (clef_ybot > 0.0) {
    clef_ybot = 0.0;
  }
  if (clef_ytop < (TONE_HT * 8)) {
    clef_ytop = TONE_HT * 8;
  }
  //do_T (out, 0, -y);

  /* draw -15, -8, +8, +15 above or below clef */
  switch (t->octave_offset) {
    case -2:
      num_to_show = -15;
      break;
    case -1:
      num_to_show = -8;
      break;
    case 1:
      num_to_show = 8;
      break;
    case 2:
      num_to_show = 15;
      break;
    default:
      num_to_show = 0;
      break;
  }
  if (t->octave_offset > 0) {
    fprintf(f, "%.1f %.1f (+%d) bnum\n", x, clef_ytop + 2, num_to_show);
  }
  if (t->octave_offset < 0) {
    fprintf(f, "%.1f %.1f (%d) bnum\n", x, clef_ybot - CLEFNUM_HT - 2, num_to_show);
  }
}

void set_keysig(struct key* k, struct key* newval)
/* copy across key signature */
{
  int i;

  for (i=0; i<7; i++) {
    k->map[i] = newval->map[i];
    k->mult[i] = newval->mult[i];
  };
  k->sharps = newval->sharps;
}

static double size_keysig(char oldmap[], char newmap[])
/* compute width taken up by drawing the key signature */
{
  int i, n;
 
  n = 0;
  for (i=0; i<7; i++) {
    if (((newmap[i] == '=')&&(oldmap[i] != '=')) ||
        (newmap[i] == '^') || (newmap[i] == '_')) {
       n = n + 1;
    };
  };
  return((double)n * 5.0);
}

static double size_timesig(struct fract* meter)
/* compute width of the time signature */
{
  double len1, len2;
  char temp[20];

  sprintf(temp, "%d", meter->num);
  len1 = stringwidth(temp, 16.0, 2);
  sprintf(temp, "%d", meter->denom);
  len2 = stringwidth(temp, 16.0, 2);
  if (len1 > len2) {
    return(len1);
  } else {
    return(len2);
  };
}

static void draw_keysig(char oldmap[], char newmap[], int newmult[], 
            double x, cleftype_t* clef)
/* draw key specified key signature at position x on line           */
/* arrays oldmap[], newmap[], newmult[] are indexed with a=0 to g=7 */
/* sharp_pos[] and flat_pos[] give order of notes                   */
/* e.g. sharp_pos[0] = 8 means 1st sharp drawn is conventionally f  */
/* which is in position 8 on the stave (bottom line is E = 0)       */
{
  double xpos;
  static int sharp_pos[7] = { 8, 5, 9, 6, 3, 7, 4 };
  static int flat_pos[7] =  { 4, 7, 3, 6, 2, 5, 1};
  int i;
  int offset, pos;
  int note;
 
  switch (clef->basic_clef) {
    case basic_clef_treble:
    case basic_clef_auto:
    case basic_clef_perc:
    case basic_clef_none:
      offset = 0 + (clef->staveline - 2) * 2;
      break;
    case basic_clef_alto:
      offset = 6 + (clef->staveline - 3) * 2;
      break;
    case basic_clef_bass:
      offset = 12 + (clef->staveline - 6) * 2;
      break;
    case basic_clef_undefined:
      break;
  }
  xpos = x;
  /* draw naturals to cancel out old accidentals */
  for (i=0; i<7; i++) {
    note = (sharp_pos[i] + 4) % 7;
    if ((newmap[note] == '=')&&(oldmap[note] != '=')) {
      pos = (sharp_pos[i] + offset - 3) % 7 + 3;
      fprintf(f, " %.1f %d nt0", xpos, pos*TONE_HT);
      xpos = xpos + 5;
    };
  };
  /* draw sharps */
  for (i=0; i<7; i++) {
    note = (sharp_pos[i] + 4) % 7;
    if (newmap[note] == '^') {
      pos = (sharp_pos[i] + offset - 3) % 7 + 3;
      if (newmult[note] == 2) {
        fprintf(f, " %.1f %d dsh0", xpos, pos*TONE_HT);
      } else {
        fprintf(f, " %.1f %d sh0", xpos, pos*TONE_HT);
      };
      xpos = xpos + 5;
    };
  };
  /* draw flats */
  for (i=0; i<7; i++) {
    note = (flat_pos[i] + 4) % 7;
    if (newmap[note] == '_') {
      pos = (flat_pos[i] + offset - 1)%7 + 1;
      if (newmult[note] == 2) {
        fprintf(f, " %.1f %d dft0", xpos, pos*TONE_HT);
      } else {
        fprintf(f, " %.1f %d ft0", xpos, pos*TONE_HT);
      };
      xpos = xpos + 5;
    };
  };
  fprintf(f, "\n");
}

static void draw_meter(struct fract* meter, double x)
/* draw meter (time signature) at specified x value */
{
  fprintf(f, "%.1f (%d) (%d) tsig\n", x, meter->num, meter->denom);
}

static double maxstrwidth(struct llist* strings, double ptsize, int fontno)
/* return the width of longest string in the list */
{
  double width, max;
  char* str;

  max = 0.0;
  str = firstitem(strings);
  while (str != NULL) {
    width = stringwidth(str, ptsize, fontno);
    if (width > max)  {
      max = width;
    };
    str = nextitem(strings);
  };
  return(max);
}

static void sizerest(struct rest* r, struct feature* ft)
/* compute width of rest element */
{
  double width;

  if (r->multibar > 0) {
    ft->xleft = 0.0;
    ft->xright = 40.0;
  } else {
    switch (r->base_exp) { 
    case 1:
    case 0:
      ft->xleft = 2.5;
      ft->xright = 2.5;
      break;
    case -1:
      ft->xleft = 2.5;
      ft->xright = 2.5;
      break;
    case -2:
      ft->xleft = 3.0;
      ft->xright = 4.0;
      break;
    case -3:
      ft->xleft = 4.0;
      ft->xright = 3.0;
      break;
    case -4:
      ft->xleft = 5.0;
      ft->xright = 3.0;
      break;
    case -5:
      ft->xleft = 6.0;
      ft->xright = 5.0;
      break;
    case -6:
    default:
      ft->xleft = 7.0;
      ft->xright = 5.0;
      break;
    };
    if (r->dots > 0) {
      ft->xright = ft->xright + (r->dots* (float) DOT_SPACE);
    };
  };
  if (r->gchords != NULL) {
    width = maxstrwidth(r->gchords, (double)(gchordfont.pointsize),
                                     gchordfont.default_num);
    if (width > ft->xleft + ft->xright) {
      ft->xright = (float) width - ft->xleft;
    };
  };
}

static int acc_upsize(char ch)
/* returns height above note of accent */
{
  int size;

  switch (ch) {
  case '=':
    size = NAT_UP;
    break;
  case '^':
    size = SH_UP;
    break;
  case '_':
    size = FLT_UP;
    break;
  default:
    size = 0;
    break;
  };
  return(size);
}

static int acc_downsize(char ch)
/* returns depth below note of accent */
{
  int size;

  switch (ch) {
  case '=':
    size = NAT_DOWN;
    break;
  case '^':
    size = SH_DOWN;
    break;
  case '_':
    size = FLT_DOWN;
    break;
  default:
    size = 0;
    break;
  };
  return(size);
}

static void setstemlen(struct note* n, int ingrace)
/* work out how long stem needs to be for an isolated note */
/* if the note is in a beamed set, the length may be altered later */
{
  if (n->base_exp >= 0) {
    n->stemlength = 0.0;
  } else {
    if (ingrace) {
      if (n->beaming != single) {
        n->stemlength = GRACE_STEMLEN; /* default stem length */
        /* value will be recalculated later by setbeams() */
      } else {
        switch(n->base_exp) {
        case -6:
          n->stemlength = (float) GRACE_STEMLEN + 2* (float) TAIL_SEP* (float) 0.7;
          break;
        case -5:
          n->stemlength = (float) GRACE_STEMLEN + (float) TAIL_SEP* (float) 0.7;
          break;
        default:
          n->stemlength = (float) GRACE_STEMLEN; /* default stem length */
          break;
        };
      };
    } else {
      if (n->beaming != single) {
        n->stemlength = STEMLEN; /* default stem length */
        /* value will be recalculated later by setbeams() */
      } else {
        switch(n->base_exp) {
        case -6:
          n->stemlength = (float) STEMLEN + 2* (float) TAIL_SEP;
          break;
        case -5:
          n->stemlength = (float) STEMLEN + (float) TAIL_SEP;
          break;
        default:
          n->stemlength = (float) STEMLEN; /* default stem length */
          break;
        };
      };
    };
  };
}

static void sizenote(struct note* n, struct feature* f, int ingrace)
/* compute width and height of note element */
{
  double width;
  char* decorators;
  int i;

  f->ydown = (float) TONE_HT*(n->y - 2);
  f->yup = (float) TONE_HT*(n->y + 2);
  if (ingrace) {
    f->xleft = (float) GRACE_HALF_HEAD;
    f->xright = (float) GRACE_HALF_HEAD;
  } else {
    if (n->base_exp >= 1) {
      f->xleft = HALF_BREVE;
      f->xright = HALF_BREVE;
    } else {
      f->xleft = HALF_HEAD;
      f->xright = HALF_HEAD;
    };
    if (n->fliphead) {
      if (n->stemup) {
        f->xright = (float) f->xright + (float) HALF_HEAD * 2;
      } else {
        f->xleft = f->xleft + (float) HALF_HEAD * 2;
      };
    };
  };
  if (n->dots > 0) {
    f->xright = f->xright + (n->dots* (float) DOT_SPACE);
  };
  if ((n->stemup) && (n->base_exp <= -3) && (n->beaming==single)) {
    if (f->xright < (HALF_HEAD + TAILWIDTH)) {
      f->xright = HALF_HEAD + TAILWIDTH;
    };
  };
  if (n->accidental != ' ') {
    f->xleft = (float) HALF_HEAD + (float) ACC_OFFSET + (n->acc_offset-1) * (float) ACC_OFFSET2;
    if (n->stemup) {
      f->ydown = (float) TONE_HT*(n->y) - (float)(acc_downsize(n->accidental));
    } else {
      f->yup = (float) ((float) TONE_HT*(n->y) + (double)(acc_upsize(n->accidental)));
    };
  };
  if (n->stemlength > 0.0) {
    if (n->stemup) {
      f->yup = TONE_HT*(n->y) + n->stemlength;
    } else {
      f->ydown = TONE_HT*(n->y) - n->stemlength;
    };
  };
  if (n->accents != NULL) {
    decorators = n->accents;
    for (i=0; i<(int) strlen(decorators); i++) {
      switch(decorators[i]) {
      case 'H':
      case '~':
      case 'u':
      case 'v':
      case 'T':
        f->yup = f->yup + BIG_DEC_HT;
        break;
      case 'R':
        if (n->stemup) {
          f->ydown = f->ydown + BIG_DEC_HT;
        } else {
          f->yup = f->yup + BIG_DEC_HT;
        };
        break;
      case 'M':
      case '.':
        if (n->stemup) {
          f->ydown = f->ydown + SMALL_DEC_HT;
        } else {
          f->yup = f->yup + SMALL_DEC_HT;
        };
        break;
      default:
        /* this should never happen */
        event_error("Un-recognized accent");
        break;
      };
    };
  };
  if (n->syllables != NULL) {
    width = maxstrwidth(n->syllables, (double)(vocalfont.pointsize),
                                      vocalfont.default_num);
    if (width > f->xleft + f->xright) {
      f->xright = (float) width - f->xleft;
    };
  };
  if (n->gchords != NULL) {
    width = maxstrwidth(n->gchords, (double)(gchordfont.pointsize),
                         gchordfont.default_num);
    if (width > f->xleft + f->xright) {
      f->xright = (float) width - f->xleft;
    };
  };
}

static void spacechord(struct feature* chordplace)
/* prevents collision of note heads and accidentals in a chord */
/* sets fliphead and acc_offset for all notes in chord */
{
  struct feature* place;
  struct note* anote;
  int thisy, lasty, lastflip;
  int stemdir =0;  /* [SDG] 2020-06-03 */
  int doneflip;
  int ygap[10];
  int accplace;
  int i;

  /* first flip heads */
  place = chordplace;
  lasty = 200;
  lastflip = 1;
  doneflip = 0;
  while ((place != NULL) && (place->type != CHORDOFF)) {
    if ((place->type == CHORDNOTE) || (place->type == NOTE)) {
      anote = place->item.voidptr;
      thisy = anote->y;
      if ((lasty - thisy <= 1) && (lastflip != 1)) {
        anote->fliphead = 1;
        stemdir = anote->stemup;
        lastflip = 1;
        doneflip = 1;
      } else {
        lastflip = 0;
      };
      lasty = thisy;
    };
    place = place->next;
  };
  /* now deal with accidentals */
  for (i=0; i<10; i++) {
    ygap[i] = 200;
  };
  if ((doneflip) && (stemdir == 0)) {
    /* block off space where note is */
    ygap[0] = -200;
  };
  place = chordplace;
  while ((place != NULL) && (place->type != CHORDOFF)) {
    if ((place->type == CHORDNOTE) || (place->type == NOTE)) {
      anote = place->item.voidptr;
      thisy = anote->y;
      if (anote->accidental != ' ') {
        /* find space for this accidental */
        accplace = 0;
        while ((accplace <10) && (ygap[accplace] < thisy)) {
          accplace = accplace + 1;
        };
        anote->acc_offset = accplace + 1;
        ygap[accplace] = thisy - 6;
      };
    };
    place = place->next;
  };
}

static void sizechord(struct chord* ch, int ingrace)
/* decide on stem direction and set default stem length for a chord */
{
  if ((ch->ytop + ch->ybot < 8)||(ingrace)) {
    ch->stemup = 1;
  } else {
    ch->stemup = 0;
  };
  ch->stemlength = STEMLEN;  
}

static void setxy(double* x, double* y, struct note* n, struct feature* ft, 
      double offset, double half_head)
/* compute x,y co-ordinates above note for drawing a beam */
{
  if (n->stemup) {
    *x = ft->x + half_head;
    *y = (double)(TONE_HT*n->y) + n->stemlength - offset;
  } else {
    *x = ft->x - half_head;
    *y = (double)(TONE_HT*n->y) - n->stemlength + offset;
  };
}

static void drawtuple(struct feature* beamset[], int beamctr, int tupleno)
/* label a beam which is a tuplet of notes (e.g. triplet) */
{
  double x0, x1, y0, y1;
  struct note* n;
  int stemup;

  x0 = beamset[0]->x;
  n = beamset[0]->item.voidptr;
  stemup = n->stemup;
  if (stemup) {
    y0 = (double)(TONE_HT*n->y) + n->stemlength;
  } else {
    y0 = (double)(TONE_HT*n->y) - n->stemlength;
  };
  x1 = beamset[beamctr-1]->x;
  n = beamset[beamctr-1]->item.voidptr;
  if (stemup) {
    y1 = (double)(TONE_HT*n->y) + n->stemlength;
  } else {
    y1 = (double)(TONE_HT*n->y) - n->stemlength;
  };
  if (stemup) {
    fprintf(f, " %.1f %.1f (%d) bnum", (x0+x1)/2, (y0+y1)/2 + TUPLE_UP, tupleno);
  } else {
    fprintf(f, " %.1f %.1f (%d) bnum", (x0+x1)/2, (y0+y1)/2 + TUPLE_DOWN, 
           tupleno);
  };
}

static void drawhtuple(double xstart, double xend, int num, double y)
/* draw a tuple using half-brackets */
/* used for tuples which do not correspond to a beam */
{
  double xmid;

  xmid = (xstart + xend)/2;
  fprintf(f, " %.1f %.1f %.1f %.1f hbr", xstart, y, xmid-6.0, y);
  fprintf(f, " %.1f %.1f %.1f %.1f hbr", xend, y, xmid+6.0, y);
  fprintf(f, " %.1f %.1f (%d) bnum\n", xmid, y-4.0, num);
}

static void drawbeam(struct feature* beamset[], int beamctr, int dograce)
/* draw a beam spanning the notes in array beamset[] */
{
  int i, d;
  int donenotes;
  int start, stop;
  double x0, x1, y0, y1, offset;
  struct note* n;
  int stemup, beamdir;
  double half_head;

  if (beamctr==0) {
    event_error("Internal error: beam with 0 notes");
    showtune(&thetune);
    return;
    /* exit(0); [SS] 2017-11-17 */
  };
  if (beamset[0]->type != NOTE) {
    event_error("Internal error: beam does not start with NOTE");
    exit(0);
  };
  fprintf(f, "\n");
  if (redcolor) fprintf(f,"1.0 0.0 0.0 setrgbcolor\n");
  n = beamset[0]->item.voidptr;
  stemup = n->stemup;
  beamdir = 2*stemup - 1;
  donenotes = 1;
  d = -3;
  offset = 0.0;
  if (dograce) {
    half_head = GRACE_HALF_HEAD;
  } else {
    half_head = HALF_HEAD;
  };
  while (donenotes) {
    donenotes = 0;
    start = -1;
    for (i=0; i<beamctr; i++) {
      n = beamset[i]->item.voidptr;
      if (n->base_exp <= d) {
        if (start == -1) {
          start = i;
          setxy(&x0, &y0, n, beamset[i], offset, half_head);
        };
        stop = i;          
        setxy(&x1, &y1, n, beamset[i], offset, half_head);
      };
      if ((n->beaming == endbeam) || (n->base_exp > d)) {
        if (start != -1) {
          /* draw unit */
          if (start == stop) {
            if (start != 0) {
              /* half line in front of note */
              n = beamset[start-1]->item.voidptr;
              setxy(&x0, &y0, n, beamset[start-1], offset, half_head);
              x0 = x0 + (x1-x0)/2;
              y0 = y0 + (y1-y0)/2;
            } else {
              /* half line behind note */
              n = beamset[start+1]->item.voidptr;
              setxy(&x1, &y1, n, beamset[start+1], offset, half_head);
              x1 = x1 + (x0-x1)/2;
              y1 = y1 + (y0-y1)/2;
            };
            if (dograce) {
              fprintf(f, "%.1f %.1f %.1f %.1f gbm2\n", x0, y0, x1, y1);
            } else {
              fprintf(f, "%.1f %.1f %.1f %.1f %.1f bm\n", x0, y0, x1, y1,
                       (double)(beamdir * TAIL_WIDTH));
            };
          } else {
            if (dograce) {
              fprintf(f, "%.1f %.1f %.1f %.1f gbm2\n", x0, y0, x1, y1);
            } else {
              fprintf(f, "%.1f %.1f %.1f %.1f %.1f bm\n", x0, y0, x1, y1,
                       (double)(beamdir * TAIL_WIDTH));
            };
          };
          donenotes = 1;
          start = -1;
        };
      };
    };
    d = d - 1;
    offset = offset + TAIL_SEP;
  };
  if (redcolor) fprintf(f,"0 setgray\n");
}

static void sizeclef(cleftype_t *theclef, struct feature *ft)
{
  double y;
  double clef_y;
  double clef_ytop;
  double clef_ybot;

  /* take account of staveline when calculating clef size */
  y = (TONE_HT * 2) * (theclef->staveline - 1);
  switch (theclef->basic_clef) {
    case basic_clef_treble:
    case basic_clef_auto:
    case basic_clef_perc:
    case basic_clef_none:
    default:
      clef_y = y - (TONE_HT * 2);
      clef_ytop = clef_y + (TONE_HT * 11);
      clef_ybot = clef_y - (TONE_HT * 6);
      ft->xright = TREBLE_RIGHT;
      ft->xleft = TREBLE_LEFT;
      break;
    case basic_clef_bass:
      clef_y = y - (TONE_HT * 6);
      clef_ytop = clef_y + (TONE_HT * 8);
      clef_ybot = clef_y - (TONE_HT * 0);
      ft->xright = BASS_RIGHT;
      ft->xleft = BASS_LEFT;
      break;
    case basic_clef_alto:
      clef_y = y - (TONE_HT * 4);
      clef_ytop = clef_y + (TONE_HT * 8);
      clef_ybot = clef_y - (TONE_HT * 0);
      ft->xright = CCLEF_RIGHT;
      ft->xleft = CCLEF_LEFT;
      break;
  }
  /* extend limits to top and bottom of stave if necessary */
  if (clef_ybot > 0.0)
  {
    ft->ydown = 0.0;
  }
  if (clef_ytop < (TONE_HT * 8))
  {
    clef_ytop = TONE_HT * 8;
  }
  /* allow extra space for +8, -8 etc */
  if (theclef->octave_offset > 0) {
    clef_ytop = clef_ytop + 2 + CLEFNUM_HT + 2;
  }
  if (theclef->octave_offset < 0) {
    clef_ybot = clef_ybot - CLEFNUM_HT - 2;
  }
  ft->yup = clef_ytop;
  ft->ydown = clef_ybot;
}

static void sizevoice(struct voice* v, struct tune* t)
/* compute width and height values for all elements in voice */
{
  struct feature* ft;
  struct note* anote;
  struct key* akey;
  cleftype_t* theclef;
  char* astring;
  struct fract* afract;
  struct rest* arest;
  struct note* lastnote;
  struct chord* thischord;
  struct feature* chordplace;
  struct tuple* thistuple;
  enum tail_type chordbeaming;
  int intuple, tuplecount;
  struct feature* tuplefeature;

  ingrace = 0;
  inchord = 0;
  intuple = 0;
  tuplefeature = NULL;
  chordhead = NULL;
  thischord = NULL;
  chordplace = NULL;
  copy_clef (v->clef, &t->clef);
  if (v->keysig == NULL) {
    event_error("Voice has no key signature");
  };
  ft = v->first;
  lastnote = NULL;
  while (ft != NULL) {
    ft->xleft = 0.0;
    ft->xright = 0.0;
    ft->ydown = 0.0;
    ft->yup = 0.0;
    switch (ft->type) {
    case SINGLE_BAR: 
      ft->xleft = (float) 0.8;
      ft->xright = 0.0;
      break;
    case DOUBLE_BAR: 
      ft->xleft = 3.0;
      ft->xright = 0.0;
      break;
    case BAR_REP: 
      ft->xleft = 1.0;
      ft->xright = 10.0;
      break;
    case REP_BAR: 
      ft->xleft = 1.0;
      ft->xright = 10.0;
      break;
    case REP1: 
      break;
    case REP2: 
      break;
    case PLAY_ON_REP:
      break;
    case BAR1: 
      break;
    case REP_BAR2: 
      ft->xleft = 6.0;
      ft->xright = 5.0;
      break;
    case DOUBLE_REP: 
      ft->xleft = 10.0;
      ft->xright = 10.0;
      break;
    case THICK_THIN: 
      ft->xleft = 0.0;
      ft->xright = 6.0;
      break;
    case THIN_THICK: 
      ft->xleft = 6.0;
      ft->xright = 0.0;
      break;
    case PART: 
      astring = ft->item.voidptr;
    case TEMPO: 
      break;
    case TIME: 
      afract = ft->item.voidptr;
      if (afract == NULL) {
        afract = &v->meter;
      };
      ft->xleft = 0;
      ft->xright = (float) size_timesig(afract);
      break;
    case KEY: 
      ft->xleft = 0;
      akey = ft->item.voidptr;
      ft->xright = (float) size_keysig(v->keysig->map, akey->map);
      set_keysig(v->keysig, akey);
      break;
    case REST: 
      arest = ft->item.voidptr;
      sizerest(arest, ft);
      if (intuple) {
        if (ft->yup > thistuple->height) {
          thistuple->height = ft->yup;
        };
        tuplecount = tuplecount - 1;
        if (tuplecount <= 0) {
          tuplefeature->yup = thistuple->height + HTUPLE_HT;
          intuple = 0;
          thistuple = NULL;
          tuplefeature = NULL;
        };
      };
      break;
    case TUPLE: 
      thistuple = ft->item.voidptr;
      if (thistuple->beamed == 0) {
        intuple = 1;
        tuplecount = thistuple ->r;
        thistuple->height = (double)(10*TONE_HT);
        tuplefeature = ft;
      };
      break;
    case NOTE: 
      anote = ft->item.voidptr;
      setstemlen(anote, ingrace);
      sizenote(anote, ft, ingrace);
      if (inchord) {
        if (chordcount == 0) {
          chordhead = ft;
          thischord->ytop = anote->y;
          thischord->ybot = anote->y;
          chordbeaming = anote->beaming;
        } else {
          if (anote->y > thischord->ytop) {
            thischord->ytop = anote->y;
          };
          if (anote->y < thischord->ybot) {
            thischord->ybot = anote->y;
          };
          if (ft->xleft > chordhead->xleft) {
            chordhead->xleft = ft->xleft;
          };
          if (ft->xright > chordhead->xright) {
            chordhead->xright = ft->xright;
          };
          if (ft->yup > chordhead->yup) {
            chordhead->yup = ft->yup;
          };
          if (ft->ydown > chordhead->ydown) {
            chordhead->ydown = ft->ydown;
          };
          ft->type = CHORDNOTE;
        };
        chordcount = chordcount + 1;
      };
      if (intuple) {
        if (ft->yup > thistuple->height) {
          thistuple->height = ft->yup;
        };
        tuplecount = tuplecount - 1;
        if (tuplecount <= 0) {
          tuplefeature->yup = thistuple->height + HTUPLE_HT;
          intuple = 0;
          thistuple = NULL;
          tuplefeature =NULL;
        };
      };
      break;
    case NONOTE: 
      break;
    case OLDTIE:
      break;
    case TEXT:
      break;
    case SLUR_ON: 
      break;
    case SLUR_OFF: 
      break;
    case TIE: 
      break;
    case CLOSE_TIE:
      break;
    case TITLE: 
      break;
    case CHANNEL:
      break;
    case TRANSPOSE:
      break;
    case RTRANSPOSE:
      break;
    case GRACEON:
      ingrace = 1;
      break;
    case GRACEOFF:
      ingrace = 0;
      break;
    case SETGRACE:
      break;
    case SETC:
      break;
    case GCHORD: 
      break;
    case GCHORDON: 
      break;
    case GCHORDOFF:
      break;
    case VOICE:
      break;
    case CHORDON:
      inchord = 1;
      chordcount = 0;
      thischord = ft->item.voidptr;
      chordplace = ft;
      spacechord(chordplace);
      break;
    case CHORDOFF:
      if (thischord != NULL) {
        anote = chordhead->item.voidptr;
        thischord->beaming = chordbeaming;
        if (thischord->beaming == single) {
          sizechord(thischord, ingrace);
        };
      };
      inchord = 0;
      chordhead = NULL;
      thischord = NULL;
      chordplace = NULL;
      break;
    case SLUR_TIE:
      break;
    case TNOTE:
      break;
    case LT:
      break;
    case GT:
      break;
    case DYNAMIC:
      break;
    case LINENUM:
      lineno = ft->item.number;
      break;
    case MUSICLINE: 
      break;
    case MUSICSTOP:
      break;
    case WORDLINE:
      break;
    case WORDSTOP:
      break;
    case INSTRUCTION:
      break;
    case NOBEAM:
      break;
    case CLEF:
      theclef = ft->item.voidptr;
      if (theclef == NULL) {
        theclef = v->clef;
      }
      sizeclef(theclef, ft);
      copy_clef (v->clef, theclef);
      break;
    case PRINTLINE:
      break;
    case NEWPAGE:
      break;
    case LEFT_TEXT:
      break;
    case CENTRE_TEXT:
      break;
    case VSKIP:
      break;
    case SPLITVOICE:
      break;
    default:
      printf("unknown type %d\n", ft->type);
      break;
    };
    ft = ft->next;
  };
}

static void sizetune(struct tune* t)
/* compute width and height values for all elements in the tune */
{
  struct voice* v;

  v = firstitem(&t->voices);
  while (v != NULL) {
    sizevoice(v, t);
    v = nextitem(&t->voices);
  };
}

static void singlehead(double x, double y, int base, int base_exp, int dots)
/* draws a note head */
{
  int i;
  double dot_offset;

  fprintf(f, "%.1f %.1f ", x, y);
  /* note head */
  dot_offset = HALF_HEAD;
  switch(base_exp) {
  case 1:
    fprintf(f, "BHD");
    dot_offset = HALF_BREVE;
    break; 
  case 0:
    fprintf(f, "HD");
    break; 
  case -1:
    fprintf(f, "Hd");
    break; 
  default:
    if (base_exp > 1) {
      fprintf(f, "BHD");
      dot_offset = HALF_BREVE;
      event_warning("Note value too long to represent");
    } else {
      fprintf(f, "hd");
    };
    break; 
  };
  if (dots == -1) {
    event_warning("Note value cannot be represented");
  };
  for (i=1; i <= dots; i++) {
    fprintf(f, " %.1f 3.0 dt", (double)(dot_offset+DOT_SPACE*i)); 
  };
}

static void drawhead(struct note* n, double x, struct feature* ft)
/* draw just the head of a note together with any decorations */
/* and accidentals */
{
  int i;
  char* decorators;
  double ytop, ybot;
  double notex, accspace;
  int offset;

  if (n->fliphead) {
    if (n->stemup) {
      notex = x + 2.0 * HALF_HEAD;
    } else {
      notex = x - 2.0 * HALF_HEAD;
    }
  } else {
    notex = x;
  };
  singlehead(notex, (double)(TONE_HT*n->y), n->base, n->base_exp, n->dots);
  if (n->acc_offset == 0) {
    accspace = ACC_OFFSET;
  } else {
   if (n->fliphead) {
     /* compensate for flipped note head */
     if (n->stemup) {
       offset = n->acc_offset;
     } else {
       offset = n->acc_offset - 2;
     };
   } else {
     offset = n->acc_offset - 1;
   };   
   accspace = ACC_OFFSET + ACC_OFFSET2 * offset;
  };
  switch (n->accidental) {
  case '=':
    fprintf(f, " %.1f nt", accspace);
    break;
  case '^':
    if (n->mult == 1) {
      fprintf(f, " %.1f sh", accspace);
    } else {
      fprintf(f, " %.1f dsh", accspace);
    };
    break;
  case '_':
    if (n->mult == 1) {
      fprintf(f, " %.1f ft", accspace);
    } else {
      fprintf(f, " %.1f dft", accspace);
    };
    break;
  default:
    break;    
  };
  i = 10;
  while (n->y >= i) {
    fprintf(f, " %d hl", i*TONE_HT);
    i = i + 2;
  };
  i = -2;
  while (n->y <= i) {
    fprintf(f, " %d hl", i*TONE_HT);
    i = i - 2;
  };
  if (n->accents != NULL) {
    decorators = n->accents;
    ytop = (n->y + 2) * TONE_HT;
    ybot = (n->y - 2) * TONE_HT;
    if (n->stemup) {
      ytop = ytop + n->stemlength;
    } else {
      ybot = ybot - n->stemlength;
    };
    for (i=0; i< (int) strlen(decorators); i++) {
      switch (decorators[i]) {
      case  '.':
        if (n->stemup) {
          fprintf(f, " %.1f stc", ybot+STC_OFF);
          ybot = ybot - SMALL_DEC_HT;
        } else {
          fprintf(f, " %.1f stc", ytop+STC_OFF);
          ytop = ytop + SMALL_DEC_HT;
        };
        break;
      case  'R':
        if (n->stemup) {
          fprintf(f, " %.1f cpd", ybot+CPD_OFF);
          ybot = ybot - SMALL_DEC_HT;
        } else {
          fprintf(f, " %.1f cpu", ytop+CPU_OFF);
          ytop = ytop + SMALL_DEC_HT;
        };
        break;
      case  'M':
        if (n->stemup) {
          fprintf(f, " %.1f emb", ybot+EMB_OFF);
          ybot = ybot - SMALL_DEC_HT;
        } else {
          fprintf(f, " %.1f emb", ytop+EMB_OFF);
          ytop = ytop + SMALL_DEC_HT;
        };
        break;
      case 'H':
        fprintf(f, " %.1f hld", ytop+HLD_OFF);
        ytop = ytop + BIG_DEC_HT;
        break;
      case '~':
        fprintf(f, " %.1f grm", ytop+GRM_OFF);
        ytop = ytop + BIG_DEC_HT;
        break;
      case 'u':
        fprintf(f, " %.1f upb", ytop+UPB_OFF);
        ytop = ytop + BIG_DEC_HT;
        break;
      case 'v':
        fprintf(f, " %.1f dnb", ytop+DNB_OFF);
        ytop = ytop + BIG_DEC_HT;
        break;
      case 'T':
        fprintf(f, " %.1f trl", ytop+TRL_OFF);
        ytop = ytop + BIG_DEC_HT;
        break;
      default:
        break;
      };
    };
  };
  fprintf(f, "\n");
}

static void drawgracehead(struct note* n, double x, struct feature* ft, 
                          enum tail_type tail)
/* draw the head of a grace note */
{
  int i;
  double y;

  y = (double)(TONE_HT*n->y);
  switch (tail) {
  case nostem:
    /* note head only */
    fprintf(f, "%.1f %.1f gn", x, y);
    break;
  case single:
    /* note head with stem and tail */
    fprintf(f, "%.1f %.1f %.1f gn1", x, y, n->stemlength);
    break;
  case midbeam:
  case startbeam:
  case endbeam:
    /* note head with stem and tail */
    fprintf(f, "%.1f %.1f %.1f gnt", x, y, n->stemlength);
    break;
  };
  switch (n->accidental) {
  case '=':
    fprintf(f, " %.1f %.1f gnt0", x-ACC_OFFSET, y);
    break;
  case '^':
    if (n->mult == 1) {
      fprintf(f, " %.1f %.1f gsh0", x-ACC_OFFSET, y);
    } else {
      fprintf(f, " %.1f %.1f gds0h", x-ACC_OFFSET, y);
    };
    break;
  case '_':
    if (n->mult == 1) {
      fprintf(f, " %.1f %.1f gft0", x-ACC_OFFSET, y);
    } else {
      fprintf(f, " %.1f %.1f gdf0", x-ACC_OFFSET, y);
    };
    break;
  default:
    break;    
  };
  i = 10;
  while (n->y >= i) {
    fprintf(f, " %.1f %.1f ghl", x, (double)i*TONE_HT);
    i = i + 2;
  };
  i = -2;
  while (n->y <= i) {
    fprintf(f, " %.1f %.1f ghl", x, (double)i*TONE_HT);
    i = i - 2;
  };
  fprintf(f, "\n");
}

static void handlegracebeam(struct note *n, struct feature* ft)
/* this is called to set up the variables needed to lay out */
/* beams for grace notes */
{
  switch (n->beaming) {
  case single:
    gracebeamctr = -1;
    break;
  case startbeam:
    gracebeamset[0] = ft;
    gracebeamctr = 1;
    break;
  case midbeam:
  case endbeam:
    if (gracebeamctr > 0) {
      gracebeamset[gracebeamctr] = ft;
      if (gracebeamctr < 32) {
        gracebeamctr = gracebeamctr + 1;
      } else {
        event_error("Too many notes under one gracebeam");
      };
    } else {
      event_error("Internal beaming error");
      exit(1);
    };
    break;
  case nostem:
    break;
  };
}

static void drawgracenote(struct note* n, double x, struct feature* ft, 
              struct chord* thischord)
/* draw an entire grace note */
{
  handlegracebeam(n, ft);
  drawgracehead(n, x, ft, n->beaming);
  if (n->beaming == endbeam) {
    drawbeam(gracebeamset, gracebeamctr, 1);
  };
  fprintf(f, "\n");
}

static void singletail(int base, int base_exp, int stemup, double stemlength)
/* draw the tail of a note if it has one */
{
  switch(base_exp) {
  case 0:
  case -1:
  case -2:
    break;
  case -3:
    if (stemup) {
      fprintf(f, " %.1f f1u", stemlength);
    } else {
      fprintf(f, " %.1f f1d", stemlength);
    };
    break;
  case -4:
    if (stemup) {
      fprintf(f, " %.1f f2u", stemlength);
    } else {
      fprintf(f, " %.1f f2d", stemlength);
    };
    break;
  case -5:
    if (stemup) {
      fprintf(f, " %.1f f3u", stemlength);
    } else {
      fprintf(f, " %.1f f3d", stemlength);
    };
    break;
  case -6:
    if (stemup) {
      fprintf(f, " %.1f f4u", stemlength);
    } else {
      fprintf(f, " %.1f f4d", stemlength);
    };
    break;
  default:
    if (base_exp < -6) {
      event_warning("Note value too small to represent");
      if (stemup) {
        fprintf(f, " %.1f f4u", stemlength);
      } else {
        fprintf(f, " %.1f f4d", stemlength);
      };
    };
    break;
  };
}

static void drawchordtail(struct chord* ch, double x)
/* draw the tail to a set of notes belonging to a single chord */
{
  if (ch->base > 1) {
    fprintf(f, "%.1f setx ", x);
    if (ch->stemup) {
      fprintf(f, "%.1f sety ", (double)(ch->ybot*TONE_HT));
      fprintf(f, " %.1f su ", ch->stemlength + 
              (double)((ch->ytop - ch->ybot)*TONE_HT));
    } else {
      fprintf(f, "%.1f sety ", (double)(ch->ytop*TONE_HT));
      fprintf(f, " %.1f sd ", ch->stemlength + 
              (double)((ch->ytop - ch->ybot)*TONE_HT));
    };
    if (ch->beaming == single) {
      singletail(ch->base, ch->base_exp, ch->stemup, ch->stemlength+
                   (double)((ch->ytop - ch->ybot)*TONE_HT));
    };
    fprintf(f, "\n");
  };
}

static void showtext(struct llist* textitems, double x, double ystart, double ygap)
/* This routine deals with lyrics, guitar chords and instructions */
/* print text item at specified point  */
/* if a string starts with ':' this means it is a special symbol */
{
  double ygc;
  char* gc;

  gc = firstitem(textitems);
  ygc = ystart;
  while (gc != NULL) {
    if (*gc == ':') {
      switch (*(gc+1)) {
      case 'p': /* part label */
        fprintf(f, " %.1f %.1f %.1f (", x, ygc, ygap);
        ISOfprintf(gc+2);
        fprintf(f, ") boxshow ");
        break;
      case 's':
        fprintf(f, " %.1f %.1f segno\n", x, ygc);
        break;
      case 'c':
        fprintf(f, " %.1f %.1f coda\n", x, ygc);
        break;
      default:
        break;
      };
    } else {
      fprintf(f, " %.1f %.1f (", x, ygc);
      ISOfprintf(gc);
      fprintf(f, ") gc ");
    };
    gc = nextitem(textitems);
    ygc = ygc - ygap;
  };
}

static void define_font(struct font* thefont, char* s)
/* set up a font structure with user-defined font and pointsize */
{
  char fontname[80];
  int fontsize, params;
   
  params = sscanf(s, "%s %d", fontname, &fontsize);
  if (params >= 1) {
    if (strcmp(fontname, "-") != 0) {
      if (thefont->name != NULL) {
        free(thefont->name);
      };
      thefont->name = addstring(fontname);
      thefont->defined = 0;
    };
    if (params == 2) {
      thefont->pointsize = fontsize;
    };
  };
}

int read_boolean(s)
char *s;
/* set logical parameter from %%command */
/* part of the handling for event_specific */
{
  char p[12]; /* [JA] 2020-09-30 */
  int result;

  p[0] = '\0';
  sscanf(s, " %10s", p);
  result = 1;
  if ((strcmp(p, "0") == 0) || (strcmp(p, "no") == 0) || 
      (strcmp(p, "false") == 0)) {
    result = 0;
  };
  return(result);
}

void font_command(p, s)
/* execute font-related %%commands */
/* called from event_specific */
char* p;
char*s;
{
  if (strcmp(p, "textfont") == 0) {
    define_font(&textfont, s);
  };
  if (strcmp(p, "titlefont") == 0) {
    define_font(&titlefont, s);
  };
  if (strcmp(p, "subtitlefont") == 0) {
    define_font(&subtitlefont, s);
  };
  if (strcmp(p, "wordsfont") == 0) {
    define_font(&wordsfont, s);
  };
  if (strcmp(p, "composerfont") == 0) {
    define_font(&composerfont, s);
  };
  if (strcmp(p, "vocalfont") == 0) {
    define_font(&vocalfont, s);
  };
  if (strcmp(p, "gchordfont") == 0) {
    define_font(&gchordfont, s);
  };
  if (strcmp(p, "partsfont") == 0) {
    define_font(&partsfont, s);
  };
  if (strcmp(p, "titlespace") == 0) {
    set_space(&titlefont, s);
  };
  if (strcmp(p, "subtitlespace") == 0) {
    set_space(&subtitlefont, s);
  };
  if (strcmp(p, "composerspace") == 0) {
    set_space(&composerfont, s);
  };
  if (strcmp(p, "vocalspace") == 0) {
    set_space(&vocalfont, s);
  };
  if (strcmp(p, "wordsspace") == 0) {
    set_space(&wordsfont, s);
  };
  if (strcmp(p, "gchordspace") == 0) {
    set_space(&gchordfont, s);
  };
  if (strcmp(p, "textspace") == 0) {
    set_space(&textfont, s);
  };
  if (strcmp(p, "partsspace") == 0) {
    set_space(&textfont, s);
  };
  if (strcmp(p, "staffsep") == 0) {
    sscanf(s, "%d", &staffsep);
  };
  if (strcmp(p, "titleleft") == 0) {
    titleleft = read_boolean(s);
  };
  if (strcmp(p, "titlecaps") == 0) {
    titlecaps = read_boolean(s);
  };
}

static void setfont(int size, int num)
/* define font point size and style */
{
  if ((fontsize != size) || (fontnum != num)) {
    fprintf(f, "%d.0 F%d\n", size, num);
    fontsize = size;
    fontnum = num;
  };
}

static void setfontstruct(struct font* thefont)
/* set font according to font structure */
{
  if (thefont->name == NULL) {
    setfont(thefont->pointsize, thefont->default_num);
  } else {
    if (thefont->defined == 0) {
      /* define font */
      fprintf(f, "/%s-ISO /%s setISOfont\n", thefont->name, thefont->name);
      fprintf(f, "/F%d { /%s-ISO exch selectfont} def\n", 
                 thefont->special_num, thefont->name);
      thefont->defined = 1;
    }
    setfont(thefont->pointsize, thefont->special_num);
  };
}

static void notetext(struct note* n, int* tupleno, double x, struct feature* ft,
         struct vertspacing* spacing)
/* print text associated with note */
{
  if (n->beaming == endbeam) {
    drawbeam(beamset, beamctr, 0);
    if (*tupleno != 0) {
      drawtuple(beamset, beamctr, *tupleno);
      *tupleno = 0;
    };
  };
  fprintf(f, "\n");
  if (n->instructions != NULL && spacing != NULL) {
    setfontstruct(&partsfont);
    showtext(n->instructions, x - ft->xleft, spacing->yinstruct, 
               (double)(partsfont.pointsize+partsfont.space));
  };
  if (n->gchords != NULL && spacing != NULL) {
    setfontstruct(&gchordfont);
    showtext(n->gchords, x - ft->xleft, spacing->ygchord, 
              (double)(gchordfont.pointsize+gchordfont.space));
  };
  if (n->syllables != NULL && spacing != NULL) {
    setfontstruct(&vocalfont);    
    showtext(n->syllables, x - ft->xleft, spacing->ywords, 
              (double)(vocalfont.pointsize + vocalfont.space));
  };
}

static void handlebeam(struct note* n, struct feature* ft)
/* set up some variables needed to lay out the beam for a set of */
/* notes beamed together */
{
  switch (n->beaming) {
  case single:
    beamctr = -1;
    break;
  case startbeam:
    beamset[0] = ft;
    beamctr = 1;
    rootstem = n->stemup;
    break;
  case midbeam:
  case endbeam:
    if (beamctr > 0) {
      beamset[beamctr] = ft;
      if (beamctr < 64)  {
        beamctr = beamctr + 1;
      } else {
        event_error("Too many notes under one beam");
      };
    } else {
      event_error("Internal beaming error");
    };
    n->stemup = rootstem;
    break;
  };
}

static void drawnote(struct note* n, double x, struct feature* ft, 
         struct chord* thischord,
         int* tupleno, struct vertspacing* spacing)
/* draw a note */
{
  handlebeam(n, ft);
  if (redcolor) fprintf(f,"1.0 0.0 0.0 setrgbcolor\n");
  drawhead(n, x, ft);
  if (thischord == NULL) {
    if (n->base > 1) {
      if (n->stemup) {
        fprintf(f, " %.1f su", n->stemlength);
      } else {
        fprintf(f, " %.1f sd", n->stemlength);
      };
    };
  };
  if (n->beaming == single) {
    singletail(n->base, n->base_exp, n->stemup, n->stemlength);
  };
  fprintf(f, "\n");
  if (redcolor) fprintf(f,"0 setgray\n");
  notetext(n, tupleno, x, ft, spacing);
}

static void drawrest(struct rest* r, double x, struct vertspacing* spacing)
/* draw a rest of specified duration */
{
  int i;

  if (redcolor) fprintf(f,"1.0 0.0 0.0 setrgbcolor\n");
  if (r->multibar > 0) {
    fprintf(f, "(%d) %.1f %d mrest ", r->multibar, x, 4*TONE_HT);
  } else {
    reducef(&r->len);  
    switch (r->base_exp) {
    case 1:
      fprintf(f, "%.1f %d r0 ", x, 4*TONE_HT);
      break;
    case 0:
      fprintf(f, "%.1f %d r1 ", x, 4*TONE_HT);
      break;
    case -1:
      fprintf(f, "%.1f %d r2 ", x, 4*TONE_HT);
      break;
    case -2:
      fprintf(f, "%.1f %d r4 ", x, 4*TONE_HT);
      break;
    case -3:
      fprintf(f, "%.1f %d r8 ", x, 4*TONE_HT);
      break;
    case -4:
      fprintf(f, "%.1f %d r16 ", x, 4*TONE_HT);
      break;
    case -5:
      fprintf(f, "%.1f %d r32 ", x, 4*TONE_HT);
      break;
    case -6:
      fprintf(f, "%.1f %d r64 ", x, 4*TONE_HT);
      break;
    default:
      event_error("Cannot represent rest length");
      break;
    };
  };
  for (i=1; i <= r->dots; i++) {
    fprintf(f, " %.1f 3.0 dt", (double)(HALF_HEAD+DOT_SPACE*i)); 
  };
  fprintf(f, "\n");
  if (redcolor) fprintf(f,"0 setgray\n");
  if (r->instructions != NULL) {
    setfontstruct(&partsfont);
    showtext(r->instructions, x, spacing->yinstruct, 
                (double)(partsfont.pointsize+partsfont.space));
  };
  if (r->gchords != NULL) {
    setfontstruct(&gchordfont);
    showtext(r->gchords, x, spacing->ygchord, 
               (double)(gchordfont.pointsize+gchordfont.space));
  };
}

static void setbeams(struct feature* note[], struct chord* chording[], int m, 
                     double stemlen)
/* choose the spatial postioning of beams and compute appropriate stem */
/* lengths for all notes within the beam */
{
  int i;
  double min[64];
  double lift;
  struct note* anote;
  int stemup;
  double x1, y1, x2, y2, xi, yi;
  double max;

  /* printf("Doing setbeams m=%d\n", m); */
  anote = note[0]->item.voidptr;
  stemup = anote->stemup;
  /* calculate minimum feasible stem lengths */
  /* bearing in mind space needed for the tails */
  for (i=0; i<m; i++) {
    anote = note[i]->item.voidptr;
    anote->stemup = stemup;
    switch (anote->base) {
    default:
    case 8:
      min[i] = TAIL_SEP;
      break;
    case 16:
      min[i] = 2*TAIL_SEP;
      break;
    case 32:
      min[i] = 3*TAIL_SEP;
      break;
    case 64:
      min[i] = 4*TAIL_SEP;
      break;
    };
    if (anote->accidental == ' ') {
      min[i] = min[i] + TONE_HT;
    } else {
      if (stemup) {
        min[i] = min[i] + (double)acc_upsize(anote->accidental);
      } else {
        min[i] = min[i] + (double)acc_downsize(anote->accidental);
      };
    };
    min[i] = min[i] + TONE_HT; /* require at least this much clearance */
    if (stemup) {
      if (chording[i] == NULL) {
        min[i] = TONE_HT*anote->y + min[i];
        /* avoid collision with previous note */
        if ((i > 0) && (min[i-1] - TONE_HT < min[i])) {
          min[i-1] = min[i];
        };
      } else {
        min[i] = TONE_HT*chording[i]->ytop + min[i];
      };
    } else {
      if (chording[i] == NULL) {
        min[i] = TONE_HT*anote->y - min[i];
        /* avoid collision with previous note */
        if (i > 0) {
          anote = note[i-1]->item.voidptr;
          if (anote->y*TONE_HT < min[i]) {
            min[i] = anote->y*TONE_HT;
          };
        };
      } else {
        min[i] = TONE_HT*chording[i]->ybot - min[i];
      };
    };
  };
  /* work out clearance from a straight line between 1st and last note */
  x1 = note[0]->x;
  anote = note[0]->item.voidptr;
  y1 = anote->y*TONE_HT;
  x2 = note[m-1]->x;
  anote = note[m-1]->item.voidptr;
  y2 = anote->y*TONE_HT;
  if (x1 == x2) {
    printf("Internal error: x1 = x2 = %.1f\n", x1);
    x2 = x2 + 1;
  };
  lift = 0.0;
  max = 0.0;
  if (stemup) {
    for (i=0; i<m; i++) {
      xi = note[i]->x;
      yi = y1 + (y2-y1)*(xi-x1)/(x2-x1);
      if (min[i] - yi > lift) {
        lift = min[i] - yi;
      };
      if (min[i] - yi < max) {
        max = min[i] - yi;
      };
    };
    if (lift - max < stemlen) {
      lift = stemlen - max;
    };
  } else {
    for (i=0; i<m; i++) {
      xi = note[i]->x;
      yi = y1 + (y2-y1)*(xi-x1)/(x2-x1);
      if (min[i] - yi < lift) {
        lift = min[i] - yi;
      };
      if (min[i] - yi > max) {
        max = min[i] - yi;
      };
    };
    if (max - lift < stemlen) {
      lift = max - stemlen;
    };
  };
  /* raise the line just enough to clear notes */
  for (i=0; i<m; i++) {
    anote = note[i]->item.voidptr;
    xi = note[i]->x;
    if (stemup) {
      anote->stemlength =  (float) (y1 + (y2-y1)*(xi-x1)/(x2-x1) + lift - 
                           TONE_HT*anote->y);
    } else {
      anote->stemlength = - (float) ((y1 + (y2-y1)*(xi-x1)/(x2-x1) + lift - 
                            TONE_HT*anote->y));
    };
  };
  /* now transfer results to chords */
  for (i=0; i<m; i++) {
    if (chording[i] != NULL) {
      anote = note[i]->item.voidptr;
      chording[i]->stemup = anote->stemup;
      if (chording[i]->stemup) {
        chording[i]->stemlength = anote->stemlength;
      } else {
        chording[i]->stemlength = (float) (anote->stemlength -
           (double)((chording[i]->ytop - chording[i]->ybot)*TONE_HT));
      };
    };
  };
};

static void beamline(struct feature* ft)
/* compute positioning of beams for one stave line of music */
{
  struct feature* p;
  struct note* n;
  struct feature* beamnote[64];
  struct chord* chording[64];
  struct chord* lastchord;
  struct feature* gracebeamnote[64];
  struct chord* gracechording[64];
  struct chord* gracelastchord;
  int i, j;
  int ingrace;

  
  i = j = 0; /* [SS] 2019-08-11 not set in conditional line 2426 when
  beamline() is called from finalsizeline()
  https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=890250 */
  /* [SDG] 2020-06-03 also set j */
  p = ft;
  ingrace = 0;
  lastchord = NULL;
  gracelastchord = NULL;
  while ((p != NULL) && (p->type != PRINTLINE)) {
    switch (p->type) {
    case NOTE:
      if (ingrace) {
        n = p->item.voidptr;
        if (n == NULL) {
          event_error("Missing NOTE!!!!");
          exit(0);
        };
        if (n->beaming == startbeam) {
          j = 0;
        };
        if (n->beaming != single) {
          gracebeamnote[j] = p;
          gracechording[j] = gracelastchord;
          j = j + 1;
        };
        if ((n->beaming == endbeam)||(j==10)) {
          setbeams(gracebeamnote, gracechording, j, GRACE_STEMLEN);
          j = 0;
        };
      } else {
        /* non-grace notes*/
        n = p->item.voidptr;
        if (n == NULL) {
          event_error("Missing NOTE!!!!");
          exit(0);
        };
        if (n->beaming == startbeam) {
          i = 0;
        };
        if (n->beaming != single) {
          beamnote[i] = p;
          chording[i] = lastchord;
          i = i + 1;
        };
        if ((n->beaming == endbeam)||(i==64)) {
          setbeams(beamnote, chording, i, STEMLEN);
          i = 0;
        };
      };
      break;
    case CHORDON:
      if (ingrace) {
        gracelastchord = p->item.voidptr;
      } else {
        lastchord = p->item.voidptr;
      };
      break;
    case CHORDOFF:
      if (ingrace) {
        gracelastchord = NULL;
      } else {
        lastchord = NULL;
      };
      break;
    case GRACEON:
      ingrace = 1;
      break;
    case GRACEOFF:
      ingrace = 0;
      gracelastchord = NULL;
      break;
    default:
      break;
    }; 
    p = p->next;
  };
}

static void measureline(struct feature* ft, double* height, double* descender,
            double* yend, double* yinstruct, double* ygchord, double* ywords)
/* calculate vertical space (height/descender) requirement of current line */
{
  struct feature* p;
  struct note* n;
  struct rest* r;
  double max, min;
  int gotgchords, gcount;
  int gotinstructions, icount;
  int gotwords, wcount;
  int gotends;
  char* gc;

  p = ft;
  min = 0.0;
  max = 8*TONE_HT;
  gotgchords = 0;
  gotwords = 0;
  gotinstructions = 0;
  gotends = 0;
  while ((p != NULL) && (p->type != PRINTLINE)) {
    switch (p->type) {
    case NOTE:
      n = p->item.voidptr;
      if (n == NULL) {
        event_error("Missing NOTE!!!!");
        exit(0);
      } else {
        if (n->gchords != NULL) {
          gcount = 0;
          gc = firstitem(n->gchords);
          while (gc != NULL) {
            gcount = gcount + 1;
            gc = nextitem(n->gchords);
          };
          if (gcount > gotgchords) {
            gotgchords = gcount;
          };
        };
        if (n->instructions != NULL) {
          icount = 0;
          gc = firstitem(n->instructions);
          while (gc != NULL) {
            icount = icount + 1;
            gc = nextitem(n->instructions);
          };
          if (icount > gotinstructions) {
            gotinstructions = icount;
          };
        };
        if (n->syllables != NULL) {
          wcount = 0;
          gc = firstitem(n->syllables);
          while (gc != NULL) {
            wcount = wcount + 1;
            gc = nextitem(n->syllables);
          };
          if (wcount > gotwords) {
            gotwords = wcount;
          };
        };
        /* recalculate note size in case of new stemlen */
        sizenote(n, p, ingrace);
        if (p->yup > max) {
          max = p->yup;
        };
        if (p->ydown < min) {
          min = p->ydown;
        };
      };
      break;
    case REST:
      r = p->item.voidptr;
      if (r == NULL) {
        event_error("Missing REST!!!!");
        exit(0);
      } else {
        if (r->gchords != NULL) {
          gcount = 0;
          gc = firstitem(r->gchords);
          while (gc != NULL) {
            gcount = gcount + 1;
            gc = nextitem(r->gchords);
          };
          if (gcount > gotgchords) {
            gotgchords = gcount;
          };
        };
        if (r->instructions != NULL) {
          icount = 0;
          gc = firstitem(r->instructions);
          while (gc != NULL) {
            icount = icount + 1;
            gc = nextitem(r->instructions);
          };
          if (icount > gotinstructions) {
            gotinstructions = icount;
          };
        };
      };
      break;
    case REP1:
    case REP2:
    case PLAY_ON_REP:
    case BAR1:
    case REP_BAR2:
      gotends = 1;
      break;
    case TUPLE:
    case CLEF:
      if (p->yup > max) {
        max = p->yup;
      };
      if (-(p->ydown) < min) {
        min = -(p->ydown);
      };
      break;
    case TEMPO:
      if (gotinstructions == 0) {
        gotinstructions = 1;
      };
      break;
    default:
      break;
    }; 
    p = p->next;
  };
  if ((gotgchords > 0) && (gchords_above)) {
    max = max + (gchordfont.pointsize + gchordfont.space)*gotgchords;
    *ygchord = max - (gchordfont.pointsize + gchordfont.space);
  };
  if (gotinstructions > 0) {
    max = max + INSTRUCT_HT*gotinstructions;
    *yinstruct = max - INSTRUCT_HT;
  };
  if (gotends) {
    max = max + END_HT;
    *yend = max - END_HT;
  };
  if ((gotgchords > 0) && (!gchords_above)) {
    *ygchord = min - (gchordfont.pointsize + gchordfont.space);
    min = min - (gchordfont.pointsize + gchordfont.space)*gotgchords;
  };
  if (gotwords > 0) {
    *ywords = min - (vocalfont.pointsize + vocalfont.space);
    min = min - (vocalfont.pointsize + vocalfont.space)*gotwords;
  };
  *height = staffsep + max;
  *descender = -min;
}

static void printtext(place, s, afont)
enum placetype place;
char* s;
struct font* afont;
/* print text a line of text in specified font at specified place */
{
  newblock((double)(afont->pointsize + afont->space), 0.0);
  setfontstruct(afont);
  fprintf(f, "(");
  ISOfprintf(s);
  switch (place) {
  case left:
    fprintf(f, ") 0 0 M show\n");
    break;
  case right:
    fprintf(f, ") %.1f 0 M lshow\n", (double) scaledwidth);
    break;
  case centre:
    fprintf(f, ") %.1f 0 M cshow\n", scaledwidth/2.0);
    break;
  };
}

void vskip(double gap)
/* skip over specified amount of vertical space */
{
  newblock(gap, 0.0);
}

void centretext(s)
char* s;
/* print text centred in the middle of the page */
/* used by %%centre command */
{
  printtext(centre, s, &textfont);
}

void lefttext(s)
char* s;
/* print text up against the left hand margin */
/* used by %%text command */
{
  printtext(left, s, &textfont);
}

static void draw_tempo(double x_init, double y_init, struct atempo* t)
/* print out tempo as specified in Q: field */
/* this goes to the left of the tune's title */
{
  int base, base_exp, dots;
  double x, y;
  char ticks[30];

  if (t != NULL) {
    x = x_init;
    y = y_init;
    /* fprintf(f, "gsave 0.7 0.7 scale\n"); */
    setfont(12, 3);
    if (t->pre != NULL) {
      fprintf(f, "%.1f %.1f M (", x, y);
      ISOfprintf(t->pre);
      fprintf(f, ") show\n");
      x = x + stringwidth(t->pre, 12, 3) + HALF_HEAD * 2;
    };
    if (t->count != 0) {
      dots = count_dots(&base, &base_exp, t->basenote.num, t->basenote.denom);
      singlehead(x, y, base, base_exp, dots);
      if (base >= 2) {
        fprintf(f, " %.1f su", TEMPO_STEMLEN);
      };
      singletail(base, base_exp, 1, TEMPO_STEMLEN);
      x = x + 20.0;
      sprintf(ticks, "= %d", t->count);
      fprintf(f, " %.1f %.1f M (%s) show\n", x, y, ticks);
      x = x + stringwidth(ticks, 12, 3);
    };
    if (t->post != NULL) {
      fprintf(f, "%.1f %.1f M ( ", x, y);
      ISOfprintf(t->post);
      fprintf(f, ") show\n");
    };
    /* fprintf(f, "grestore\n"); */
  };
}

static void voicedivider()
/* This draws a horizontal line. It is used to mark where one voice */
/* ends and the next one starts */
{
  newblock((double)staffsep, 0.0);
  fprintf(f, " %.1f %.1f M %.1f %.1f L\n", 
             scaledwidth/4.0, -descend,
             scaledwidth*3/4.0, -descend);
  newblock((double)staffsep, 0.0);
}

static void resetvoice(struct tune* t, struct voice * v)
/* sets up voice with initial attributes as defined in abc tune header */
{
  v->place = v->first;
  if (v->clef == NULL) {
    v->clef = newclef(&t->clef);
  } else {
    copy_clef (v->clef, &t->clef);
  };
  if (v->keysig == NULL) {
    v->keysig = newkey(t->keysig->name, t->keysig->sharps, 
                       t->keysig->map, t->keysig->mult);
  } else {
    free(v->keysig->name);
    v->keysig->name = addstring(t->keysig->name);
    set_keysig(v->keysig, t->keysig);
    /* v->keysig->sharps = t->keysig->sharps; */
  };
  v->meter.num = t->meter.num;
  v->meter.denom = t->meter.denom;
}

static void resettune(struct tune* t)
/* initializes all voices to their start state */
{
  struct voice* v;

  v = firstitem(&t->voices);
  while (v != NULL) {
    resetvoice(t, v);
    v = nextitem(&t->voices);
  };
}

void setup_fonts()
{
  init_font(&textfont,        TEXT_HT,      1, 1, 5);
  init_font(&titlefont,       TITLE1_HT,    0, 0, 6);
  init_font(&subtitlefont,    TITLE2_HT,    0, 0, 7);
  init_font(&wordsfont,       WORDS_HT,     1, 0, 8);
  init_font(&composerfont,    COMP_HT,      1, 1, 9);
  init_font(&vocalfont,       LYRIC_HT,     1, 2, 10);
  init_font(&gchordfont,      CHORDNAME_HT, 1, 3, 11);
  init_font(&partsfont,       INSTRUCT_HT,  1, 3, 12);
  staffsep = VERT_GAP;
}

void open_output_file(filename, boundingbox)
char* filename;
struct bbox* boundingbox;
/* open output file and write the abc2ps library to it */
{
  f = fopen(filename, "w");
  if (f == NULL) {
    printf("Could not open file!!\n");
    exit(0);
  };
  printf("writing file %s\n", filename);
  printlib(f, filename, boundingbox);
  fontsize = 0;
  fontnum = 0;
  startpage();
}

void close_output_file()
/* complete last page and close file */
{
  if (f != NULL) {
    closepage();
    fclose(f);
    f = NULL;
  };
}

static int endrep(inend, end_string, x1, x2, yend)
int inend;
char* end_string;
double x1, x2, yend;
/* draws either 1st or 2nd ending marker */
{
  if (inend != 0) {
    fprintf(f, "%.1f %.1f %.1f (%s) endy1\n", yend, x1, x2, end_string);
  };
  return(0);
}

static void drawslurtie(struct slurtie* s)
/* draws slur or tie corresponding to the given structure */
{
  double x0, y0, x1, y1;
  struct feature* ft;
  struct note* n;
  struct rest* r;
  int stemup = 0;

  ft = s->begin;
  if (ft == NULL)  {
    event_error("Slur or tie missing start note");
    return;
  };
  x0 = ft->x;
  if ((ft->type == NOTE) || (ft->type == CHORDNOTE)) {
    n = ft->item.voidptr;
    y0 = (double)(n->y * TONE_HT);
    stemup = n->stemup;
  } else {
    if (ft->type == REST) {
      r = ft->item.voidptr;
      y0 = (double)(4 * TONE_HT);
    } else {
      printf("Internal error: NOT A NOTE/REST (%d)in slur/tie\n" ,ft->type);
      return;
    };
  };
  if (s->crossline) {
    /* draw to end of stave */
    x1 = scaledwidth + xmargin/(2.0*scale);
    y1 = y0;
  } else {
    /* draw slur/tie to other element */
    ft = s->end;
    if (ft == NULL)  {
      event_error("Slur or tie missing end note");
      return;
    };
    x1 = ft->x;
    if ((ft->type == NOTE) || (ft->type == CHORDNOTE)) {
      n = ft->item.voidptr;
      y1 = (double)(n->y * TONE_HT);
      stemup = n->stemup;
    } else {
      if (ft->type == REST) {
        r = ft->item.voidptr;
        y1 = (double)(4 * TONE_HT);
      } else {
        printf("Internal error: NOT A NOTE/REST (%d)in slur/tie\n" ,ft->type);
        return;
      };
    };
  };
  if (stemup) {
    fprintf(f, " %.1f %.1f %.1f %.1f slurdown\n", x0, y0, x1, y1); 
  } else {
    fprintf(f, " %.1f %.1f %.1f %.1f slurup\n", x0, y0, x1, y1); 
  };
}

static void close_slurtie(struct slurtie* s)
/* This deals with the special case of a slur or tie running beyond */
/* one line of music and into the next. This draws the part on the */
/* second line of music */
{
  double x0, y0, x1, y1;
  struct feature* ft;
  struct note* n;
  struct rest* r;
  int stemup = 0;

  if ((s == NULL) || (s->crossline == 0)) {
    return;
  } else {
    /* draw slur/tie to other element */
    ft = s->end;
    if (ft == NULL)  {
      event_error("Slur or tie missing end note");
      return;
    };
    x1 = ft->x;
    if ((ft->type == NOTE) || (ft->type == CHORDNOTE)) {
      n = ft->item.voidptr;
      y1 = (double)(n->y * TONE_HT);
      stemup = n->stemup;
    } else {
      if (ft->type == REST) {
        r = ft->item.voidptr;
        y1 = (double)(4 * TONE_HT);
      } else {
        printf("Internal error: NOT A NOTE/REST (%d)in slur/tie\n" ,ft->type);
        return;
      };
    };
  };
  x0 = TREBLE_LEFT + TREBLE_RIGHT;
  y0 = y1;
  if (stemup) {
    fprintf(f, " %.1f %.1f %.1f %.1f slurdown\n", x0, y0, x1, y1); 
  } else {
    fprintf(f, " %.1f %.1f %.1f %.1f slurup\n", x0, y0, x1, y1); 
  };
}

static void blockline(struct voice* v, struct vertspacing** spacing)
/* gets vertical spacing for current line */
{
  struct feature* ft;
  /* struct vertspacing* spacing; */

  ft = v->place;
  /* find end of line */
  while ((ft != NULL) && (ft->type != PRINTLINE)) {
    ft = ft->next;
  };
  if ((ft != NULL) && (ft->type == PRINTLINE)) {
    *spacing = ft->item.voidptr;
  } else {
    *spacing = NULL;
    event_error("Expecting line of music - possible voices mis-match");
  };
}

static void printbarnumber(double x, int n)
{
if (barnums <0 ) return;
if (x + 15.0 > scaledwidth) return;
fprintf(f, " %.1f %.1f (%d) bnum\n", x, 28.0, n); 
}

static void underbar(struct feature* ft)
/* This is never normally called, but is useful as a debugging routine */
/* shows width of a graphical element */
{
  fprintf(f, " %.1f -10 moveto %.1f -10 lineto stroke\n", ft->x - ft->xleft,
                                                       ft->x + ft->xright);
}

static int printvoiceline(struct voice* v)
/* draws one line of music from specified voice */
{
  struct feature* ft;
  struct note* anote;
  struct key* akey;
  char* astring;
  struct fract* afract;
  struct rest* arest;
  struct tuple* atuple;
  double x;
  int sharps;
  struct chord* thischord;
  int chordcount;
  double xchord;
  cleftype_t* theclef;
  int printedline;
  double xend;
  int inend;
  char endstr[80];
  int ingrace;
  struct vertspacing* spacing;
  struct dynamic *psaction;

  /* skip over line number so we can check for end of tune */
  while ((v->place != NULL) && 
         ((v->place->type == LINENUM) || (v->place->type == NEWPAGE) ||
          (v->place->type == LEFT_TEXT) || (v->place->type == CENTRE_TEXT) ||
          (v->place->type == VSKIP))) {
    if (v->place->type == LINENUM) {
      lineno = v->place->item.number;
    };
    if (v->place->type == NEWPAGE) {
      newpage();
    };
    /* [JA] 2020-11-07 */
    if (v->place->type == LEFT_TEXT) {
      printtext(left, v->place->item.voidptr, &textfont);
    };
    if (v->place->type == CENTRE_TEXT) {
      printtext(centre, v->place->item.voidptr, &textfont);
    };

    if (v->place->type == VSKIP) {
      vskip((double)v->place->item.number);
    };
    v->place = v->place->next;
  };
  if (v->place == NULL) {
    return(0);
  };
  inend = 0;
  ingrace = 0;
  chordcount = 0;
  v->beamed_tuple_pending = 0;
  x = 20;
  thischord = NULL;
  if (v->keysig == NULL) {
    event_error("Voice has no key signature");
  } else {
    sharps = v->keysig->sharps;
  };
  /* draw stave */
  ft = v->place;
  blockline(v, &spacing);
  if (spacing != NULL) {
    newblock(spacing->height, spacing->descender);
    staveline();
    fprintf(f, "0 0 M 0 24 L\n");
  };
  x = 0.0;
  /* now draw the line */
  printedline = 0;
  while ((ft != NULL) && (!printedline)) {
    /*  printf("type = %d\n", ft->type); */
    switch (ft->type) {
    case SINGLE_BAR: 
      fprintf(f, "%.1f bar\n", ft->x);
      printbarnumber(ft->x, ft->item.number);
      break;
    case DOUBLE_BAR: 
      fprintf(f, "%.1f dbar\n", ft->x);
      printbarnumber(ft->x, ft->item.number);
      inend = endrep(inend, endstr, xend, ft->x, spacing->yend);
      break;
    case BAR_REP: 
      fprintf(f, "%.1f fbar1 %.1f rdots\n", ft->x, ft->x+10);
      printbarnumber(ft->x, ft->item.number);
      inend = endrep(inend, endstr, xend, ft->x, spacing->yend);
      break;
    case REP_BAR: 
      fprintf(f, "%.1f rdots %.1f fbar2\n", ft->x, ft->x+10);
      printbarnumber(ft->x, ft->item.number);
      inend = endrep(inend, endstr, xend, ft->x, spacing->yend);
      break;
    case REP1: 
      inend = endrep(inend, endstr, xend, ft->x - ft->xleft, spacing->yend);
      inend = 1;
      strcpy(endstr, "1");
      xend = ft->x + ft->xright;
      break;
    case REP2: 
      inend = endrep(inend, endstr, xend, ft->x - ft->xleft, spacing->yend);
      inend = 2;
      strcpy(endstr, "2");
      xend = ft->x + ft->xright;
      break;
    case PLAY_ON_REP: 
      inend = endrep(inend, endstr, xend, ft->x - ft->xleft, spacing->yend);
      inend = 1;
      strcpy(endstr, ft->item.voidptr);
      xend = ft->x + ft->xright;
      break;
    case BAR1: 
      fprintf(f, "%.1f bar\n", ft->x);
      printbarnumber(ft->x, ft->item.number);
      inend = endrep(inend, endstr, xend, ft->x - ft->xleft, spacing->yend);
      inend = 1;
      strcpy(endstr, "1");
      xend = ft->x + ft->xright;
      break;
    case REP_BAR2: 
      fprintf(f, "%.1f rdots %.1f fbar2\n", ft->x, ft->x+10);
      printbarnumber(ft->x, ft->item.number);
      inend = endrep(inend, endstr, xend, ft->x - ft->xleft, spacing->yend);
      inend = 2;
      strcpy(endstr, "2");
      xend = ft->x + ft->xright;
      break;
    case DOUBLE_REP: 
      fprintf(f, "%.1f fbar1 %.1f rdots %.1f fbar2 %.1f rdots\n", 
              ft->x, ft->x+10, ft->x+2, ft->x-8);
      inend = endrep(inend, endstr, xend, ft->x, spacing->yend);
      break;
    case THICK_THIN: 
      fprintf(f, "%.1f fbar1\n", ft->x);
      inend = endrep(inend, endstr, xend, ft->x, spacing->yend);
      break;
    case THIN_THICK: 
      fprintf(f, "%.1f fbar2\n", ft->x);
      inend = endrep(inend, endstr, xend, ft->x, spacing->yend);
      break;
    case PART: 
      astring = ft->item.voidptr;
      break;
    case TEMPO: 
      draw_tempo(ft->x, spacing->yinstruct, ft->item.voidptr);
      break;
    case TIME: 
      afract = ft->item.voidptr;
      if (afract==NULL) {
        if (v->line == midline) {
          draw_meter(&v->meter, ft->x);
        };
      } else {
        setfract(&v->meter, afract->num, afract->denom);
        if (v->line == midline) {
          draw_meter(afract, ft->x);
        };
      };
      break;
    case KEY: 
      akey = ft->item.voidptr;
      if (v->line == midline) {
        draw_keysig(v->keysig->map, akey->map, akey->mult, ft->x, v->clef);
      };
      set_keysig(v->keysig, akey);
      break;
    case REST: 
      arest = ft->item.voidptr;
      drawrest(arest, ft->x, spacing);
      if (v->tuple_count > 0) {
        if (v->tuple_count == v->tuplenotes) {
          v->tuple_xstart = ft->x - ft->xleft;
        };
        if (v->tuple_count == 1) {
          v->tuple_xend = ft->x + ft->xright;
          drawhtuple(v->tuple_xstart, v->tuple_xend, v->tuplelabel,
                     v->tuple_height+4.0);
        };
        v->tuple_count = v->tuple_count - 1;
      };
      break;
    case TUPLE: 
      atuple = ft->item.voidptr;
      if (atuple->beamed) {
        v->beamed_tuple_pending = atuple->n;
        v->tuplenotes = atuple->r;
        v->tuple_count = 0;
      } else {
        v->tuple_height = atuple->height;
        v->tuplenotes = atuple->r;
        v->tuplelabel = atuple->label;
        v->tuple_count = atuple->r;
      };
      break;
    case NOTE: 
      anote = ft->item.voidptr;
      if (thischord == NULL) {
        if (ingrace) {
          drawgracenote(anote, ft->x, ft, thischord);
        } else {
          drawnote(anote, ft->x, ft, thischord, &v->beamed_tuple_pending, 
                   spacing);
          if (v->tuple_count > 0) {
            if (v->tuple_count == v->tuplenotes) {
              v->tuple_xstart = ft->x - ft->xleft;
            };
            if (v->tuple_count == 1) {
              v->tuple_xend = ft->x + ft->xright;
              drawhtuple(v->tuple_xstart, v->tuple_xend, v->tuplelabel,
                         v->tuple_height+4.0);
            };
            v->tuple_count = v->tuple_count - 1;
          };
        };
      } else {
        xchord = ft->x; /* make sure all chord heads are aligned */
        if (ingrace) {
          handlegracebeam(anote, ft);
          drawgracehead(anote, ft->x, ft, nostem);
        } else {
          handlebeam(anote, ft);
          drawhead(anote, ft->x, ft);
          if (chordcount == 0) {
            notetext(anote, &v->beamed_tuple_pending, ft->x, ft, spacing);
          };
          chordcount = chordcount + 1;
        };
      };
      break;
    case CHORDNOTE:
      anote = ft->item.voidptr;
      if (ingrace) {
        drawgracehead(anote, xchord, ft, nostem);
      } else {
        drawhead(anote, xchord, ft);
      };
      break;
    case NONOTE:
      break;
    case OLDTIE: 
      break;
    case TEXT: 
      break;
    case SLUR_ON: 
      drawslurtie(ft->item.voidptr);
      break;
    case SLUR_OFF: 
      close_slurtie(ft->item.voidptr);
      break;
    case TIE: 
      drawslurtie(ft->item.voidptr);
      break;
    case CLOSE_TIE:
      close_slurtie(ft->item.voidptr);
      break;
    case TITLE: 
      break;
    case CHANNEL: 
      break;
    case TRANSPOSE: 
      break;
    case RTRANSPOSE: 
      break;
    case GRACEON:
      ingrace = 1;
      break;
    case GRACEOFF:
      ingrace = 0;
      break;
    case SETGRACE: 
      break;
    case SETC: 
      break;
    case GCHORD: 
      break;
    case GCHORDON: 
      break;
    case GCHORDOFF: 
      break;
    case VOICE: 
      break;
    case CHORDON: 
      thischord = ft->item.voidptr;
      chordcount = 0;
      drawchordtail(thischord, ft->next->x);
      break;
    case CHORDOFF: 
      thischord = NULL;
      if (v->tuple_count > 0) {
         if (v->tuple_count == v->tuplenotes) {
             v->tuple_xstart = ft->x - ft->xleft;
            };
         if (v->tuple_count == 1) {
             v->tuple_xend = ft->x + ft->xright;
             drawhtuple(v->tuple_xstart, v->tuple_xend, v->tuplelabel,
                  v->tuple_height+4.0);
            };
         v->tuple_count = v->tuple_count - 1;
        };
      break;
    case SLUR_TIE: 
      break;
    case TNOTE: 
      break;
    case LT: 
      break;
    case GT: 
      break;
    case DYNAMIC: 
      psaction = ft->item.voidptr;
      if(psaction->color == 'r') redcolor = 1; /* [SS] 2013-11-04 */
      if(psaction->color == 'b') redcolor = 0;
      break;
    case LINENUM: 
      lineno = ft->item.number;
      break;
    case MUSICLINE: 
      v->line = midline;
      break;
    case PRINTLINE:
      inend = endrep(inend, endstr, xend, scaledwidth, spacing->yend);
      printedline = 1;
      v->line = newline;
      break;
    case MUSICSTOP: 
      break;
    case WORDLINE: 
      break;
    case WORDSTOP: 
      break;
    case INSTRUCTION:
      break;
    case NOBEAM:
      break;
    case CLEF:
      theclef = ft->item.voidptr;
      if (theclef != NULL) {
        copy_clef (v->clef, theclef);
      };
      if (v->line == midline) {
        printclef(v->clef, ft->x, ft->yup, ft->ydown);
      };
      break;
    case NEWPAGE:
      event_warning("newpage in music line ignored");
      break;
    case LEFT_TEXT:
      event_warning("text in music line ignored");
      break;
    case CENTRE_TEXT:
      event_warning("centred text in music line ignored");
      break;
    case VSKIP:
      event_warning("%%vskip in music line ignored");
      break;
    case SPLITVOICE:
      break;
    default:
      printf("unknown type: %d\n", (int)ft->type);
      break;
    };
    ft = ft->next;
  };
  v->place = ft;
  return(1);
}

static int finalsizeline(struct voice* v)
/* does final beaming and works out vertical height for */
/* one line of specified voice */
{
  struct feature* ft;
  double height, descender;
  double yend, yinstruct, ygchord, ywords;
  struct vertspacing* avertspacing;

  /* skip over line number so we can check for end of tune */
  while ((v->place != NULL) && 
         ((v->place->type == LINENUM) || (v->place->type == NEWPAGE) ||
          (v->place->type == LEFT_TEXT) || (v->place->type == CENTRE_TEXT) ||
          (v->place->type == VSKIP))) {
    v->place = v->place->next;
  };
  if (v->place == NULL) {
    return(0);
  };
  ft = v->place;
  beamline(ft);
  measureline(ft, &height, &descender, &yend, &yinstruct, &ygchord, &ywords);
  /* newblock(height, descender); */
  /* find end of line */
  while ((ft != NULL) && (ft->type != PRINTLINE)) {
    ft = ft->next;
  };
  if ((ft != NULL) && (ft->type == PRINTLINE)) {
    avertspacing =  ft->item.voidptr;
    avertspacing->height = (float) height;
    avertspacing->descender = (float) descender;
    avertspacing->yend = (float) yend;
    avertspacing->yinstruct = (float) yinstruct;
    avertspacing->ygchord = (float) ygchord;
    avertspacing->ywords = (float) ywords;
    ft = ft->next;
  };
  v->place = ft;
  return(1);
}

static void finalsizetune(t)
struct tune* t;
/* calculate beaming and calculate height of each music line */
{
  struct voice* thisvoice;
  int doneline;

  thisvoice = firstitem(&t->voices);
  while (thisvoice != NULL) {
    doneline = 1;
    while (doneline==1) {
      doneline = finalsizeline(thisvoice);
    };
    thisvoice = nextitem(&t->voices);
  };
}

static int getlineheight(struct voice* v, double* height)
/* calculate vertical space for one line of music from specified voice */
{
  struct vertspacing* spacing;

  /* go over between line stuff */
  while ((v->place != NULL) && 
         ((v->place->type == LINENUM) || (v->place->type == NEWPAGE) ||
          (v->place->type == LEFT_TEXT) || (v->place->type == CENTRE_TEXT) ||
          (v->place->type == VSKIP))) {
    if (v->place->type == LINENUM) {
      lineno = v->place->item.number;
    };
    if (v->place->type == LEFT_TEXT) {
      *height = *height + textfont.pointsize + textfont.space;
    };
    if (v->place->type == CENTRE_TEXT) {
      *height = *height + textfont.pointsize + textfont.space;
    };
    if (v->place->type == VSKIP) {
      *height = *height + (double)(v->place->item.number);
    };
    v->place = v->place->next;
  };
  if (v->place == NULL) {
    return(0);
  };
  /* skip over rest of line */
  while ((v->place != NULL) && (v->place->type != PRINTLINE)) {
    v->place = v->place->next;
  };
  if (v->place != NULL) {
    spacing = v->place->item.voidptr;
    *height = *height + spacing->height + spacing->descender;
    v->place = v->place->next;
  };
  return(1);
}

static double tuneheight(t)
struct tune* t;
/* measures the vertical space needed by the tune */
{
  char* atitle;
  char* wordline;
  struct voice* thisvoice;
  int doneline;
  double height;
  char *notesfield; /* from N: field */

  height = 0.0;
  resettune(t);
  atitle = firstitem(&t->title);
  while (atitle != NULL) {
    height = height + titlefont.pointsize + titlefont.space;
    atitle = nextitem(&t->title);
  };
  if ((titleleft == 1) && (t->tempo != NULL)) {
    height = height + titlefont.pointsize + 2*titlefont.space;
  };
  if (t->composer != NULL) {
    height = height + composerfont.pointsize + composerfont.space;
  };
  if (t->origin != NULL) {
    height = height + composerfont.pointsize + composerfont.space;
  };
  if (t->parts != NULL) {
    height = height + composerfont.pointsize + composerfont.space;
  };
  notesfield = firstitem(&t->notes);
  while (notesfield != NULL) {
    height = height + composerfont.pointsize + composerfont.space;
    notesfield = nextitem(&t->notes);
  };
  thisvoice = firstitem(&t->voices);
  if (separate_voices) {
    thisvoice = firstitem(&t->voices);
    while (thisvoice != NULL) {
      doneline = 1;
      while (doneline==1) {
        doneline = getlineheight(thisvoice, &height);
      };
      thisvoice = nextitem(&t->voices);
      if (thisvoice != NULL) {
        height = height + 2.0 * staffsep;
      };
    };
  } else {
    doneline = 1;
    while(doneline) {
      thisvoice = firstitem(&t->voices);
      doneline = 0;
      while (thisvoice != NULL) {
        doneline = (getlineheight(thisvoice, &height) || doneline);
        thisvoice = nextitem(&t->voices);
      };
    };
  };
  wordline = firstitem(&t->words);
  while (wordline != NULL) {
    height = height + wordsfont.pointsize + wordsfont.space;
    wordline = nextitem(&t->words);
  };
  return(height);
}

void printtune(struct tune* t)
/* draws the PostScript for an entire abc tune */
{
  int i;
  char *atitle;
  char *notesfield; /* from N: field */
  char *wordline;
  int notesdone;
  int titleno;
  struct voice* thisvoice;
  int doneline;
  double firstvline, lastvline;
  struct voice* firstvoice;
  char xtitle[200];
  struct bbox boundingbox;
  enum placetype titleplace;

  redcolor = 0; /* [SS] 2013-11-04 */
  resettune(t);
  sizetune(t);
  resettune(t);
  if (separate_voices) {
    monospace(t);
  } else {
    spacevoices(t);
  };
  resettune(t);
  finalsizetune(t);
  if (debugging) {
    showtune(t);
  };
  if (eps_out) {
    if (landscape) {
      boundingbox.llx = ymargin;
      boundingbox.lly = xmargin;
      boundingbox.urx = (int) (ymargin + tuneheight(t) * scale);
      boundingbox.ury = xmargin + pagewidth;
    } else {
      boundingbox.llx = xmargin;
      boundingbox.lly = (int) (ymargin + pagelen - tuneheight(t) * scale);
      boundingbox.urx = xmargin + pagewidth;
      boundingbox.ury = ymargin + pagelen;
    };
#ifdef NO_SNPRINTF
    /* [SS] 2020-11-01 */
    sprintf(outputname,  "%s%d.eps", outputroot, t->no); /* [JA] 2020-11-01 */
#else
    snprintf(outputname, MAX_OUTPUTNAME, "%s%d.eps", outputroot, t->no); /* [JA] 2020-11-01 */
#endif
    open_output_file(outputname, &boundingbox);
  } else {
    make_open();
    if ((totlen > 0.0) && 
        (totlen+descend+tuneheight(t) > scaledlen-(double)(pagenumbering))) {
      newpage();
    };
  };
  resettune(t);
  notesdone = 0;
  if (titleleft) {
    titleplace = left;
  } else {
    titleplace = centre;
  };
  atitle = firstitem(&t->title);
  titleno = 1;
  while (atitle != NULL) {
    if (titleno == 1) {
      if (titlecaps) {
        for (i=0; i< (int) strlen(atitle); i++) {
          if (islower(atitle[i])) {
            atitle[i] = atitle[i] + 'A' - 'a';
          };
        };
      };
      if ((print_xref) && (strlen(atitle) < 180)) {
        sprintf(xtitle, "%d. %s", t->no, atitle);
        printtext(titleplace, xtitle, &titlefont);
      } else {
        printtext(titleplace, atitle, &titlefont);
      };
    } else {
      printtext(titleplace, atitle, &subtitlefont);
    };
    titleno = titleno + 1;
    atitle = nextitem(&t->title);
  };
  if (t->tempo != NULL) {
    if (titleleft) {
      newblock((double)(titlefont.pointsize + titlefont.space*2), 0.0);
    };
    draw_tempo(10.0, 0.0, t->tempo);
  };
  if (t->composer != NULL) {
    printtext(right, t->composer, &composerfont);
  };
  if (t->origin != NULL) {
    printtext(right, t->origin, &composerfont);
  };
  if (t->parts != NULL) {
    printtext(right, t->parts, &composerfont);
  };
  notesfield = firstitem(&t->notes);
  while (notesfield != NULL) {
    printtext(left, notesfield, &composerfont);
    notesfield = nextitem(&t->notes);
  };
  donemeter = 0;
  /* musicsetup(); */
  thisvoice = firstitem(&t->voices);
  while (thisvoice != NULL) {
    copy_clef (thisvoice->clef, &t->clef);
    setfract(&thisvoice->meter, t->meter.num, t->meter.denom);
    thisvoice->place = thisvoice->first;
    thisvoice = nextitem(&t->voices);
  };
  if (separate_voices) {
    thisvoice = firstitem(&t->voices);
    while (thisvoice != NULL) {
      doneline = 1;
      while (doneline==1) {
        doneline = printvoiceline(thisvoice);
      };
      thisvoice = nextitem(&t->voices);
      if (thisvoice != NULL) {
        voicedivider();
      };
    };
  } else {
    doneline = 1;
    while(doneline) {
      thisvoice = firstitem(&t->voices);
      firstvoice = thisvoice;
      doneline = 0;
      while (thisvoice != NULL) {
        doneline = (printvoiceline(thisvoice) || doneline);
        if (thisvoice == firstvoice) {
          firstvline = totlen;
        } else {
          lastvline = totlen;
          if (lastvline > firstvline) {
            fprintf(f, "0 24 M 0 %.1f L\n", 
                lastvline - firstvline);
          };
          firstvline = totlen;
        };
        thisvoice = nextitem(&t->voices);
      };
    };
  };
  /* musicrestore(); */
  wordline = firstitem(&t->words);
  while (wordline != NULL) {
    printtext(left, wordline, &wordsfont);
    wordline = nextitem(&t->words);
  };
  if (eps_out) {
    close_output_file();
  };
}


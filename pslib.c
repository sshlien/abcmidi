
/*
 * This file is Copyright (C) 1999 Michael Methfessel
 * with modifications by James Allwright
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

/* pslib.c */
/* part of yaps - abc to PostScript converter */
/* This file contains a single C routine which prints a library of */
/* PostScript routines to a file */

/*  Extra routines added :                                            */
/*  /slurup, /slurdown  - simpler interface than original functions   */
/*  /endy1, /endy2      - re-positionable versions of end1, end2      */
/*  /setx, /sety        - set /x and /y with no other effects         */
/*  /boxshow            - show text string in a box                   */
/*  /segno              - draw segno symbol                           */
/*  /coda               - draw coda symbol                            */
/*  /setISOfont         - create ISO variant of font                  */
/*  /BHD                - draw breve                                  */
/*  /r0                 - breve rest                                  */
/*  /mrest              - multiple bar rest (taken from abcm2ps)      */

#include <stdio.h>
#include <stdlib.h>

/*  #define ANSILIBS is just used to access time functions for */
/*  %%CreationDate . This can safely be removed if it causes   */
/*  compilation problems */
#ifdef ANSILIBS
#include <time.h>
#endif
#include "drawtune.h"

/* output library of PostScript routines for drawing music symbols */

static void ps_header(f, filename, boundingbox)
/* create header section of PostScript file */
FILE* f;
char* filename;
struct bbox* boundingbox;
{

  if (eps_out) {
    fprintf(f,"%%!PS-Adobe-3.0 EPSF-3.0\n");
  } else {
    fprintf(f,"%%!PS-Adobe-3.0\n");
  };
  if (boundingbox != NULL) {
    fprintf(f,"%%%%BoundingBox: %d %d %d %d\n", boundingbox->llx, 
              boundingbox->lly, boundingbox->urx, boundingbox->ury);
  };
  fprintf(f,"%%%%Title: %s\n", filename);
  fprintf(f,"%%%%Creator: yaps (abc to PostScript converter)\n");
#ifdef ANSILIBS
  {
    char timebuff[40];
    time_t now;

    fprintf(f,"%%%%CreationDate: ");
    now = time(NULL);
    /* define ASCTIME if your system does not have strftime() */
#ifdef ASCTIME
    fprintf(f, asctime(localtime(&now)));
#else
    strftime(timebuff, (size_t)40, "%a %d %b %Y at %H:%M\n", localtime(&now));
    fprintf(f,"%s", timebuff);
#endif
  };
#endif
  fprintf(f,"%%%%LanguageLevel: 2\n");
  fprintf(f,"%%%%EndComments\n");
  fprintf(f,"\n");
  fprintf(f,"%%\n");
  fprintf(f,"%%  This is the abc2ps PostScript Music Library,\n");
  fprintf(f,"%%  containing Postscript for generating musical symbols.\n");
  fprintf(f,"%%  Original Copyright (c) 1999 Michael Methfessel.\n");
  fprintf(f,"%%  This Library may be re-distributed under the\n");
  fprintf(f,"%%  terms of the GNU General Public License.\n");
  fprintf(f,"%%  This form of the library created by James\n");
  fprintf(f,"%%  Allwright, July 1999.\n");
  fprintf(f,"%%\n");
  fprintf(f,"\n");
  fprintf(f,"%%%%BeginSetup\n");
  fprintf(f,"\n");
}

static void section1(f)
FILE* f;
{
  fprintf(f,"%% Section 1 - Fonts and String Manipulation\n");
  fprintf(f,"\n");
  fprintf(f,"/Times-Roman findfont\n");
  fprintf(f,"dup length dict begin\n");
  fprintf(f,"   {1 index /FID ne {def} {pop pop} ifelse} forall\n");
  fprintf(f,"   /Encoding ISOLatin1Encoding def\n");
  fprintf(f,"   currentdict\n");
  fprintf(f,"end\n");
  fprintf(f,"/Times-Roman-ISO exch definefont pop\n");
  fprintf(f,"/F0 { /Times-Roman-ISO exch selectfont } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/Times-Italic findfont\n");
  fprintf(f,"dup length dict begin\n");
  fprintf(f,"   {1 index /FID ne {def} {pop pop} ifelse} forall\n");
  fprintf(f,"   /Encoding ISOLatin1Encoding def\n");
  fprintf(f,"   currentdict\n");
  fprintf(f,"end\n");
  fprintf(f,"/Times-Italic-ISO exch definefont pop\n");
  fprintf(f,"/F1 { /Times-Italic-ISO exch selectfont } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/Times-Bold findfont\n");
  fprintf(f,"dup length dict begin\n");
  fprintf(f,"   {1 index /FID ne {def} {pop pop} ifelse} forall\n");
  fprintf(f,"   /Encoding ISOLatin1Encoding def\n");
  fprintf(f,"   currentdict\n");
  fprintf(f,"end\n");
  fprintf(f,"/Times-Bold-ISO exch definefont pop\n");
  fprintf(f,"/F2 { /Times-Bold-ISO exch selectfont } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/Helvetica findfont\n");
  fprintf(f,"dup length dict begin\n");
  fprintf(f,"   {1 index /FID ne {def} {pop pop} ifelse} forall\n");
  fprintf(f,"   /Encoding ISOLatin1Encoding def\n");
  fprintf(f,"   currentdict\n");
  fprintf(f,"end\n");
  fprintf(f,"/Helvetica-ISO exch definefont pop\n");
  fprintf(f,"/F3 { /Helvetica-ISO exch selectfont } bind def\n");
  fprintf(f,"\n");

  fprintf(f,"/setISOfont { %% usage /Name-ISO /Name setISOfont\n");
  fprintf(f,"              %% define ISO encoding variant of font\n");
  fprintf(f,"  findfont\n");
  fprintf(f,"  dup length dict begin\n");
  fprintf(f,"     {1 index /FID ne {def} {pop pop} ifelse} forall\n");
  fprintf(f,"     /Encoding ISOLatin1Encoding def\n");
  fprintf(f,"     currentdict\n");
  fprintf(f,"  end\n");
  fprintf(f,"  definefont pop\n");
  fprintf(f,"} def\n");
  fprintf(f,"\n");

  fprintf(f,"/cshow { %% usage: string cshow  - center at current pt\n");
  fprintf(f,"   dup stringwidth pop 2 div neg 0 rmoveto\n");
  fprintf(f,"   currentpoint pop dup 0 lt {neg 0 rmoveto} {pop} ifelse\n");
  fprintf(f,"   show\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/lshow { %% usage: string lshow - show left-aligned\n");
  fprintf(f,"   dup stringwidth pop neg 0 rmoveto show\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f, "/boxshow {  %% usage: x y h string boxshow - text in a box\n");
  fprintf(f, "  3 index 2 add 3 index 2 add moveto\n");
  fprintf(f, "  dup show\n");
  fprintf(f, "  3 index 3 index moveto\n");
  fprintf(f, "  stringwidth pop 4 add exch dup 0 exch rlineto\n");
  fprintf(f, "  exch 0 rlineto\n");
  fprintf(f, "  neg 0 exch rlineto\n");
  fprintf(f, "  closepath stroke\n");
  fprintf(f, "} bind def\n");
  fprintf(f, "\n");
  fprintf(f,"/wd { moveto show } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/wln {\n");
  fprintf(f,"dup 3 1 roll moveto gsave 0.6 setlinewidth lineto stroke grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/whf {moveto gsave 0.5 1.2 scale (-) show grestore} bind def\n");
  fprintf(f,"\n");
}

static void section2(f)
FILE* f;
{
  fprintf(f,"%% Section 2 - Clefs\n");
  fprintf(f,"\n");
  fprintf(f,"/tclef {  %% usage:  x tclef  - treble clef \n");
  fprintf(f,"  0 moveto\n");
  fprintf(f,"  -2.22 5.91 rmoveto\n");
  fprintf(f,"  -0.74 -1.11 rlineto\n");
  fprintf(f,"  -2.22 2.22 -0.74 8.12 3.69 8.12 rcurveto\n");
  fprintf(f,"  2.22 0.74 7.02 -1.85 7.02 -6.65 rcurveto\n");
  fprintf(f,"  0.00 -4.43 -4.06 -6.65 -7.75 -6.65 rcurveto\n");
  fprintf(f,"  -4.43 0.00 -8.49 2.22 -8.49 8.49 rcurveto\n");
  fprintf(f,"  0.00 2.58 0.37 5.54 5.91 9.97 rcurveto\n");
  fprintf(f,"  6.28 4.43 6.28 7.02 6.28 8.86 rcurveto\n");
  fprintf(f,"  0.00 1.85 -0.37 3.32 -1.11 3.32 rcurveto\n");
  fprintf(f,"  -1.85 -1.48 -3.32 -5.17 -3.32 -7.38 rcurveto\n");
  fprintf(f,"  0.00 -13.66 4.43 -16.25 4.80 -25.85 rcurveto\n");
  fprintf(f,"  0.00 -3.69 -2.22 -5.54 -5.54 -5.54 rcurveto\n");
  fprintf(f,"  -2.22 0.00 -4.06 1.85 -4.06 3.69 rcurveto\n");
  fprintf(f,"  0.00 1.85 1.11 3.32 2.95 3.32 rcurveto\n");
  fprintf(f,"  3.69 0.00 3.69 -5.91 0.37 -4.80 rcurveto\n");
  fprintf(f,"  0.37 -2.22 5.54 -1.11 5.54 2.95 rcurveto\n");
  fprintf(f,"  -0.74 12.18 -5.17 14.40 -5.17 28.06 rcurveto\n");
  fprintf(f,"  0.00 4.06 1.11 7.38 4.43 8.86 rcurveto\n");
  fprintf(f,"  2.22 -1.48 4.06 -4.06 3.69 -6.65 rcurveto\n");
  fprintf(f,"  0.00 -4.06 -2.58 -7.02 -5.91 -10.34 rcurveto\n");
  fprintf(f,"  -2.22 -2.58 -5.91 -4.43 -5.91 -9.60 rcurveto\n");
  fprintf(f,"  0.00 -4.43 2.58 -6.65 5.54 -6.65 rcurveto\n");
  fprintf(f,"  2.95 0.00 5.54 1.85 5.54 4.80 rcurveto\n");
  fprintf(f,"  0.00 3.32 -2.95 4.43 -4.80 4.43 rcurveto\n");
  fprintf(f,"  -2.58 0.00 -4.06 -1.85 -2.95 -3.69 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/stclef {\n");
  fprintf(f,"  0.85 div gsave 0.85 0.85 scale tclef grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/bclef {  %% usage:  x bclef  - bass clef \n");
  fprintf(f,"  0 moveto\n");
  fprintf(f,"  -9.80 2.50 rmoveto\n");
  fprintf(f,"  8.30 4.00 12.80 9.00 12.80 13.00 rcurveto\n");
  fprintf(f,"  0.00 4.50 -2.00 7.50 -4.30 7.30 rcurveto\n");
  fprintf(f,"  -1.00 0.20 -4.20 0.20 -5.70 -3.80 rcurveto\n");
  fprintf(f,"  1.50 0.50 3.50 0.00 3.50 -1.50 rcurveto\n");
  fprintf(f,"  0.00 -1.00 -0.50 -2.00 -2.00 -2.00 rcurveto\n");
  fprintf(f,"  -1.00 0.00 -2.00 0.90 -2.00 2.50 rcurveto\n");
  fprintf(f,"  0.00 2.50 2.10 5.50 6.00 5.50 rcurveto\n");
  fprintf(f,"  4.00 0.00 7.50 -2.50 7.50 -7.50 rcurveto\n");
  fprintf(f,"  0.00 -5.50 -6.50 -11.00 -15.50 -13.70 rcurveto\n");
  fprintf(f,"  8.30 4.00 12.80 9.00 12.80 13.00 rcurveto\n");
  fprintf(f,"  0.00 4.50 -2.00 7.50 -4.30 7.30 rcurveto\n");
  fprintf(f,"  -1.00 0.20 -4.20 0.20 -5.70 -3.80 rcurveto\n");
  fprintf(f,"  1.50 0.50 3.50 0.00 3.50 -1.50 rcurveto\n");
  fprintf(f,"  0.00 -1.00 -0.50 -2.00 -2.00 -2.00 rcurveto\n");
  fprintf(f,"  -1.00 0.00 -2.00 0.90 -2.00 2.50 rcurveto\n");
  fprintf(f,"  0.00 2.50 2.10 5.50 6.00 5.50 rcurveto\n");
  fprintf(f,"  4.00 0.00 7.50 -2.50 7.50 -7.50 rcurveto\n");
  fprintf(f,"  0.00 -5.50 -6.50 -11.00 -15.50 -13.70 rcurveto\n");
  fprintf(f,"  16.90 18.20 rmoveto\n");
  fprintf(f,"  0.00 1.50 2.00 1.50 2.00 0.00 rcurveto\n");
  fprintf(f,"  0.00 -1.50 -2.00 -1.50 -2.00 0.00 rcurveto\n");
  fprintf(f,"  0.00 -5.50 rmoveto\n");
  fprintf(f,"  0.00 1.50 2.00 1.50 2.00 0.00 rcurveto\n");
  fprintf(f,"  0.00 -1.50 -2.00 -1.50 -2.00 0.00 rcurveto\n");
  fprintf(f,"fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/sbclef {\n");
  fprintf(f,"  0.85 div gsave 0.85 0.85 scale 0 4 translate bclef grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/cchalf {\n");
  fprintf(f,"  0 moveto\n");
  fprintf(f,"  0.00 12.00 rmoveto\n");
  fprintf(f,"  1.20 2.75 rlineto\n");
  fprintf(f,"  4.20 -0.50 6.00 2.25 6.00 5.00 rcurveto\n");
  fprintf(f,"  0.00 2.00 -0.60 3.90 -3.30 4.00 rcurveto\n");
  fprintf(f,"  -0.78 0.00 -2.70 0.00 -3.60 -2.00 rcurveto\n");
  fprintf(f,"  0.90 0.25 2.10 0.00 2.10 -0.75 rcurveto\n");
  fprintf(f,"  0.00 -0.50 -0.30 -1.00 -1.20 -1.00 rcurveto\n");
  fprintf(f,"  -0.60 0.00 -1.20 0.45 -1.20 1.25 rcurveto\n");
  fprintf(f,"  0.00 1.25 1.26 2.75 3.60 2.75 rcurveto\n");
  fprintf(f,"  3.60 0.00 5.40 -1.25 5.40 -3.75 rcurveto\n");
  fprintf(f,"  0.00 -3.25 -3.00 -6.00 -6.60 -5.75 rcurveto\n");
  fprintf(f,"  -0.60 -2.50 rlineto\n");
  fprintf(f,"fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/cclef {   %% usage: x cclef\n");
  fprintf(f,"   dup dup dup\n");
  fprintf(f,"   cchalf gsave 0 24 translate 1 -1 scale cchalf\n");
  fprintf(f,"   6.5 sub 0 moveto 0 24 rlineto 3 0 rlineto 0 -24 rlineto fill\n");
  fprintf(f,"   1.8 sub 0 moveto 0 24 rlineto 0.8 setlinewidth stroke grestore \n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/scclef { cclef } bind def\n");
  fprintf(f,"\n");
}

static void section3(f)
FILE* f;
{
  fprintf(f,"%% Section 3 - Note Heads, Stems and Beams\n");
  fprintf(f,"\n");
  fprintf(f,"/hd {  %% usage: x y hd  - full head\n");
  fprintf(f,"  dup /y exch def exch dup /x exch def exch moveto\n");
  fprintf(f,"  3.30 2.26 rmoveto\n");
  fprintf(f,"  -2.26 3.30 -8.86 -1.22 -6.60 -4.52 rcurveto\n");
  fprintf(f,"  2.26 -3.30 8.86 1.22 6.60 4.52 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/Hd {  %% usage: x y Hd  - open head for half\n");
  fprintf(f,"  dup /y exch def exch dup /x exch def exch moveto\n");
  fprintf(f,"  3.51 1.92 rmoveto\n");
  fprintf(f,"  -2.04 3.73 -9.06 -0.10 -7.02 -3.83 rcurveto\n");
  fprintf(f,"  2.04 -3.73 9.06 0.10 7.02 3.83 rcurveto\n");
  fprintf(f,"  -0.44 -0.24 rmoveto\n");
  fprintf(f,"  0.96 -1.76 -5.19 -5.11 -6.15 -3.35 rcurveto\n");
  fprintf(f,"  -0.96 1.76 5.19 5.11 6.15 3.35 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/HD { %% usage: x y HD  - open head for whole\n");
  fprintf(f,"  dup /y exch def exch dup /x exch def exch moveto\n");
  fprintf(f,"  5.96 0.00 rmoveto\n");
  fprintf(f,"  0.00 1.08 -2.71 3.52 -5.96 3.52 rcurveto\n");
  fprintf(f,"  -3.25 0.00 -5.96 -2.44 -5.96 -3.52 rcurveto\n");
  fprintf(f,"  0.00 -1.08 2.71 -3.52 5.96 -3.52 rcurveto\n");
  fprintf(f,"  3.25 0.00 5.96 2.44 5.96 3.52 rcurveto\n");
  fprintf(f,"  -8.13 1.62 rmoveto\n");
  fprintf(f,"  1.62 2.17 5.96 -1.07 4.34 -3.24 rcurveto\n");
  fprintf(f,"  -1.62 -2.17 -5.96 1.07 -4.34 3.24 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/BHD { %% usage x y BHD - breve\n");
  fprintf(f,"  HD\n");
  fprintf(f,"  x 6.0 sub y 3.5 sub moveto 0 7.0 rlineto stroke\n");
  fprintf(f,"  x 8.0 sub y 3.5 sub moveto 0 7.0 rlineto stroke\n");
  fprintf(f,"  x 6.0 add y 3.5 sub moveto 0 7.0 rlineto stroke\n");
  fprintf(f,"  x 8.0 add y 3.5 sub moveto 0 7.0 rlineto stroke\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/setx { %% usage: x setx - set x value\n");
  fprintf(f,"  /x exch store\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/sety { %% usage: y sety - set y value\n");
  fprintf(f,"  /y exch store\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/su {  %% usage: len su   - up stem\n");
  fprintf(f,"  x y moveto 3.5 1.0 rmoveto 1.0 sub 0 exch rlineto stroke\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/sd {  %% usage: len td   - down stem\n");
  fprintf(f,"  x y moveto -3.5 -1.0 rmoveto neg 1.0 add 0 exch rlineto stroke\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/bm {  %% usage: x1 y1 x2 y2 t bm  - beam, depth t\n");
  fprintf(f,"  3 1 roll moveto dup 0 exch neg rlineto\n");
  fprintf(f,"  dup 4 1 roll sub lineto 0 exch rlineto fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/bnum {  %% usage: x y (str) bnum  - number on beam\n");
  fprintf(f,"  3 1 roll moveto gsave /Times-Italic 12 selectfont\n");
  fprintf(f,"  cshow grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/hbr {  %% usage: x1 y1 x2 y2 hbr  - half bracket\n");
  fprintf(f,"  moveto lineto 0 -3 rlineto stroke\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/SL {  %% usage: pp2x pp1x p1 pp1 pp2 p2 p1 sl\n");
  fprintf(f,"  moveto curveto rlineto curveto fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f, "\n");
  fprintf(f, "%% New slurs with simple interface \n");
  fprintf(f, "/slurdown { %% Slur - usage x1 y1 x2 y2 slurdown\n");
  fprintf(f, "  /ys2 exch def\n");
  fprintf(f, "  /xs2 exch def\n");
  fprintf(f, "  /ys1 exch def\n");
  fprintf(f, "  /xs1 exch def\n");
  fprintf(f, "  /ddy xs2 xs1 sub abs 2 div dup 12 gt {pop 12} if def\n");
  fprintf(f, "  xs1 4 add ys1 4 sub moveto\n");
  fprintf(f, "  xs1 2 mul xs2 add 3 div ys1 2 mul ys2 add 3 div ddy sub\n");
  fprintf(f, "  xs1 xs2 2 mul add 3 div ys1 ys2 2 mul add 3 div ddy sub\n");
  fprintf(f, "  xs2 4 sub ys2 4 sub\n");
  fprintf(f, "  curveto\n");
  fprintf(f, "  xs1 xs2 2 mul add 3 div ys1 ys2 2 mul add 3 div ddy sub 2 sub\n");
  fprintf(f, "  xs1 2 mul xs2 add 3 div ys1 2 mul ys2 add 3 div ddy sub 2 sub\n");
  fprintf(f, "  xs1 4 add ys1 4 sub\n");
  fprintf(f, "  curveto fill\n");
  fprintf(f, "} def\n");
  fprintf(f, "/slurup { %% Slur - usage x1 y1 x2 y2 slurup\n");
  fprintf(f, "  /ys2 exch def\n");
  fprintf(f, "  /xs2 exch def\n");
  fprintf(f, "  /ys1 exch def\n");
  fprintf(f, "  /xs1 exch def\n");
  fprintf(f, "  /ddy xs2 xs1 sub abs 2 div dup 12 gt {pop 12} if def\n");
  fprintf(f, "  xs1 4 add ys1 4 add moveto\n");
  fprintf(f, "  xs1 2 mul xs2 add 3 div ys1 2 mul ys2 add 3 div ddy add\n");
  fprintf(f, "  xs1 xs2 2 mul add 3 div ys1 ys2 2 mul add 3 div ddy add\n");
  fprintf(f, "  xs2 4 sub ys2 4 add\n");
  fprintf(f, "  curveto\n");
  fprintf(f, "  xs1 xs2 2 mul add 3 div ys1 ys2 2 mul add 3 div ddy add 2 sub\n");
  fprintf(f, "  xs1 2 mul xs2 add 3 div ys1 2 mul ys2 add 3 div ddy add 2 sub\n");
  fprintf(f, "  xs1 4 add ys1 4 add\n");
  fprintf(f, "  curveto fill\n");
  fprintf(f, "} def\n");
  fprintf(f,"\n");
}

static void section4(f)
FILE* f;
{
  fprintf(f,"%% Section 4 - Note Ornaments (Staccato, Roll, Trill etc.)\n");
  fprintf(f,"\n");
  fprintf(f,"/dt {  %% usage: dx dy dt  - dot shifted by dx,dy\n");
  fprintf(f,"  y add exch x add exch 1.2 0 360 arc fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/grm {  %% usage:  y grm  - gracing mark\n");
  fprintf(f,"  x exch moveto\n");
  fprintf(f,"  -5.00 -1.00 rmoveto\n");
  fprintf(f,"  5.00 8.50 5.50 -4.50 10.00 2.00 rcurveto\n");
  fprintf(f,"  -5.00 -8.50 -5.50 4.50 -10.00 -2.00 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/stc {  %% usage:  y stc  - staccato mark\n");
  fprintf(f,"  x exch 1.2 0 360 arc fill } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/cpu {  %% usage:  y cpu  - roll sign above head\n");
  fprintf(f,"  x exch moveto\n");
  fprintf(f,"  -5.85 0.00 rmoveto\n");
  fprintf(f,"  0.45 7.29 11.25 7.29 11.70 0.00 rcurveto\n");
  fprintf(f,"  -1.35 5.99 -10.35 5.99 -11.70 0.00 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/cpd {  %% usage:  y cpd  - roll sign below head\n");
  fprintf(f,"  x exch moveto\n");
  fprintf(f,"  -5.85 0.00 rmoveto\n");
  fprintf(f,"  0.45 -7.29 11.25 -7.29 11.70 0.00 rcurveto\n");
  fprintf(f,"  -1.35 -5.99 -10.35 -5.99 -11.70 0.00 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/sld {  %% usage:  y dx sld  - slide\n");
  fprintf(f,"  x exch sub exch moveto\n");
  fprintf(f,"  -7.20 -4.80 rmoveto\n");
  fprintf(f,"  1.80 -0.70 4.50 0.20 7.20 4.80 rcurveto\n");
  fprintf(f,"  -2.07 -5.00 -5.40 -6.80 -7.65 -6.00 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/emb {  %% usage:  y emb  - empahsis bar\n");
  fprintf(f,"  gsave 1.2 setlinewidth 1 setlinecap x exch moveto \n");
  fprintf(f," -2.5 0 rmoveto 5 0 rlineto stroke grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/trl {  %% usage:  y trl  - trill sign\n");
  fprintf(f,"  gsave /Times-BoldItalic 14 selectfont\n");
  fprintf(f,"  x 4 sub exch moveto (tr) show grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/hld {  %% usage:  y hld  - hold sign\n");
  fprintf(f,"  x exch 2 copy 1.5 add 1.3 0 360 arc moveto\n");
  fprintf(f,"  -7.50 0.00 rmoveto\n");
  fprintf(f,"  0.00 11.50 15.00 11.50 15.00 0.00 rcurveto\n");
  fprintf(f,"  -0.25 0.00 rlineto\n");
  fprintf(f,"  -1.25 9.00 -13.25 9.00 -14.50 0.00 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/dnb {  %% usage:  y dnb  - down bow\n");
  fprintf(f,"  x exch moveto\n");
  fprintf(f,"  -3.20 0.00 rmoveto\n");
  fprintf(f,"  0.00 7.20 rlineto\n");
  fprintf(f,"  6.40 0.00 rlineto\n");
  fprintf(f,"  0.00 -7.20 rlineto\n");
  fprintf(f,"   currentpoint stroke moveto\n");
  fprintf(f,"  -6.40 4.80 rmoveto\n");
  fprintf(f,"  0.00 2.40 rlineto\n");
  fprintf(f,"  6.40 0.00 rlineto\n");
  fprintf(f,"  0.00 -2.40 rlineto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/upb {  %% usage:  y upb  - up bow\n");
  fprintf(f,"  x exch moveto\n");
  fprintf(f,"  -2.56 8.80 rmoveto\n");
  fprintf(f,"  2.56 -8.80 rlineto\n");
  fprintf(f,"  2.56 8.80 rlineto\n");
  fprintf(f,"   stroke\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
}

static void section5(f)
FILE* f;
{
  fprintf(f,"%% Section 5 - helper lines\n");
  fprintf(f,"\n");
  fprintf(f,"/hl {  %% usage: y hl  - helper line at height y\n");
  fprintf(f,"   gsave 1 setlinewidth x exch moveto \n");
  fprintf(f,"   -5.5 0 rmoveto 11 0 rlineto stroke grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/hl1 {  %% usage: y hl1  - longer helper line\n");
  fprintf(f,"   gsave 1 setlinewidth x exch moveto \n");
  fprintf(f,"   -7 0 rmoveto 14 0 rlineto stroke grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
}

static void section6(f)
FILE* f;
{
  fprintf(f,"%% Section 6 - Note tails \n");
  fprintf(f,"\n");
  fprintf(f,"/f1u {  %% usage:  len f1u  - single flag up\n");
  fprintf(f,"  y add x 3.5 add exch moveto\n");
  fprintf(f,"  0.00 0.00 rmoveto\n");
  fprintf(f,"  1.00 -2.00 0.67 -1.67 2.67 -4.00 rcurveto\n");
  fprintf(f,"  3.33 -2.67 3.33 -6.67 2.67 -9.33 rcurveto\n");
  fprintf(f,"  -0.67 -2.67 -2.67 -4.00 -1.00 -1.00 rcurveto\n");
  fprintf(f,"  1.67 4.33 -1.67 8.33 -4.33 9.00 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/f1d {  %% usage:  len f1d  - single flag down\n");
  fprintf(f,"  neg y add x 3.5 sub exch moveto\n");
  fprintf(f,"  0.00 0.00 rmoveto\n");
  fprintf(f,"  1.20 2.00 0.80 1.67 3.20 4.00 rcurveto\n");
  fprintf(f,"  4.00 2.67 4.00 6.67 3.20 9.33 rcurveto\n");
  fprintf(f,"  -0.80 2.67 -3.20 4.00 -1.20 1.00 rcurveto\n");
  fprintf(f,"  2.00 -4.33 -2.00 -8.33 -5.20 -9.00 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/f2u {  %% usage:  len f2u  - double flag up\n");
  fprintf(f,"  y add x 3.5 add exch moveto\n");
  fprintf(f,"  0.00 0.00 rmoveto\n");
  fprintf(f,"  1.33 -3.33 6.00 -4.00 5.00 -12.00 rcurveto\n");
  fprintf(f,"  0.00 6.00 -4.00 7.67 -5.00 7.67 rcurveto\n");
  fprintf(f,"  1.33 -5.00 6.00 -5.00 5.00 -13.00 rcurveto\n");
  fprintf(f,"  0.00 6.00 -4.00 7.67 -5.00 8.00 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/f2d {  %% usage:  len f2d  - double flag down\n");
  fprintf(f,"  neg y add x 3.5 sub exch moveto\n");
  fprintf(f,"  0.00 0.00 rmoveto\n");
  fprintf(f,"  1.60 3.33 7.20 4.00 6.00 12.00 rcurveto\n");
  fprintf(f,"  0.00 -6.00 -4.80 -7.67 -6.00 -7.67 rcurveto\n");
  fprintf(f,"  1.60 5.00 7.20 5.00 6.00 13.00 rcurveto\n");
  fprintf(f,"  0.00 -6.00 -4.80 -7.67 -6.00 -8.00 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/xfu {  %% usage:  len xfu  - extra flag up\n");
  fprintf(f,"  y add x 3.5 add exch moveto\n");
  fprintf(f,"  0.00 0.00 rmoveto\n");
  fprintf(f,"  1.33 -5.00 6.00 -5.00 5.00 -13.00 rcurveto\n");
  fprintf(f,"  0.00 6.00 -4.00 7.67 -5.00 8.00 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/xfd {  %% usage:  len xfd  - extra flag down\n");
  fprintf(f,"  neg y add x 3.5 sub exch moveto\n");
  fprintf(f,"  0.00 0.00 rmoveto\n");
  fprintf(f,"  1.60 5.00 7.20 5.00 6.00 13.00 rcurveto\n");
  fprintf(f,"  0.00 -6.00 -4.80 -7.67 -6.00 -8.00 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/f3d {dup f2d 9.5 sub xfd} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/f4d {dup dup f2d 9.5 sub xfd 14.7 sub xfd} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/f3u {dup f2u 9.5 sub xfu} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/f4u {dup dup f2u 9.5 sub xfu 14.7 sub xfu} bind def\n");
  fprintf(f,"\n");
}

static void section7(f)
FILE* f;
{
  fprintf(f,"%% Section 7 - Flats, Sharps and Naturals.\n");
  fprintf(f,"\n");
  fprintf(f,"/ft0 { %% usage:  x y ft0  - flat sign\n");
  fprintf(f,"  moveto\n");
  fprintf(f,"  -1.60 2.67 rmoveto\n");
  fprintf(f,"  6.40 3.11 6.40 -3.56 0.00 -6.67 rcurveto\n");
  fprintf(f,"  4.80 4.00 4.80 7.56 0.00 5.78 rcurveto\n");
  fprintf(f,"  currentpoint fill moveto\n");
  fprintf(f,"  0.00 7.11 rmoveto\n");
  fprintf(f,"  0.00 -12.44 rlineto\n");
  fprintf(f,"  stroke\n");
  fprintf(f," } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/ft { %% usage: dx ft  - flat relative to head\n");
  fprintf(f," neg x add y ft0 } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/ftx { %% usage:  x y ftx  - narrow flat sign\n");
  fprintf(f,"  moveto\n");
  fprintf(f,"  -1.42 2.67 rmoveto\n");
  fprintf(f,"  5.69 3.11 5.69 -3.56 0.00 -6.67 rcurveto\n");
  fprintf(f,"  4.27 4.00 4.27 7.56 0.00 5.78 rcurveto\n");
  fprintf(f,"  currentpoint fill moveto\n");
  fprintf(f,"  0.00 7.11 rmoveto\n");
  fprintf(f,"  0.00 -12.44 rlineto\n");
  fprintf(f,"  stroke\n");
  fprintf(f," } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/dft0 { %% usage: x y dft0 ft  - double flat sign\n");
  fprintf(f,"  2 copy exch 2.5 sub exch ftx exch 1.5 add exch ftx } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/dft { %% usage: dx dft  - double flat relative to head\n");
  fprintf(f,"  neg x add y dft0 } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/sh0 {  %% usage:  x y sh0  - sharp sign\n");
  fprintf(f,"  moveto\n");
  fprintf(f,"  2.60 2.89 rmoveto\n");
  fprintf(f,"  0.00 2.17 rlineto\n");
  fprintf(f,"  -5.20 -1.44 rlineto\n");
  fprintf(f,"  0.00 -2.17 rlineto\n");
  fprintf(f,"  5.20 1.44 rlineto\n");
  fprintf(f,"  0.00 -6.50 rmoveto\n");
  fprintf(f,"  0.00 2.17 rlineto\n");
  fprintf(f,"  -5.20 -1.44 rlineto\n");
  fprintf(f,"  0.00 -2.17 rlineto\n");
  fprintf(f,"  5.20 1.44 rlineto\n");
  fprintf(f,"  currentpoint fill moveto\n");
  fprintf(f,"  -1.30 -3.61 rmoveto\n");
  fprintf(f,"  0.00 15.53 rlineto\n");
  fprintf(f,"  currentpoint stroke moveto\n");
  fprintf(f,"  -2.60 -16.61 rmoveto\n");
  fprintf(f,"  0.00 15.53 rlineto\n");
  fprintf(f,"  stroke\n");
  fprintf(f," } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/sh { %% usage: dx sh  - sharp relative to head\n");
  fprintf(f," neg x add y sh0 } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/nt0 {  %% usage:  x y nt0  - natural sign\n");
  fprintf(f,"  moveto\n");
  fprintf(f,"  -1.62 -4.33 rmoveto\n");
  fprintf(f,"  3.25 0.72 rlineto\n");
  fprintf(f,"  0.00 2.17 rlineto\n");
  fprintf(f,"  -3.25 -0.72 rlineto\n");
  fprintf(f,"  0.00 6.50 rlineto\n");
  fprintf(f,"  0.00 -2.89 rmoveto\n");
  fprintf(f,"  3.25 0.72 rlineto\n");
  fprintf(f,"  0.00 2.17 rlineto\n");
  fprintf(f,"  -3.25 -0.72 rlineto\n");
  fprintf(f,"  0.00 -2.17 rlineto\n");
  fprintf(f,"  currentpoint fill moveto\n");
  fprintf(f,"  0.00 6.50 rmoveto\n");
  fprintf(f,"  0.00 -11.92 rlineto\n");
  fprintf(f,"  currentpoint stroke moveto\n");
  fprintf(f,"  3.25 7.94 rmoveto\n");
  fprintf(f,"  0.00 -11.92 rlineto\n");
  fprintf(f,"  stroke\n");
  fprintf(f," } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/nt { %% usage: dx nt  - natural relative to head\n");
  fprintf(f," neg x add y nt0 } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/dsh0 {  %% usage:  x y dsh0  - double sharp \n");
  fprintf(f,"  moveto\n");
  fprintf(f,"  0.39 0.00 rmoveto\n");
  fprintf(f,"  1.78 1.67 rlineto\n");
  fprintf(f,"  1.17 0.00 rlineto\n");
  fprintf(f,"  0.11 1.78 rlineto\n");
  fprintf(f,"  -1.78 -0.11 rlineto\n");
  fprintf(f,"  0.00 -1.17 rlineto\n");
  fprintf(f,"  -1.67 -1.78 rlineto\n");
  fprintf(f,"  -1.67 1.78 rlineto\n");
  fprintf(f,"  0.00 1.17 rlineto\n");
  fprintf(f,"  -1.78 0.11 rlineto\n");
  fprintf(f,"  0.11 -1.78 rlineto\n");
  fprintf(f,"  1.17 0.00 rlineto\n");
  fprintf(f,"  1.78 -1.67 rlineto\n");
  fprintf(f,"  -1.78 -1.67 rlineto\n");
  fprintf(f,"  -1.17 0.00 rlineto\n");
  fprintf(f,"  -0.11 -1.78 rlineto\n");
  fprintf(f,"  1.78 0.11 rlineto\n");
  fprintf(f,"  0.00 1.17 rlineto\n");
  fprintf(f,"  1.67 1.78 rlineto\n");
  fprintf(f,"  1.67 -1.78 rlineto\n");
  fprintf(f,"  0.00 -1.17 rlineto\n");
  fprintf(f,"  1.78 -0.11 rlineto\n");
  fprintf(f,"  -0.11 1.78 rlineto\n");
  fprintf(f,"  -1.17 0.00 rlineto\n");
  fprintf(f,"  -1.78 1.67 rlineto\n");
  fprintf(f,"  fill\n");
  fprintf(f," } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/dsh { %% usage: dx dsh  - double sharp relative to head\n");
  fprintf(f," neg x add y dsh0 } bind def\n");
  fprintf(f,"\n");
}

static void section8(f)
FILE* f;
{
  fprintf(f,"%% Section 8 - Guitar Chords\n");
  fprintf(f,"\n");
  fprintf(f,"/gc { %% usage: x y (str) gc  -- draw guitar chord string\n");
  fprintf(f,"  3 1 roll moveto show\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
}

static void section9(f)
FILE* f;
{
  fprintf(f,"%% Section 9 - Rests\n");
  fprintf(f,"\n");
  fprintf(f,"/r4 {  %% usage:  x y r4  -  quarter rest\n");
  fprintf(f,"   dup /y exch def exch dup /x exch def exch moveto\n");
  fprintf(f,"  -0.52 8.87 rmoveto\n");
  fprintf(f,"  8.35 -6.78 -2.61 -4.70 3.91 -11.48 rcurveto\n");
  fprintf(f,"  -4.43 1.57 -6.00 -3.13 -2.87 -5.22 rcurveto\n");
  fprintf(f,"  -5.22 2.09 -3.65 7.83 0.00 7.30 rcurveto\n");
  fprintf(f,"  -5.22 4.17 3.13 3.13 -1.04 9.39 rcurveto\n");
  fprintf(f,"  fill\n");
  fprintf(f," } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/r8 {  %% usage:  x y r8  -  eighth rest\n");
  fprintf(f,"   dup /y exch def exch dup /x exch def exch moveto\n");
  fprintf(f,"  3.11 5.44 rmoveto\n");
  fprintf(f,"  -1.17 -1.94 -1.94 -3.50 -3.69 -3.89 rcurveto\n");
  fprintf(f,"  2.14 2.72 -2.92 3.89 -2.92 1.17 rcurveto\n");
  fprintf(f,"  0.00 -1.17 1.17 -1.94 2.33 -1.94 rcurveto\n");
  fprintf(f,"  2.72 0.00 3.11 1.94 3.89 3.50 rcurveto\n");
  fprintf(f,"  -3.42 -12.06 rlineto\n");
  fprintf(f,"  0.51 0.00 rlineto\n");
  fprintf(f,"  3.50 13.22 rlineto\n");
  fprintf(f,"  fill\n");
  fprintf(f," } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/r16 {  %% usage:  x y r16  -  16th rest\n");
  fprintf(f,"   dup /y exch def exch dup /x exch def exch moveto\n");
  fprintf(f,"  3.11 5.44 rmoveto\n");
  fprintf(f,"  -1.17 -1.94 -1.94 -3.50 -3.69 -3.89 rcurveto\n");
  fprintf(f,"  2.14 2.72 -2.92 3.89 -2.92 1.17 rcurveto\n");
  fprintf(f,"  0.00 -1.17 1.17 -1.94 2.33 -1.94 rcurveto\n");
  fprintf(f,"  2.72 0.00 3.11 1.94 3.89 3.50 rcurveto\n");
  fprintf(f,"  -1.24 -4.28 rlineto\n");
  fprintf(f,"  -1.17 -1.94 -1.94 -3.50 -3.69 -3.89 rcurveto\n");
  fprintf(f,"  2.14 2.72 -2.92 3.89 -2.92 1.17 rcurveto\n");
  fprintf(f,"  0.00 -1.17 1.17 -1.94 2.33 -1.94 rcurveto\n");
  fprintf(f,"  2.72 0.00 3.11 1.94 4.01 3.50 rcurveto\n");
  fprintf(f,"  -1.91 -7.00 rlineto\n");
  fprintf(f,"  0.51 0.00 rlineto\n");
  fprintf(f,"  3.50 13.61 rlineto\n");
  fprintf(f,"  fill\n");
  fprintf(f," } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/r0 {  %% usage:  x y r0  -  breve rest\n");
  fprintf(f,"  dup /y exch def exch dup /x exch def exch moveto\n");
  fprintf(f,"  -3 0 rmoveto 0 6 rlineto 6 0 rlineto 0 -6 rlineto fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f, "\n");
  fprintf(f,"/r1 {  %% usage:  x y r1  -  whole rest\n");
  fprintf(f,"  dup /y exch def exch dup /x exch def exch moveto\n");
  fprintf(f,"  -3 0 rmoveto 0 -3 rlineto 6 0 rlineto 0 3 rlineto fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/r2 {  %% usage:  x y r2  -  half rest\n");
  fprintf(f,"  dup /y exch def exch dup /x exch def exch moveto\n");
  fprintf(f,"  -3 0 rmoveto 0 3 rlineto 6 0 rlineto 0 -3 rlineto fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/r32 {\n");
  fprintf(f,"  2 copy r16 5.5 add exch 1.6 add exch r8\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/r64 {\n");
  fprintf(f,"  2 copy 5.5 add exch 1.6 add exch r16\n");
  fprintf(f,"  5.5 sub exch 1.5 sub exch r16\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/mrest {  %%usage: nb_measures x y mrest\n");
  fprintf(f,"  gsave 1 setlinewidth\n");
  fprintf(f,"  2 copy -6 add moveto 0 12 rlineto stroke\n");
  fprintf(f,"  2 copy exch 40 add exch -6 add moveto 0 12 rlineto stroke\n");
  fprintf(f,"  2 copy moveto 5 setlinewidth 40 0 rlineto stroke\n");
  fprintf(f,"  /Times-Bold 15 selectfont exch 20 add exch 16 add moveto cshow\n");
  fprintf(f,"  grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
}

static void section10(f)
FILE* f;
{
  fprintf(f,"%% Section 10 - Bar signs\n");
  fprintf(f,"\n");
  fprintf(f,"/bar {  %% usage: x bar  - single bar\n");
  fprintf(f,"  0 moveto  0 24 rlineto stroke\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/dbar {  %% usage: x dbar  - thin double bar\n");
  fprintf(f,"   0 moveto 0 24 rlineto -3 -24 rmoveto\n");
  fprintf(f,"   0 24 rlineto stroke\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/fbar1 {  %% usage: x fbar1  - fat double bar at start\n");
  fprintf(f,"  0 moveto  0 24 rlineto 3 0 rlineto 0 -24 rlineto \n");
  fprintf(f,"  currentpoint fill moveto\n");
  fprintf(f,"  3 0 rmoveto 0 24 rlineto stroke\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/fbar2 {  %% usage: x fbar2  - fat double bar at end\n");
  fprintf(f,"  0 moveto  0 24 rlineto -3 0 rlineto 0 -24 rlineto \n");
  fprintf(f,"  currentpoint fill moveto\n");
  fprintf(f,"  -3 0 rmoveto 0 24 rlineto stroke\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/rdots {  %% usage: x rdots  - repeat dots \n");
  fprintf(f,"  0 moveto 0 9 rmoveto currentpoint 2 copy 1.2 0 360 arc \n");
  fprintf(f,"  moveto 0 6 rmoveto  currentpoint 1.2 0 360 arc fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/end1 {  %% usage: x1 x2 (str) end1  - mark first ending\n");
  fprintf(f,"  3 1 roll 44 moveto 0 6 rlineto dup 50 lineto 0 -6 rlineto stroke\n");
  fprintf(f,"  4 add 40 moveto gsave /Times-Roman 13 selectfont 1.2 0.95 scale\n");
  fprintf(f,"  show grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/end2 {  %% usage: x1 x2 (str) end2  - mark second ending\n");
  fprintf(f,"  3 1 roll 50 moveto dup 50 lineto 0 -6 rlineto stroke\n");
  fprintf(f,"  4 add 40 moveto gsave /Times-Roman 13 selectfont 1.2 0.95 scale\n");
  fprintf(f,"  show grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"\n %% following 2 routines added by J. Allwright\n");
  fprintf(f,"/endy1 {  %% usage: y x1 x2 (str) endy1  - mark first ending\n");
  fprintf(f,"  4 -1 roll /yval exch def\n");
  fprintf(f,"  3 1 roll yval 4 add moveto 0 6 rlineto\n");
  fprintf(f,"  dup yval 10 add lineto 0 -6 rlineto stroke\n");
  fprintf(f,"  4 add yval moveto gsave /Times-Roman 13 selectfont 1.2 0.95 scale\n");
  fprintf(f,"  show grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/endy2 {  %% usage: y x1 x2 (str) endy2  - mark second ending\n");
  fprintf(f,"  4 -1 roll /yval exch def\n");
  fprintf(f,"  3 1 roll yval 10 add moveto\n");
  fprintf(f,"  dup yval 10 add lineto 0 -6 rlineto stroke\n");
  fprintf(f,"  4 add yval moveto gsave /Times-Roman 13 selectfont 1.2 0.95 scale\n");
  fprintf(f,"  show grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
}

static void section11(f)
FILE* f;
{
  fprintf(f,"%% Section 11 - Grace Notes\n");
  fprintf(f,"\n");
  fprintf(f,"/gn {  %% usage: x y gn  - grace note head only\n");
  fprintf(f,"  moveto\n");
  fprintf(f,"  -1.29 1.53 rmoveto\n");
  fprintf(f,"  2.45 2.06 5.02 -1.00 2.58 -3.06 rcurveto\n");
  fprintf(f,"  -2.45 -2.06 -5.02 1.00 -2.58 3.06 rcurveto\n");
  fprintf(f,"  fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/gn1 {  %% usage: x y l gnt  - grace note with tail\n");
  fprintf(f,"  3 1 roll 2 copy moveto\n");
  fprintf(f,"  -1.29 1.53 rmoveto\n");
  fprintf(f,"  2.45 2.06 5.02 -1.00 2.58 -3.06 rcurveto\n");
  fprintf(f,"  -2.45 -2.06 -5.02 1.00 -2.58 3.06 rcurveto\n");
  fprintf(f,"  fill moveto 2.00 0 rmoveto 0 exch rlineto\n");
  fprintf(f,"3 -4 4 -5 2 -8 rcurveto -5 2 rmoveto 7 4 rlineto   \n");
  fprintf(f,"stroke\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/gnt {  %% usage: x y l gnt  - grace note\n");
  fprintf(f,"  3 1 roll 2 copy moveto\n");
  fprintf(f,"  -1.29 1.53 rmoveto\n");
  fprintf(f,"  2.45 2.06 5.02 -1.00 2.58 -3.06 rcurveto\n");
  fprintf(f,"  -2.45 -2.06 -5.02 1.00 -2.58 3.06 rcurveto\n");
  fprintf(f,"  fill moveto 2.00 0 rmoveto 0 exch rlineto stroke\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/gbm2 {  %% usage: x1 y1 x2 y2 gbm2  - double note beam\n");
  fprintf(f,"  gsave 1.4 setlinewidth\n");
  fprintf(f,"  4 copy 0.5 sub moveto 0.5 sub lineto stroke\n");
  fprintf(f,"  3.4 sub moveto 3.4 sub lineto stroke grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/gbm3 {  %% usage: x1 y1 x2 y2 gbm3  - triple gnote beam\n");
  fprintf(f,"  gsave 1.2 setlinewidth\n");
  fprintf(f,"  4 copy 0.3 sub moveto 0.3 sub lineto stroke\n");
  fprintf(f,"  4 copy 2.5 sub moveto 2.5 sub lineto stroke\n");
  fprintf(f,"  4.7 sub moveto 4.7 sub lineto stroke grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/ghl {  %% usage: x y ghl  - grace note helper line\n");
  fprintf(f,"   gsave 0.7 setlinewidth moveto \n");
  fprintf(f,"   -3 0 rmoveto 6 0 rlineto stroke grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/gsl {  %% usage: x1 y2 x2 y2 x3 y3 x0 y0 gsl\n");
  fprintf(f,"  moveto curveto stroke\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/gsh0 {  %% usage: x y gsh0\n");
  fprintf(f,"gsave translate 0.7 0.7 scale 0 0 sh0 grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/gft0 {  %% usage: x y gft0\n");
  fprintf(f,"gsave translate 0.7 0.7 scale 0 0 ft0 grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/gnt0 {  %% usage: x y gnt0\n");
  fprintf(f,"gsave translate 0.7 0.7 scale 0 0 nt0 grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/gdf0 {  %% usage: x y gdf0\n");
  fprintf(f,"gsave translate 0.7 0.6 scale 0 0 dft0 grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/gds0 {  %% usage: x y gds0\n");
  fprintf(f,"gsave translate 0.7 0.7 scale 0 0 dsh0 grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
}

static void section12(f)
FILE* f;
{
  fprintf(f,"%% Section 12 - Time Signatures\n");
  fprintf(f,"\n");
  fprintf(f,"/csig {  %% usage:  x csig  - C timesig \n");
  fprintf(f,"  0 moveto\n");
  fprintf(f,"  1.00 17.25 rmoveto\n");
  fprintf(f,"  1.00 0.00 2.75 -0.75 2.75 -3.00 rcurveto\n");
  fprintf(f,"  0.00 1.50 -1.50 1.25 -1.50 0.00 rcurveto\n");
  fprintf(f,"  0.00 -1.25 1.75 -1.25 1.75 0.25 rcurveto\n");
  fprintf(f,"  0.00 2.50 -1.50 3.25 -3.00 3.25 rcurveto\n");
  fprintf(f,"  -3.75 0.00 -6.25 -2.75 -6.25 -6.50 rcurveto\n");
  fprintf(f,"  0.00 -3.00 3.75 -7.50 9.00 -2.50 rcurveto\n");
  fprintf(f,"  -4.25 -3.00 -7.25 -0.75 -7.25 2.50 rcurveto\n");
  fprintf(f,"  0.00 3.00 1.00 6.00 4.50 6.00 rcurveto\n");
  fprintf(f,"   fill\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/ctsig {  %% usage:  x ctsig  - C| timesig \n");
  fprintf(f,"  dup csig 4 moveto 0 16 rlineto stroke\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/sep0 { %% usage: x1 x2 sep0  - hline separator \n");
  fprintf(f,"   0 moveto 0 lineto stroke\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/tsig { %% usage: x (top) (bot) tsig -- draw time signature\n");
  fprintf(f,"   3 -1 roll 0 moveto\n");
  fprintf(f,"   gsave /Times-Bold 16 selectfont 1.2 1 scale\n");
  fprintf(f,"   0 1.0 rmoveto currentpoint 3 -1 roll cshow\n");
  fprintf(f,"   moveto 0 12 rmoveto cshow grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
}

static void section13(f)
FILE* f;
{
  fprintf(f,"%% Section 13 - Staff And Other things\n");
  fprintf(f,"\n");
  fprintf(f,"/staff {  %% usage: l staff  - draw staff\n");
  fprintf(f,"  gsave 0.5 setlinewidth 0 0 moveto\n");
  fprintf(f,"  dup 0 rlineto dup neg 6 rmoveto\n");
  fprintf(f,"  dup 0 rlineto dup neg 6 rmoveto\n");
  fprintf(f,"  dup 0 rlineto dup neg 6 rmoveto\n");
  fprintf(f,"  dup 0 rlineto dup neg 6 rmoveto\n");
  fprintf(f,"  dup 0 rlineto dup neg 6 rmoveto\n");
  fprintf(f,"  pop stroke grestore\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f, "/segno { %% segno symbol usage: x y segno\n");
  fprintf(f, "  gsave translate\n");
  fprintf(f, "  5.82 0.84 moveto\n");
  fprintf(f, "  4.62 1.15   5.04 3.20   4.6 3.64 curveto\n");
  fprintf(f, "  4.17 4.08   3.19 2.75   3.9 1.51 curveto\n");
  fprintf(f, "  4.62 0.27   7.45 0.0    8.03 1.82 curveto\n");
  fprintf(f, "  8.61 3.64   5.06 7.81   3.38 9.67 curveto\n");
  fprintf(f, "  1.69 11.53  1.59 14.46  2.79 14.16 curveto\n");
  fprintf(f, "  3.99 13.85  3.55 11.80  4.00 11.36 curveto\n");
  fprintf(f, "  4.44 10.92  5.33 12.25  4.66 13.49 curveto\n");
  fprintf(f, "  3.99 14.73  1.15 15.0   0.57 13.18 curveto\n");
  fprintf(f, "  0.0  11.36  3.50 7.19   5.21 5.32  curveto\n");
  fprintf(f, "  6.92 3.46   7.01 0.53   5.82 0.84 curveto\n");
  fprintf(f, "  closepath fill\n");
  fprintf(f, "  -0.43 2.93 moveto\n");
  fprintf(f, "  8.79 12.25 lineto 9.32 11.72 lineto -0.09 2.40 lineto\n");
  fprintf(f, "  closepath fill\n");
  fprintf(f, "  0.71 7.50 1.0 0 360 arc\n");
  fprintf(f, "  closepath fill\n");
  fprintf(f, "  7.85 7.50 1.0 0 360 arc\n");
  fprintf(f, "  closepath fill\n");
  fprintf(f, "  grestore\n");
  fprintf(f, "} bind def\n");
  fprintf(f, "\n");
  fprintf(f, "/coda { %% coda : usage x y coda\n");
  fprintf(f, "  gsave\n");
  fprintf(f, "  translate\n");
  fprintf(f, "  1.0 setlinewidth\n");
  fprintf(f, "  0.0 7.5 moveto 15.0 7.5 lineto stroke\n");
  fprintf(f, "  7.5 0.0 moveto 7.5 15.0 lineto stroke\n");
  fprintf(f, "  7.5 7.5 5.0 0 360 arc stroke\n");
  fprintf(f, "  grestore\n");
  fprintf(f, "} bind def\n");
  fprintf(f, "\n");
  fprintf(f,"/WS {   %%usage:  w nspaces str WS\n");
  fprintf(f,"   dup stringwidth pop 4 -1 roll\n");
  fprintf(f,"   sub neg 3 -1 roll div 0 8#040 4 -1 roll\n");
  fprintf(f,"   widthshow\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/W1 { show pop pop } bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/str 50 string def\n");
  fprintf(f,"\n");
  fprintf(f,"/W0 {\n");
  fprintf(f,"   dup stringwidth pop str cvs exch show (  ) show show pop pop\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/WC { counttomark 1 sub dup 0 eq { 0 }\n");
  fprintf(f,"  {  ( ) stringwidth pop neg 0 3 -1 roll\n");
  fprintf(f,"  {  dup 3 add index stringwidth pop ( ) stringwidth pop add\n");
  fprintf(f,"  dup 3 index add 4 index lt 2 index 1 lt or\n");
  fprintf(f,"  {3 -1 roll add exch 1 add} {pop exit} ifelse\n");
  fprintf(f,"  } repeat } ifelse\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/P1 {\n");
  fprintf(f,"  {  WC dup 0 le {exit} if\n");
  fprintf(f,"      exch pop gsave { exch show ( ) show } repeat grestore LF\n");
  fprintf(f,"   } loop pop pop pop pop\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"/P2 {\n");
  fprintf(f,"   {  WC dup 0 le {exit} if\n");
  fprintf(f,"      dup 1 sub dup 0 eq\n");
  fprintf(f,"      { pop exch pop 0.0 }\n");
  fprintf(f,"      { 3 2 roll 3 index exch sub exch div } ifelse\n");
  fprintf(f,"      counttomark 3 sub 2 index eq { pop 0 } if exch gsave\n");
  fprintf(f,"      {  3 2 roll show ( ) show dup 0 rmoveto } repeat\n");
  fprintf(f,"      grestore LF pop\n");
  fprintf(f,"   } loop pop pop pop pop\n");
  fprintf(f,"} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"0 setlinecap 0 setlinejoin 0.8 setlinewidth\n");
  fprintf(f,"\n");
  fprintf(f,"/T {translate} bind def\n");
  fprintf(f,"/M {moveto} bind def\n");
  fprintf(f,"/L {lineto stroke} bind def\n");
  fprintf(f,"\n");
  fprintf(f,"%% End of abc2ps Postscript Library\n");
  fprintf(f,"%%%%EndSetup\n");
  fprintf(f,"\n");
}

void printlib(f, filename, boundingbox)
FILE*f;
char* filename;
struct bbox* boundingbox;
{
  ps_header(f, filename, boundingbox);
  section1(f);
  section2(f);
  section3(f);
  section4(f);
  section5(f);
  section6(f);
  section7(f);
  section8(f);
  section9(f);
  section10(f);
  section11(f);
  section12(f);
  section13(f);
}

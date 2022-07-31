/* music_utils.c
 *
 * Copyright James Allwright 2020
 *
 * This file provides basic functions for manipulating music
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
 * along with this program; if not, see <https://www.gnu.org/licenses/>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "music_utils.h"

typedef struct clef_item
{
  char *name;
  basic_cleftype_t basic_clef;
  int staveline;
  int octave_offset;
} clef_item_t;

/* entries are name, basic_clef, staveline, octave_offset */
static const clef_item_t clef_conversion_table[] = {
  {"treble", basic_clef_treble, 2, 0},
  {"alto", basic_clef_alto, 3, 0},
  {"bass", basic_clef_bass, 4, 0},
  {"soprano", basic_clef_alto, 1, 0},
  {"mezzosoprano", basic_clef_alto, 2, 0},
  {"tenor", basic_clef_alto, 4, 0},
  {"baritone", basic_clef_bass, 0, 0},
  {"tab", tablature_not_implemented, 0, 0}, /* [SS] 2022.07.31 */
};
#define NUM_CLEFS (sizeof(clef_conversion_table)/sizeof(clef_item_t))

/* The following 3 are not really clefs, but abc standard
 * 2.2 allows them. We treat them as treble clef and only
 * allow them after clef= . This ensures that K:none is not
 * interpreted as a clef instead of a key signature.
 *
 * the clef defines how a pitch value maps to a y position
 * on the stave. If there is a clef of "none", then you don't
 * know where to put the notes!
 */
static const clef_item_t odd_clef_conversion_table[] = {
  {"auto", basic_clef_treble, 2, 0},
  {"perc", basic_clef_treble, 2, 0},
  {"none", basic_clef_treble, 2, 0}
};
#define NUM_ODD_CLEFS (sizeof(odd_clef_conversion_table)/sizeof(clef_item_t))


/* array giving notes in the order used by key_acc array 
 * This can be used to implement a lookup function which is
 * the reverse of note_index().
 */
const char note_array[] = "cdefgab";

/* These are musical modes. Each specifies a scale of notes starting on a
 * particular tonic note. Effectively the notes are the same as used in a
 * major scale, but the starting note is different. This means that each
 * mode can be notated using the standard key signatures used for major
 * keys. The abc standard specifies that only the first 3 characters of
 * the mode are significant, and further that "minor" can be abbreviated
 * as "m" and "major" omitted altogether. The full names are:
 * Major, Minor, Aeolian, Locrian, Ionian, Dorian, Phyrgian, Lydian and
 * Mixolydian. In addition, we have "exp" short for "explicit" to indicate
 * that arbitrary accidentals can be applied to each stave line in the
 * key signature and "" the empty string to represent "major" where this
 * is inferred as the default value rather than being supplied.
 */
const char *mode[12] = { "maj", "min", "m",
  "aeo", "loc", "ion", "dor", "phr", "lyd", "mix", "exp", ""
};

/* This is a table for finding the sharps/flats representation of
 * a key signature for a given mode.
 * Suppose we want to find the sharps/flats representation for
 * K:GDor
 * If we know the major mode key K:G is 1 sharp, and have the index of the
 * mode we want within the mode table, we can work out the new key
 * signature as follows:
 *
 * Original major mode key signature +1 (1 sharp)
 * Desired new mode Dorian is at position 6 in table
 * modeshift[6] is -2.
 * new key signature is 1 - 2 = -1 (1 flat)
 * GDor is 1 flat.
 */
const int modeshift[12] = { 0, -3, -3, -3, -5, 0, -2, -4, 1, -1, 0, 0 };

/* convert a note a-g to an index into the key signature arrays
 * src.key_acc, target.key_acc, src_key_mult, target_key_mult
 */
noteletter_t note_index (char note_ch)
{

  switch (note_ch) {
    case 'c':
    case 'C':
      return note_c;
    case 'd':
    case 'D':
      return note_d;
    case 'e':
    case 'E':
      return note_e;
    case 'f':
    case 'F':
      return note_f;
    case 'g':
    case 'G':
      return note_g;
    case 'a':
    case 'A':
      return note_a;
    case 'b':
    case 'B':
      return note_b;
    default:
      printf ("Internal error: note_index called with bad value %c\n", note_ch);
      exit (1);
  }
}

/* convert a letter a - g into a note value in semitones */
int semitone_value_for_note (noteletter_t note)
{
  switch (note) {
    case note_c:
      return 0;
    case note_d:
      return 2;
    case note_e:
      return 4;
    case note_f:
      return 5;
    case note_g:
      return 7;
    case note_a:
      return 9;
    case note_b:
      return 11;
    default:
      printf ("Internal error: Unexpected note %d\n", note);
      return 0;
  }
}

/* convert an accidental indicator into a semitone shift */
int semitone_shift_for_acc (char acc)
{
  switch (acc) {
    case '_':
    case 'b':
      return -1;
    case '^':
    case '#':
      return 1;
    default:
      return 0;
  }
}

/* Given a semitone value 0 - 11, convert to note + accidental.
 * This function always returns sharp as the accidental
 */
void note_for_semitone (int semitones, noteletter_t * note, char *accidental)
{
  char note_for_semi[12] = "CCDDEFFGGAAB";
  char acc_for_semi[12] = " # #  # # # ";

  if (semitones < 0) {
    semitones = -(-semitones) % 12 + 12;
  }
  if (semitones > 11) {
    semitones = semitones % 12;
  }
  *note = note_index (note_for_semi[semitones]);
  *accidental = acc_for_semi[semitones];
}

/* look for any octave shift specified after a clef name */
static int get_clef_octave_offset (char *clef_ending)
{
  if (strncmp (clef_ending, "+8", 2) == 0) {
    return 1;
  }
  if (strncmp (clef_ending, "+15", 2) == 0) {
    return 2;
  }
  if (strncmp (clef_ending, "-8", 2) == 0) {
    return -1;
  }
  if (strncmp (clef_ending, "-15", 2) == 0) {
    return -2;
  }
  return 0;
}

void init_new_clef (cleftype_t * new_clef)
{
  new_clef->basic_clef = basic_clef_undefined;
  new_clef->staveline = 0;
  new_clef->octave_offset = 0;
  new_clef->named = 0;
}

/* copy contents of cleftype_t structure */
void copy_clef (cleftype_t * target_clef, cleftype_t * source_clef)
{
  target_clef->basic_clef = source_clef->basic_clef;
  target_clef->staveline = source_clef->staveline;
  target_clef->octave_offset = source_clef->octave_offset;
  target_clef->named = source_clef->named;
}

/* use lookup table to get details of clef from it's name string */
//int get_standard_clef (char *name, basic_cleftype_t * basic_clef,
//                      int *staveline, int *octave_offset)
int get_standard_clef (char *name, cleftype_t * new_clef)
{
  int i;
  int len;

  for (i = 0; i < NUM_CLEFS; i++) {
    const clef_item_t *table_row = &clef_conversion_table[i];

    len = strlen (table_row->name);
    if (strncmp (name, table_row->name, len) == 0) {
      new_clef->basic_clef = table_row->basic_clef;
      new_clef->staveline = table_row->staveline;
      new_clef->named = 1;
      new_clef->octave_offset = get_clef_octave_offset (name + len);
      return 1;                 /* lookup succeeded */
    }
  }
  return 0;
}

/* look for a clef using C, F or G and a number 1 - 5  or
 * one of the specials (none, perc, auto)
 */
//int get_extended_clef_details (char *name, basic_cleftype_t * basic_clef,
//                      int *staveline, int *octave_offset)
int get_extended_clef_details (char *name, cleftype_t * new_clef)
{
  int i;
  int len;
  int num;
  int items_read;

  for (i = 0; i < NUM_ODD_CLEFS; i++) {
    const clef_item_t *table_row = &odd_clef_conversion_table[i];

    len = strlen (table_row->name);
    if (strncmp (name, table_row->name, len) == 0) {
      new_clef->basic_clef = table_row->basic_clef;
      new_clef->staveline = table_row->staveline;
      new_clef->octave_offset = table_row->octave_offset;
      new_clef->named = 1;
      new_clef->octave_offset = get_clef_octave_offset (name + len);
      return 1;                 /* lookup succeeded */
    }
  }
  new_clef->octave_offset = 0;
  /* try [C/F/G]{1-5] format */
  switch (name[0]) {
    case 'C':
      new_clef->basic_clef = basic_clef_alto;
      break;
    case 'F':
      new_clef->basic_clef = basic_clef_bass;
      break;
    case 'G':
      new_clef->basic_clef = basic_clef_treble;
      break;
    default:
      return 0;                 /* not recognized */
  }
  items_read = sscanf (&name[1], "%d", &num);
  if ((items_read == 1) && (num >= 1) && (num <= 5)) {
    /* we have a valid clef specification */
    new_clef->staveline = num;
    new_clef->named = 0;
    new_clef->octave_offset = get_clef_octave_offset (name + 2);
    return 1;
  }
  return 0;
}

static void append_octave_offset(char * name, int octave_offset)
{
  switch (octave_offset) {
    case -2:
      strcat(name, "-15");
      break;
    case -1:
      strcat(name, "-8");
      break;
    case 1:
      strcat(name, "+8");
      break;
    case 2:
      strcat(name, "+15");
      break;
    default:
      break;
  }
}

/* given clef basic type and staveline, we try to work out it's name */
int get_clef_name (cleftype_t * new_clef, char *name)
{
  int i;

  if (new_clef->named) {
    for (i = 0; i < NUM_CLEFS; i++) {
      if ((clef_conversion_table[i].basic_clef == new_clef->basic_clef) &&
          (clef_conversion_table[i].staveline == new_clef->staveline)) {
        strcpy (name, clef_conversion_table[i].name);
        append_octave_offset(name, new_clef->octave_offset);
        return 1;               /* lookup succeeded */
      }
    }
  }
  switch (new_clef->basic_clef) {
    default:
    case basic_clef_undefined:
    case basic_clef_none :
      return 0;
    case basic_clef_auto :
      strcpy(name, "auto");
      return 1;
    case basic_clef_perc :
      strcpy(name, "perc");
      return 1;
    case basic_clef_treble:
      name[0] = 'G';
      break;
    case basic_clef_bass:
      name[0] = 'F';
      break;
    case basic_clef_alto:
      name[0] = 'C';
      break;
  }
  name[1] = '0' + new_clef->staveline;
  name[2] = '\0';
  append_octave_offset(name, new_clef->octave_offset);
  return 1;
}

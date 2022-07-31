/* music_utils.h
 *
 * Copyright James Allwright 2020
 *
 * part of abc2abc/Toadflax
 *
 * header file to export basic music manipulation functions.
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
#ifndef MUSIC_UTILS_H
#define MUSIC_UTILS_H 1

extern const char note_array[];

typedef enum noteletter {
  note_c = 0,
  note_d = 1,
  note_e = 2,
  note_f = 3,
  note_g = 4,
  note_a = 5,
  note_b = 6
} noteletter_t;

#define MODE_DEFAULT_MAJOR 11
#define MODE_EXP 10

/* There are only 3 different types of drawn clef. The other clefs are
 * obtained by drawing one of the basic clefs, but sitting on a different
 * stave line. For convenience, the abc standard 2.2 numbers these lines
 * 1 to 5, with the bottom line being 1 and the top line being 5.
 */
typedef enum basic_cleftype {
  basic_clef_treble,
  basic_clef_bass,
  basic_clef_alto,
  basic_clef_undefined, /* for when we didn't find a clef */
  basic_clef_auto, /* drawing program has free choice of clef */
  basic_clef_perc, /* percussion */
  basic_clef_none,  /* from abc standard 2.2 what does this mean ? */
  tablature_not_implemented  /* [SS] 2022.07.31 */
} basic_cleftype_t;

typedef struct new_cleftype {
  basic_cleftype_t basic_clef;
  int staveline;
  int octave_offset;
  int named;
} cleftype_t;

extern const char *mode[12];
extern const int modeshift[12];

/* note operations */
noteletter_t note_index(char note_ch);
int semitone_value_for_note(noteletter_t note);
int semitone_shift_for_acc(char acc);
void note_for_semitone(int semitones, noteletter_t *note, char *accidental);

/* clef operations */
void init_new_clef(cleftype_t *new_clef);
void copy_clef(cleftype_t *target_clef, cleftype_t *source_clef);
int get_standard_clef (char *name, cleftype_t *new_clef);
int get_extended_clef_details (char *name, cleftype_t *new_clef);
int get_clef_name (cleftype_t *new_clef, char *name);

#endif /* MUSIC_UTILS_H 1 */

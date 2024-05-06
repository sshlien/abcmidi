/* parser2.c - further parsing */
/* part of abc2midi and yaps */
/* implements event_info, event_instruction, event_gchord and event_slur */
/* also implements event_reserved */
/* parser2 requires parseabc, but parseabc does not need parser2 */

#include <stdio.h>
#include "abc.h"
#include "parseabc.h"
#include "parser2.h"

/* [JA] 2024-04-30 */
void event_info(char* s)
/* An I: field has been encountered. The 2.1 and 2.2 abc standards
   specify that this field (stylesheet directive) is handled in the
   same way as %% (pseudo-comment). If it isn't recognized it should
   be silently ignored.
*/
{
  char package[40];
  char *p;

  p = s;
  skipspace(&p);
  readstr (package, &p, 40);
  event_specific (package, p, 1);
}


static void splitstring(s, sep, handler)
/* breaks up string into fields with sep as the field separator */
/* and calls handler() for each sub-string */
char* s;
char sep;
void (*handler)();
{
  char* out;
  char* p;
  int fieldcoming;

  p = s;
  fieldcoming = 1;
  while (fieldcoming) {
    out = p;
    while ((*p != '\0') && (*p != sep)) p = p + 1;
    if (*p == sep) {
      *p = '\0';
      p = p + 1;
    } else {
      fieldcoming = 0;
    };
    (*handler)(out);
  };
}

void event_gchord(s)
/* handles guitar chords " ... " */
char* s;
{
  splitstring(s, ';', event_handle_gchord);
}

void event_instruction(s)
/* handles a ! ... ! event in the abc */
char* s;
{
  splitstring(s, ';', event_handle_instruction);
}

void event_slur(t)
int t;
/* handles old 's' notation for slur on/slur off */
{
  if (t) {
    event_sluron(1);
  } else {
    event_sluroff(0);
  };
}

void event_reserved(char s)
/* handles H - Z encountered in abc */
{
  char *expansion;

  expansion = lookup_abbreviation(s);
  if (expansion != NULL) {
    event_handle_instruction(expansion);
  } else {
    event_x_reserved(s);
  };
}

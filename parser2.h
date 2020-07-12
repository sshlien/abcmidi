/* parser2.h */
/* part of abc2midi and yaps */
/* provides further parsing of I: info field */
/* and multiple instruction and gchord fields */

#ifndef KANDR
extern void event_info_key(char *key, char *value);
extern void event_handle_gchord(char *s);
extern void event_handle_instruction(char *s);
extern void event_x_reserved(char s);
#else
extern void event_info_key();
extern void event_handle_gchord();
extern void event_handle_instruction();
extern void event_x_reserved();
#endif

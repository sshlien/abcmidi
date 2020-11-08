/* drawtune.h - part of yaps */
/* this file declares the routines in drawtune.c that are used elsewhere */

/* for Microsoft Visual C++ version 6.0 or higher */

extern int eps_out;
/* bounding box for encapsulated PostScript */
struct bbox {
  int llx, lly, urx, ury;
};

/* may add .ps or <N>.eps to outputroot to get outputname [JA] 2020-11-01 */
#define MAX_OUTPUTROOT 250
#define MAX_OUTPUTNAME (MAX_OUTPUTROOT + 20)
extern char outputroot[MAX_OUTPUTROOT + 1];
extern char outputname[MAX_OUTPUTNAME + 1];

#ifdef ANSILIBS
extern void setmargins(char* s);
extern void setpagesize(char* s);
extern void open_output_file(char* filename, struct bbox* boundingbox);
extern void close_output_file(void);
extern void newpage(void);
extern void centretext(char* s);
extern void lefttext(char* s);
extern void vskip(double gap);
#else
extern void setmargins();
extern void setpagesize();
extern void open_output_file();
extern void close_output_file();
extern void newpage();
extern void centretext();
extern void lefttext();
extern void vskip();
#endif

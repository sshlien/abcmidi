/* drawtune.h - part of yaps */
/* this file declares the routines in drawtune.c that are used elsewhere */

/* for Microsoft Visual C++ version 6.0 or higher */

extern int eps_out;
/* bounding box for encapsulated PostScript */
struct bbox {
  int llx, lly, urx, ury;
};
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

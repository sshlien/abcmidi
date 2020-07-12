/*
 * queues.h - part of abc2midi
 */

/* Notes due to finish in the future are held in a queue (linked list)
 * in time order. Qhead points to the head of the list and addtoQ() 
 * adds a note to the list. The unused elements of array Q are held
 * in another linked list pointed to by freehead. The tail is pointed
 * to by freetail. removefromQ() removes an element (always from the
 * head of the list) and adds it to the free list. Qinit() initializes
 * the queue and clearQ() outputs all the remaining notes at the end
 * of a track.
 * Qcheck() and PrintQ() are diagnostic routines.
 */

/* queue for notes waiting to end */
/* allows us to do general polyphony */
extern long delta_time, tracklen;
extern long delta_time_track0; /* [SS] 2010-06-27 */
extern int div_factor;

/* routines to handle note queue */
#ifndef KANDR
void addtoQ(int num, int denom, int pitch, int chan, int effect, int d);
void removefromQ(int i);
void clearQ(void);
void printQ(void);
void advanceQ(int t);
void Qinit(void);
/* void Qcheck(); */
void timestep(int t, int atend);
#else
void addtoQ();
void removefromQ();
void clearQ();
void printQ();
void advanceQ();
void Qinit();
/* void Qcheck(); */
void timestep();
#endif

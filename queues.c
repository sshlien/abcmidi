/*
 * abc2midi - program to convert abc files to MIDI files.
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

/* queues.c
 * This file is part of the code for abc2midi.
 * Notes due to finish in the future are held in a queue (linked list)
 * in time order. Qhead points to the head of the list and addtoQ() 
 * adds a note to the list. The unused elements of array Q are held
 * in another linked list pointed to by freehead. The tail is pointed
 * to by freetail. removefromQ() removes an element (always from the
 * head of the list) and adds it to the free list. Qinit() initializes
 * the queue and clearQ() outputs all the remaining notes at the end
 * of a track.
 * Qcheck() and PrintQ() are diagnostic routines.
 */

#include <stdio.h>
#include <stdlib.h>
#include "queues.h"
#include "abc.h"
#include "genmidi.h"
#include "midifile.h"

void write_event_with_delay(int delta,int event_type,int channel,char* data,int n);

/* queue for notes waiting to end */
/* allows us to do general polyphony */
#define QSIZE 50
struct Qitem {
  int delay;
  int pitch;
  int chan;
  int effect;  /* [SS] 2012-12-11 */
  int next;
};
struct Qitem Q[QSIZE+1];
int Qhead, freehead, freetail;
extern int totalnotedelay; /* from genmidi.c [SS] */
extern int notedelay;      /* from genmidi.c [SS] */
extern int bendvelocity;   /* from genmidi.c [SS] */
extern int bendacceleration; /* from genmidi.c [SS] */
extern int bendstate; /* from genmidi.c [SS] */
/* [SS] 2014-09-10 */
extern int benddata[256]; /* from genmidi.c [SS] 2015-09-10 2015-10-03 */
extern int bendnvals;
extern int controldata[3][256]; /* extended to 256 2015-10-03 */
extern int controlnvals[2];
extern int controldefaults[128]; /* [SS] 2015-08-10 */
extern int nlayers; /* [SS] 2015-08-20 */

void set_control_defaults() {
    int i;
    for (i=0;i<128;i++) controldefaults[i] = 0;
    controldefaults[7] = 96; /* volume coarse */
    controldefaults[8] = 64;  /* balance coarse */
    controldefaults[11] = 127; /* expression */
    controldefaults[10] = 64; /* pan position coarse */
}

/* routines to handle note queue */

/* genmidi.c communicates with queues.c mainly through the    */
/* functions addtoQ and timestep. The complexity comes in the */
/* handling of chords. When another note in a chord is passed,*/
/* addtoQ detemines whether other notes in the Q structure    */
/* overlap in time with this chord and modifies the delay item*/
/* of the note which finish later so that it is relative to the*/
/* end of the earlier note. Normally all notes in the chord end*/
/* at the same as specifiedy abc standard, so the delay of the*/
/* other notes cached in the Q structure should be set to zero.*/

void addtoQ(num, denom, pitch, chan, effect, d)
int num, denom, pitch, chan, d;
int effect; /* [SS] 2012-12-11 */
{
  int i, done;
  int wait;
  int *ptr;

  /*printf("addtoQ: %d %d %d %d %d\n",num,denom,pitch,chan,d); */

  wait = ((div_factor*num)/denom) + d;
  /* find free space */
  if (freehead == -1) {
    /* printQ(); */
    event_error("Too many notes in chord - probably missing ']' or '+'");
    return;
  } else {
    i = freehead;
    freehead = Q[freehead].next;
  };
  Q[i].pitch = pitch;
  Q[i].chan = chan;
  Q[i].effect = effect;  /* [SS] 2012-12-11 */
  /* find place in queue */
  ptr = &Qhead;
  done = 0;
  while (!done) {
    if (*ptr == -1) {
      *ptr = i;
      Q[i].next = -1;
      Q[i].delay = wait;
      done = 1;
    } else {
      if (Q[*ptr].delay > wait) {
        Q[*ptr].delay = Q[*ptr].delay - wait -notedelay;
        if (Q[*ptr].delay < 0) Q[*ptr].delay = 0;
        Q[i].next = *ptr;
        Q[i].delay = wait;
        *ptr = i;
        done = 1;
      } else {
        wait = wait - Q[*ptr].delay;
        ptr = &Q[*ptr].next;
      };
    };
  };
}

void removefromQ(i)
int i;
{
  if (i == -1) {
    printQ();
    event_fatal_error("Internal error - nothing to remove from queue");
  } else {
    if (Q[Qhead].delay != 0) {
    printQ();
      event_fatal_error("Internal error - queue head has non-zero time");
    };
    Qhead = Q[i].next;
    Q[i].next = freehead;
    freehead = i;
  };
}

void clearQ()
{
  int time;
  int i;

  /* remove gchord requests */
  time = 0;
  while ((Qhead != -1) && (Q[Qhead].pitch == -1)) {
    time = time + Q[Qhead].delay;
    i = Qhead;
    Qhead = Q[i].next;
    Q[i].next = freehead;
    freehead = i;
  };
  if (Qhead != -1) {
    timestep(time, 1);
  };
  /* do any remaining note offs, but don't do chord request */
  while (Qhead != -1) {
    event_error("Sustained notes beyond end of track");
    timestep(Q[Qhead].delay+1, 1);
  };
timestep(25,0); /* to avoid transient artefacts at end of track */
}

void printQ()
{
  int t;

  printf("Qhead = %d freehead = %d freetail = %d\n", 
         Qhead, freehead, freetail);
  t = Qhead;
  printf("Q:");
  while (t != -1) {
    printf("p(%d)-%d->", Q[t].pitch, Q[t].delay);
    t = Q[t].next;
  };
  printf("\n");
}

void advanceQ(t)
int t;
{
  if (Qhead == -1) {
    event_error("Internal error - empty queue");
  } else {
    Q[Qhead].delay = Q[Qhead].delay - t;
  };
}

void Qinit()
{
  int i;

  /* initialize queue of notes waiting to finish */
  Qhead = -1;
  freehead = 0;
  for (i=0; i<QSIZE-1; i++) {
    Q[i].next = i + 1;
  };
  Q[QSIZE-1].next = -1;
  freetail = QSIZE-1;
}

void Qcheck()
{
  int qfree, qused;
  int nextitem;
  int used[QSIZE];
  int i;
  int failed;

  failed = 0;
  for (i=0; i<QSIZE; i++) {
    used[i] = 0;
  };
  qused = 0;
  nextitem = Qhead;
  while (nextitem != -1) {
    qused = qused + 1;
    used[nextitem] = 1;
    nextitem = Q[nextitem].next;
    if ((nextitem < -1) || (nextitem >= QSIZE)) {
      failed = 1;
      printf("Queue corrupted Q[].next = %d\n", nextitem);
    };
  };
  qfree = 0;
  nextitem = freehead;
  while (nextitem != -1) {
    qfree = qfree + 1;
    used[nextitem] = 1;
    nextitem = Q[nextitem].next;
    if ((nextitem < -1) || (nextitem >= QSIZE)) {
      failed = 1;
      printf("Free Queue corrupted Q[].next = %d\n", nextitem);
    };
  };
  if (qfree + qused < QSIZE) {
    failed = 1;
    printf("qfree = %d qused = %d\n", qused, qfree);
  };
  for (i=0; i<QSIZE; i++) {
    if (used[i] == 0) {
      printf("Not used element %d\n", i);
      failed = 1;
    };
  };
  if (Q[freetail].next != -1) {
    printf("freetail = %d, Q[freetail].next = %d\n", freetail, 
           Q[freetail].next);
  };
  if (failed == 1) {
    printQ();
    event_fatal_error("Qcheck failed");
  };
}


/* [SS] 2012-12-11 */
void note_effect() {
/* note_effect is used only if !bend! is called without
   calling %%MIDI bendstring or %%MIDI bendvelocity [SS] 2015-08-11 */
/* Bends a note along the shape of a parabola. The note is
   split into 8 segments. Given the bendacceleration and
   initial bend velocity, the new pitch bend is computed
   for each time segment.
*/
  int delta8;
  int pitchbend;
  char data[2];
  int i;
  int velocity;
  delta8 = delta_time/8;
  pitchbend = bendstate;  /* [SS] 2014-09-09 */
  velocity = bendvelocity;
  for (i=0;i<8;i++) {
     pitchbend = pitchbend + velocity;
     velocity = velocity + bendacceleration;
     if (pitchbend > 16383) pitchbend = 16383;
     if (pitchbend < 0) pitchbend = 0;
 
     data[0] = (char) (pitchbend&0x7f);
     data[1] = (char) ((pitchbend>>7)&0x7f);
     mf_write_midi_event(delta8,pitch_wheel,Q[Qhead].chan,data,2);
     delta_time -= delta8;
     }
  midi_noteoff(delta_time, Q[Qhead].pitch, Q[Qhead].chan);
  pitchbend = bendstate; /* [SS] 2014-09-22 */
  data[0] = (char) (pitchbend&0x7f);
  data[1] = (char) ((pitchbend>>7)&0x7f);
  mf_write_midi_event(delta_time,pitch_wheel,Q[Qhead].chan,data,2);
  }

/* [SS] 2014-09-11 */
void note_effect2() {
  /* implements %%MIDI bendstring command. The note is split
     into n segments where n is the number of pitch shift values
     supplied in bendstring. For every segment, the pitchbend
     is shifted by the next pitch shift value.
  */
  int delta;
  int pitchbend;
  char data[2];
  int i;
  delta = delta_time/bendnvals;
  pitchbend = bendstate;  /* [SS] 2014-09-09 */
  for (i=0;i<bendnvals;i++) {
     pitchbend = pitchbend + benddata[i];
     if (pitchbend > 16383) pitchbend = 16383;
     if (pitchbend < 0) pitchbend = 0;
 
     data[0] = (char) (pitchbend&0x7f);
     data[1] = (char) ((pitchbend>>7)&0x7f);
     if (i == 0) /* [SS] 2014-09-24 */
       mf_write_midi_event(0,pitch_wheel,Q[Qhead].chan,data,2);
     else {
       mf_write_midi_event(delta,pitch_wheel,Q[Qhead].chan,data,2);
       delta_time -= delta;
       }
     }
  midi_noteoff(delta_time, Q[Qhead].pitch, Q[Qhead].chan);
  delta_time = 0; /* [SS] 2014-09-24 */
  pitchbend = bendstate; /* [SS] 2014-09-09 */
  data[0] = (char) (pitchbend&0x7f);
  data[1] = (char) ((pitchbend>>7)&0x7f);
  mf_write_midi_event(delta_time,pitch_wheel,Q[Qhead].chan,data,2);
  }

/* [SS] 2014-09-22 */
void note_effect3() {
  /* handles the %%MIDI bendstring command when only one pitch
     shift is given.
  */
  int delta;
  int pitchbend;
  char data[2];
  delta =0;
  pitchbend = bendstate; 
  pitchbend = pitchbend + benddata[0];
  if (pitchbend > 16383) pitchbend = 16383;
  if (pitchbend < 0) pitchbend = 0;
  data[0] = (char) (pitchbend&0x7f);
  data[1] = (char) ((pitchbend>>7)&0x7f);
  mf_write_midi_event(delta,pitch_wheel,Q[Qhead].chan,data,2);
  midi_noteoff(delta_time, Q[Qhead].pitch, Q[Qhead].chan);
  pitchbend = bendstate;
  data[0] = (char) (pitchbend&0x7f);
  data[1] = (char) ((pitchbend>>7)&0x7f);
  mf_write_midi_event(delta,pitch_wheel,Q[Qhead].chan,data,2); /* [SS] 2014-09-23 */
  } 

/* [SS] 2015-07-26 */
void note_effect4(chan)
    int chan;
    {
    /* Like %%MIDI bendstring, this procedure handles %%MIDI controlstring
       command. The first value of controlstring indicates the controller
       to modify. The following values are the values to send to the
       controller for each note segment.

       This procedure is no longer used. Presently note_effect5()
       has taken over this operation.
    */
    int delta;
    char data[2];
    int controltype,controlval;
    int i;
    if (controlnvals[0] <= 1) {
        event_error("missing %%MIDI controlstring ...");
        return;
        }
    delta = delta_time/(controlnvals[0]-1);
    controltype = controldata[0][0];
    if (controltype > 127 || controltype < 0) {
        event_error("control code muste be in range 0 to 127");
        return;
       }

    for (i = 1;i<controlnvals[0];i++)
        {
        controlval = controldata[0][i]; 
        if (controlval >127 || controlval < 0) {
            event_error("control code muste be in range 0 to 127");
            controlval = 0;            
            }
        data[0] = controltype;
        data[1] = controlval;
        write_event_with_delay(delta,control_change,chan,data,2);
        delta_time -= delta;
        }
    midi_noteoff(delta_time, Q[Qhead].pitch, Q[Qhead].chan);
    }


/* [SS] 2015-07-30 */
struct eventstruc {int time;
                   char cmd;
                   char data1;
                   char data2;};

int int_compare_events(const void *a, const void *b) {
   struct eventstruc *ia  = (struct eventstruc *)a;
   struct eventstruc *ib  = (struct eventstruc *)b;
   if (ib->time > ia->time) 
       return -1;
   else if (ia ->time > ib->time)
       return  1;
   else return 0; 
   }

void print_eventlist(struct eventstruc *list, int nsize) {
    int i;
    for (i = 0; i<nsize; i++) 
        printf("%d %d %d %d\n",list[i].time,list[i].cmd,list[i].data1,list[i].data2);
    }

/* [SS] 2015-08-03 */
void output_eventlist (struct eventstruc *list, int nsize, int chan) {
    int i;
    int delta;
    char cmd,data[2];
    int miditime;
    miditime = 0;
    for (i = 0; i<nsize; i++) {
        delta = list[i].time - miditime;
        miditime = list[i].time;
        data[0] = list[i].data1; 
        data[1] = list[i].data2;
        cmd     = list[i].cmd;
        if (cmd == -80) write_event_with_delay(delta,control_change,chan,data,2);
        if (cmd == -32) mf_write_midi_event(delta,pitch_wheel,Q[Qhead].chan,data,2);
        }
    } 

/* [SS] 2015-08-01 */
void note_effect5(chan)
    int chan;
    {
    /* This procedure merges the controlstring with the
       bendstring and uses it to shape the note. The control
       commands are prepared and stored in the eventstruc
       eventlist[] array, sorted chronologically and sent
       to the MIDI file.
    */
    struct eventstruc eventlist[1024]; /* extended to 1000 2015-10-03 */
    int delta=0,notetime,pitchbend; /* [SDG] 2020-06-03 */ 
    int last_delta; /* [SS] 2017-06-10 */
    int initial_bend; /* [SS] 2015-08-25 */
    int i,j;
    int layer;
    int controltype,controlval;
    char data[2];

    j = 0; 
    pitchbend = bendstate;
    initial_bend = bendstate;
    if (bendnvals > 0) {
        delta = delta_time/bendnvals;
        notetime = 0;
        for (i = 0; i <bendnvals; i++) {
            pitchbend = benddata[i] + pitchbend;
            if (pitchbend > 16383) {
               event_error("pitchbend exceeds 16383");
               pitchbend = 16383;
               }
            if (pitchbend < 0) {
               event_error("pitchbend is less than 0");
               pitchbend = 0;
               }
            eventlist[j].time = notetime;
            eventlist[j].cmd = pitch_wheel;
            eventlist[j].data1 = pitchbend & 0x7f;
            eventlist[j].data2 = (pitchbend >> 7) & 0x7f;
            notetime += delta;
            j++;
            } 
      }
 
    for (layer=0;layer <= nlayers;layer++) {
        if (controlnvals[layer] > 1) {
            delta = delta_time/(controlnvals[layer] -1);
            notetime = 0;
            controltype = controldata[layer][0];
            if (controltype > 127 || controltype < 0) {
                event_error("controller must be in range 0 to 127");
                return;
                }
    
            for (i = 1; i <controlnvals[layer]; i++) {
                controlval = controldata[layer][i];
                if (controlval < 0) {
                    event_error("control data must be zero or greater");
                    controlval = 0;
                    }
                if (controlval > 127) {
                    event_error("control data must be less or equal to 127");
                    controlval = 127;
                    }
                eventlist[j].time = notetime;
                eventlist[j].cmd  = control_change;
                eventlist[j].data1 = controltype;
                eventlist[j].data2 = controlval;
                notetime += delta;
                if (j < 1023) j++; /* [SS] increased to 999 2015-10-04 */
                else event_error("eventlist too complex");
               
                }
            }         
        }

    /* [SS] 2015-08-23 */
    if (j > 1) {
        qsort(eventlist,j,sizeof(struct eventstruc),int_compare_events);
    /*print_eventlist(eventlist,j); */
        }
    output_eventlist(eventlist,j,chan);

    /* [SS] 2015-08-28 */
    last_delta = delta - eventlist[j-1].time; /* [SS] 2017-06-10 */
    midi_noteoff(last_delta, Q[Qhead].pitch, Q[Qhead].chan);

    for (layer=0;layer <= nlayers;layer++) {
        controltype = controldata[layer][0];
        data[0] = (char) controltype;
        data[1] = (char) controldefaults[controltype];
        write_event_with_delay(0,control_change,chan,data,2);
        }
    if (bendnvals > 0) {
      /* restore pitchbend to its original state */
      pitchbend = initial_bend; /* [SS] 2014-09-25 */
      data[0] = (char) (pitchbend&0x7f);
      data[1] = (char) ((pitchbend>>7)&0x7f);
      write_event_with_delay(0,pitch_wheel,chan,data,2);
      }
   }




/* timestep is called by delay() in genmidi.c typically at the */
/* end of a note, chord or rest. It is also called by clearQ in*/
/* this file. Timestep, is not only responsible for sending the*/
/* midi_noteoff command for any expired notes in the Q structure*/
/* but also maintains the delta_time global variable which is  */
/* shared with genmidi.c. Timestep also calls advanceQ() in   */
/* this file which updates all the delay variables for the items */
/* in the Q structure to reflect the current MIDI time. Timestep */
/* also calls removefromQ in this file which cleans out expired */
/* notes from the Q structure. To make things even more complicated*/
/* timestep runs the dogchords and the dodrums for bass/chordal */
/* and drum accompaniments by calling the function progress_sequence.*/
/* Dogchords and dodrums may also call addtoQ changing the contents*/
/* of the Q structure array. The tracklen variable in MIDI time */
/* units is also maintained here.                               */ 

/* new: delta_time_track0 is declared in queues.h like delta_time */

void timestep(t, atend)
int t;
int atend;
{
  int time;
  int headtime;

  time = t;
  /* process any notes waiting to finish */
  while ((Qhead != -1) && (Q[Qhead].delay < time)) {
    headtime = Q[Qhead].delay;
    delta_time = delta_time + (long) headtime;
    delta_time_track0 = delta_time_track0 + (long) headtime; /* [SS] 2010-06-27*/
    time = time - headtime;
    advanceQ(headtime);
    if (Q[Qhead].pitch == -1) {
      if (!atend) {
        progress_sequence(Q[Qhead].chan);
      };
    } else {
       if (Q[Qhead].effect == 0) {
          midi_noteoff(delta_time, Q[Qhead].pitch, Q[Qhead].chan);
          tracklen = tracklen + delta_time;
          delta_time = 0L;}
       else {
          tracklen = tracklen + delta_time; /* [SS] 2015-06-08 */
          switch (Q[Qhead].effect) { /* [SS] 2014-09-22 */
             case 1:
                note_effect();
                break;
   
             case 2:
                note_effect2();
                break;

             case 3:
                note_effect3();
                break;

             case 10:
                /*note_effect4(Q[Qhead].chan);*/
                note_effect5(Q[Qhead].chan);
                break;
           } 
          delta_time = 0L;}
       };

    removefromQ(Qhead);
  };
  if (Qhead != -1) {
    advanceQ(time);
  };
  delta_time = delta_time + (long)time - totalnotedelay;
  delta_time_track0 = delta_time_track0 + (long)time - totalnotedelay; /* [SS] 2010-06-27*/
}


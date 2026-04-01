/* definitions for MIDI file parsing code */
#include <stdio.h>
extern int (*Mf_getc)();
extern void (*Mf_header)();
extern void (*Mf_trackstart)();
extern void (*Mf_trackend)();
extern void (*Mf_noteon)(int, int, int);
extern void (*Mf_noteoff)(int, int, int);
extern void (*Mf_pressure)(int, int, int);
extern void (*Mf_parameter)(int, int, int);
extern void (*Mf_pitchbend)(int, int, int);
extern void (*Mf_program)(int, int);
extern void (*Mf_chanpressure)(int, int, int);
extern void (*Mf_sysex)(int leng, char *mess);
extern void (*Mf_metamisc)(int, int, char *);
extern void (*Mf_seqspecific)();
extern void (*Mf_seqnum)();
extern void (*Mf_text)();
extern void (*Mf_eot)();
extern void (*Mf_timesig)(int,int,int,int);
extern void (*Mf_smpte)(int,int,int,int,int);
extern void (*Mf_tempo)();
extern void (*Mf_keysig)(int, int);
extern void (*Mf_arbitrary)();
extern void (*Mf_error)(char *);
extern long Mf_currtime;
extern int Mf_nomerge;

/* definitions for MIDI file writing code */
extern int (*Mf_putc)(char c);
extern long (*Mf_writetrack)();
extern int (*Mf_writetempotrack)();
float mf_ticks2sec(long ticks, int division, long tempo);
long mf_sec2ticks(float secs, int division, long tempo);
void mfwrite(int format, int ntracks, int division, FILE *fp);
void mfread();
int mf_write_meta_event(long delta_time, int type, char *data, int size);
int mf_write_midi_event (long delta_time, int type, int chan, char *data, int size);
void mf_write_tempo(long);
void mferror(char *);

/* MIDI status commands most significant bit is 1 */
#define note_off         	0x80
#define note_on          	0x90
#define poly_aftertouch  	0xa0
#define control_change    	0xb0
#define program_chng     	0xc0
#define channel_aftertouch      0xd0
#define pitch_wheel      	0xe0
#define system_exclusive      	0xf0
#define delay_packet	 	(1111)

/* 7 bit controllers */
#define damper_pedal            0x40
#define portamento	        0x41 	
#define sostenuto	        0x42
#define soft_pedal	        0x43
#define general_4               0x44
#define	hold_2		        0x45
#define	general_5	        0x50
#define	general_6	        0x51
#define general_7	        0x52
#define general_8	        0x53
#define tremolo_depth	        0x5c
#define chorus_depth	        0x5d
#define	detune		        0x5e
#define phaser_depth	        0x5f

/* parameter values */
#define data_inc	        0x60
#define data_dec	        0x61

/* parameter selection */
#define non_reg_lsb	        0x62
#define non_reg_msb	        0x63
#define reg_lsb		        0x64
#define reg_msb		        0x65

/* Standard MIDI Files meta event definitions */
#define	meta_event		0xFF
#define	sequence_number 	0x00
#define	text_event		0x01
#define copyright_notice 	0x02
#define sequence_name    	0x03
#define instrument_name 	0x04
#define lyric	        	0x05
#define marker			0x06
#define	cue_point		0x07
#define channel_prefix		0x20
#define	end_of_track		0x2f
#define	set_tempo		0x51
#define	smpte_offset		0x54
#define	time_signature		0x58
#define	key_signature		0x59
#define	sequencer_specific	0x74

/* Manufacturer's ID number */
#define Seq_Circuits (0x01) /* Sequential Circuits Inc. */
#define Big_Briar    (0x02) /* Big Briar Inc.           */
#define Octave       (0x03) /* Octave/Plateau           */
#define Moog         (0x04) /* Moog Music               */
#define Passport     (0x05) /* Passport Designs         */
#define Lexicon      (0x06) /* Lexicon 			*/
#define Tempi        (0x20) /* Bon Tempi                */
#define Siel         (0x21) /* S.I.E.L.                 */
#define Kawai        (0x41) 
#define Roland       (0x42)
#define Korg         (0x42)
#define Yamaha       (0x43)

/* miscellaneous definitions */
#define MThd 0x4d546864
#define MTrk 0x4d54726b
#define lowerbyte(x) ((unsigned char)(x & 0xff))
#define upperbyte(x) ((unsigned char)((x & 0xff00)>>8))

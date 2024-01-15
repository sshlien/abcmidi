abcMIDI :   abc <-> MIDI conversion utilities

midi2abc version 3.59 February 08 2023
abc2midi version 4.85 December 23 2023
abc2abc  version 2.20 February 07 2023
yaps     version 1.92 January 06 2023
abcmatch version 1.82 June 14 2022
midicopy version 1.39 November 08 2022
midistats version 0.85 January 04 2024

24th January 2002
Copyright James Allwright
jamesallwright@yahoo.co.uk
University of Westminster,
London, UK

August 2023
Copyright Seymour Shlien
fy733@ncf.ca
Ottawa, Canada

This is free software. You may copy and re-distribute it under the terms of 
the GNU General Public License version 2 or later, which is available from
the Free Software Foundation (and elsewhere). 

This package is to be found on the web at

http://abc.sourceforge.net/abcMIDI/
(The latest versions for the time being is found on
 ifdo.ca/~seymour/runabc/top.html.)

Note, if you have difficulty compiling the package because you do not have
snprintf see the note in doc/CHANGES dated January 08 2005 (and also
December 17 2004).

These programs make use of the 'midifilelib' public domain MIDI file utilities,
which was originally available from

http://www.harmony-central.com/MIDI/midifilelib.tar.gz

If you have the source distribution and intend to re-compile the code,
read the file coding.txt.
---------------------------------------------------------------------
midi2abc - program to convert MIDI format files to abc notation. 

This program takes a MIDI format file and converts it to something as close
as possible to abc text format. The user then has to add text fields not
present in the MIDI header and possibly tidy up the abc note output.

Features :

* The key is chosen so as to minimize the number of accidentals. 
Alternatively, the user can specify the key numerically (a positive number
is the number of sharps, a negative number is minus the number of flats).
* Note length can be set by specifying the total number of bars or the 
tempo of the piece. Alternatively the note length can be read from the file.
However, by default it is deduced in a heuristic manner from the inter-note 
distances.  This means that you do not have to use the MIDI clock as a 
metronome when playing in a tune from a keyboard. 
* Barlines are automatically inserted. The user specifies the number of
measures in the anacrusis before the first barline and the time signature.
* The program can guess how many beats there should be in the anacrusis,
either by looking for the first strong note or minimizing the number of
notes split by a tie across a barline.
* Where a note extends beyond a bar break, it is split into two tied notes.
* The output has 4 bars per line.
* Enough accidental signs are put in the music to ensure that no pitch
errors occur if a barline is added or deleted.
* The program attempts to group notes sensibly in each bar.
* Triplets and broken rhythm (a>b) are supported.
* Chords are identified.
* Text information from the original MIDI file is included as comments.
* The -c option can be used to select only 1 MIDI channel. Events on 
other channels are ignored.

What midi2abc does not do :

* Supply tune title, composer or any other field apart from X: , K:, Q:, M:
and L: - these must be added by hand afterwards, though they may have been
included in the text of the MIDI file.
* Support duplets, quadruplets, other esoteric features.
* Support mid-tune key or meter changes.
* Deduce repeats. The output is just the notes in the input file.
* Recover an abc tune as supplied to abc2midi. However, if you want to
do this, "midi2abc -xa -f file.mid" comes close.

midi2abc 
  usage :
midi2abc <options>
         -a <half L: units>
         -xa  extract anacrusis from file (find first strong note)
         -ga  guess anacrusis (minimize ties across bars)
         -gk  guess key signature by minimizing accidentals
         -gu  guess the number of midi pulses per note from note
              duration statistics in the MIDI file 
         -m <time signature>
         -b <bars wanted in output>
         -Q <tempo in quarter-notes per minute>
         -k <key signature> -6 to 6 sharps
         -c <channel>
         [-f] <input file>
         -o <output file>
         -s do not discard very short notes
         -sr do not notate a short rest after a note
         -sum summary
         -nt do not look for triplets or broken rhythm
         -u number of midi pulses per abc time unit
         -ppu parts per abc unit length (power of 2 only)
         -aul denominator of abc unit length (power of 2 only)
         -obpl one bar per line (deprecated)
         -bpl <number> of bars per printed line
         -bps <number> of bars per staff line
         -nogr No note grouping. Space put between every note.
         -splitbars splits bars to avoid nonhomophonic chords. 
         -splitvoices splits voices to avoid nonhomophonic chords. 
         -midigram   No abc file is created, but a list
		of all notes is produced. Other parameters
                are ignored.
         -mftext  No abc file is created, but a list of all
                the midi commands (mftext like output) is
                produced. The output is best viewed with
                runabc.tcl
         -stats prints summary and statistics of the midi file
                which would be best viewed using an new application
                midiexplorer.tcl.
         -ver   Prints version number and exits

Use only one of -u -gu -b and -Q or better none.
If none of these is present, the program uses the PPQN header
information in the MIDI file to determine the note length in
MIDI pulses. This usually results in the best output. 

The output of midi2abc is printed to the screen. To save it to a file, use
the redirection operator or the -o option.

e.g.

midi2abc -f file.mid > file.abc
or
midi2abc -f file.mid -o file.abc

By default the program uses the key signature and time signature
specified in the MIDI file. If the key signature is not specified,
the program will automatically determine the key signature by
minimizing the number of accidentals. If there is no time signature
found, then 4/4 is assumed. You may also specify a time signature
using the -m option.  Allowable time signatures are C,
4/4, 3/8, 2/4 and so on.

If the tune has an anacrusis, you should specify the number in quantum
units lengths. By default a quantum unit length is one half of
the unit length note specified by the L: command. (However, this
can be changed using the -ppu runtime parameter.)  For example,
if the meter is 6/8 and L: is set to 1/8, a half unit length is
1/16 note. An anacrusis of a quarter note would be specified as 4.

The -bpl and -bps control the formatting of the output. -bpl controls
the number of bars put in every line. The lines are joined together
with a backslash (\) until the desired number of bars per staff are
printed. Since -bpl 1 is equivalent to -obpl, the latter parameter
is being deprecated.

Midi2abc has evolved quite a lot since James Allwright has
transferred support and development to me [SS]. Most of the
MIDI files posted on the web site appear to have been generated
from a music notation program and therefore convert cleanly
to abc files using all the information built into the file.
Therefore midi2abc no longer automatically estimates the number 
of MIDI pulses per note from the file statistics, but uses
the indications embedded in the file. However, the user still
has the option of calling these functions or changing this
parameter by using the run time parameters Use only one of -u -gu 
-b and -Q or better none. The -u parameter, allows you
to specify the number of MIDI pulses per unit length, assuming
you know what it should be. To find out the value that
midi2abc is using, run the program with the -sum parameter.

Midi2abc quantizes the note durations to a length of half
the L: value. Thus if L: was set to 1/8, then the smallest
note that midi2abc would extract would be a 1/16 th note.
This is the basic quantum unit.  The L: note length is set
by midi2abc on the basis of the time signature. For time 
signatures less than 3/4 it uses an L: 1/16 and for others 
it uses a length of 1/8. This convention was probably chosen 
so that midi2abc does not quantize the notes to a too fine 
level producing outputs like  C2-C/4 D2-D/8 etc which would 
be difficult to read.  For some music, this may be too coarse 
and it may be preferable to allow the user take control of 
the L: value or allow the splitting of the L: value into 
smaller parts.  Two new run time parameters were introduced:
-ppu specifies the number of parts to split the L: note. 
It should be a power of 2 (eg. 1,2,4,8,16 ...) and by default 
it remains as 2 as before. The -aul specifies the L: value to use,
for example, to get L: 1/32 you would put -aul 32.


Keyboard and guitar music has a lot of chords which frequently
poses a problem to midi2abc. If the notes in the chord do
not share a common onset or end time, midi2abc uses ties to join
the longer notes to other chords. This produces a mess looking
somewhat like
[AG-G-G-D-G,,,-][B/2-B/2-B/2-G/2G/2-G/2-D/2-G,,,/2-]...
which does not convert into something easy to read when
display in common music notation. Abcm2ps and abc2midi allow
a bar to be split into separate voices using the & sign.
Separating the notes which do not overlap exactly into
individual voices provides some improvement. If you encountering
this problem with your MIDI file, you may wish to try rerunning
the file with either the -splitbars or -splitvoices parameter.
This is a feature introduced in June 2005 (version 2.83).
The algorithm which separates the notes in a voice into
distinct voices (tracks) (label_split in midi2abc.c) is rather
crude and needs improvement. I welcome any suggestions
or improved code.


The -midigram option runs midi2abc in a completely different
mode. It does not produce any abc file but outputs a list
of all the notes found in the MIDI file. Each line of output
represents one note. For each line, the on and off time
in MIDI time units, the track number, the channel number, 
midi pitch, and midi velocity are listed. The last line
contains a single value equal to the duration of the file
in MIDI pulses.  The output is designed to eventually go into a
graphical user interface (runabc) which will display these events 
in piano roll format.

Note: midi2abc does not work correctly if the lyrics are embedded
in the same track as the notes. If you intend to run midi2abc on
the MIDI file produced by abc2midi, you need to use the -STFW
option if the tune contains lyrics. 

eg. abc2midi lyric_tune.abc -STFW

Since February 01 2010, abc2midi by default places the lyrics in
the same track as the notes. See doc/CHANGES.


-------------------------------------------------------------------------

abc2midi  - converts abc file to MIDI file(s).
Usage : abc2midi <abc file> [reference number] [-c] [-v] [-o filename]
        [-t] [-n <value>] [-ver] [-NFNP] [-NFER] [-NGRA] [-STFW] [-NCOM] [-OCC]
        [reference number] selects a tune
        -c  selects checking only
        -v <level> selects verbose option
        -o <filename>  selects output filename
        -t selects filenames derived from tune titles
        -n <limit> set limit for length of filename stem
        -CS use 2:1 instead of 3:1 for broken rhythms
        -quiet suppress some common warnings
        -silent suppresses other messages
        -Q <tempo> in quarter notes/minute
        -NFNP ignore all dynamic indications (!f! !ff! !p! etc.)
        -NFER ignore fermata markings
        -NGRA ignore grace notes
        -STFW separate tracks for words (lyrics)
        -NCOM suppress comments
        -ver prints version number and exits
        -BF Barfly mode: invokes a stress model if possible
        -OCC old chord convention (eg. +CE+)
        -TT tune to A = <frequency>
        -CSM <filename> load custom stress models from file

 The default action is to write a MIDI file for each abc tune
 with the filename <stem>N.mid, where <stem> is the filestem
 of the abc file and N is the tune reference number. If the -o
 option is used, only one file is written. This is the tune
 specified by the reference number or, if no reference number
 is given, the first tune in the file. The -Q parameter sets
 the default tempo in event the Q: command is not given in the
 abc header. The program accepts both the deprecated (eg.
 !trill!) and standard (+trill+) notation for decorations.
 Older versions of this program handled the defunct convention
 for chords (i.e +G2B2D2+ instead of [GBD]2). If you need to
 handle the older notation, include the -OCC flag; however the
 program will not accept the standard notation for decorations.
 Broken rhythms indicated by > or < (eg. A > B) assume a
 the hornpipe ratio of 3:1.  To change it to the Celtic ratio
 3:1 include the -CS flag.  


Features :

* Broken rythms (>, <), chords, n-tuples, slurring, ties, staccatto notes,
repeats, in-tune tempo/length/meter changes are all supported.

* R:hornpipe or r:hornpipe is recognized and note timings are adjusted to
give a broken rhythm (ab is converted to a>b).

* Most errors in the abc input will generate a suitable error message in
the output and the converter keeps going.

* Comments and text fields in the abc source are converted to text events
in the MIDI output

* If guitar chords are present, they are used to generate an accompaniment
in the MIDI output.

* If there are mis-matched repeat signs in the abc, the program attempts to
fix them. However, it will not attempt this if a multi-part tune 
description has been used or if multiple voices are in use.

* Karaoke MIDI files can be generated by using the w: field to include 
lyrics.

* Nonnumeric voice ids, eg V: soprano, as described as proposed
for the new abc standard is handled.

* Invisible rests specified as x, are treated as normal rests (z).

* There are some extensions to the abc syntax of the form

%%MIDI channel n

These control channel and program selection, transposing and various
other features of abc2midi. See the file abcguide.txt for more
details.

Bugs and Limitations :

* No field is inherited from above the X: field of the tune.

* Where an accidental is applied to a tied note that extends across
  a barline, abc2midi requires that the note beyond the barline must 
  be explicitly given an accidental e.g.

  ^F- | F     - will be reported as an error.
  ^F- | ^F    - will produce a tied ^F note.

  It is common to see no accidental shown when this occurs in published 
  printed music.

-------------------------------------------------------------------------
abc2abc

Usage: abc2abc <filename> [-s] [-n X] [-b] [-r] [-e] [-t X]
       [-u] [-d] [-v] [-V X] [-ver] [-X n]
  -s for new spacing
  -n X to re-format the abc with a new linebreak every X bars
  -b to remove bar checking
  -r to remove repeat checking
  -e to remove all error reports
  -t X to transpose X semitones
  -nda No double accidentals in guitar chords
  -nokeys No key signature (i.e. K: none). Use sharps.
  -nokeyf No key signature (i.e. K: none). Use flats.
  -u to update notation ([] for chords and () for slurs)
  -usekey n Use key signature sf (sharps/flats)
  -useclef (treble or bass)
  -d to notate with doubled note lengths
  -v to notate with halved note lengths
  -V X[,Y...] to output only voice X,Y...
  -P X[,Y...] restricts action to voice X,Y... leaving other voices intact
  -ver prints version number and exits
  -X n renumber the all X: fields as n, n+1, ..
  -xref n output only the tune with X reference number n.
  -usekey sf Use key signature sf (flats/sharps)
  -OCC old chord convention (eg. +CE+)

A simple abc checker/re-formatter/transposer. If the -n option is selected, 
error checking is turned off. 

If a voice is assigned to channel 10 (drum channel) using a
%%MIDI channel 10
command, then this voice is never transposed.

The -nokeys or -nokeyf option will set "K: none" and place accidentals on
all notes that should have accidentals for the expected key signature. 
The first option will use only sharps; the second option will use
only flats.

The -usekey option will force the key signature to be key[sf] where
sf is a number between -5 and +5 inclusive.
sf  -5  -4  -3  -2  -1  0  1  2  3  4  5
key Db  Ab  Eb  Bb  F   C  G  D  A  E  B
Accidentals will be added to preserve the correct notes. This
is useful for some music with many accidentals which does
not fit in any specific key signature. If sf = 0, abc2abc
use K:none.

If you want to check an abc tune, it is recommended that you use abc2midi 
with the -c option as this performs extra checks that abc2abc does not do.

When using the -P X option, it may be necessary to insert some
field commands such as K: or L: following the voice X declaration,
so that they will be converted and appear in the output.

The output of abc2abc is printed to the screen. To save it to a file, use
the redirection operator.

e.g.

abc2abc file.abc -t 2 > newfile.abc

Known problems:
* When using the -n option on a program with lyrics, a barline in a w:
  field may be carried forward to the next w: field.
-------------------------------------------------------------------------

mftext - MIDI file to text

This gives a verbose description of what is in a MIDI file. You may wish
to use it to check the output from abc2midi. It is part of the original
midifilelib distribution.

-------------------------------------------------------------------------
YAPS
----
YAPS is an abc to PostScript converter which can be used as an alternative
to abc2ps. See the file yaps.txt for a more detailed description of this
program.

-------------------------------------------------------------------------
midicopy is a stand alone application which copies a midi file or part
of a midi file to a new midi file. If you run it with no parameters, a
short description shown below will appear.

usage:
midicopy <options> input.mid output.mid
midicopy copies selected tracks, channels, time interval of the input midi file.options:
-ver  version information
-trks n1,n2,..(starting from 1)
-xtrks n1,n2,... (tracks to exclude)
-xchns n1,n2,... (channels to exclude)
-chns n1,n2,..(starting from 1)
-from n (in midi ticks)
-to n   (in midi ticks)
-fromsec %f (in seconds)
-tosec %f (in seconds)
-frombeat %f
-tobeat %f
-replace trk,loc,val
-tempo n (in quarter notes/min)
-speed %f (between 0.1 and 10.0)
-drumfocus n (35 - 81) m (0 - 127)
-mutenodrum [level] 
-setdrumloudness n (35-81) m (0 -127)
-focusontrack n1,n2,... 
-focusonchannel n1,n2,...
-attenuation n
-nobends
-indrums n1,n2,... (drums to include)
-xdrums n1,n2,... (drums to exclude)
-onlydrums (only channel 10)
-nodrums (exlcude channel 10)
-zerochannels  set all channel numbers to zero


midicopy.exe -ver

will print out something like 
1.29 December 06 2017


midicopy.exe input.mid output.mid
does nothing interesting except copy the contents of input.mid to a
new file output.mid.



If you  include the -from parameter followed by a midi pulse number,
then the program will select the appropriate data starting after the
given midi pulse location so that will you play midi file it will start
from that midi pulse number. In order to ensure that the right tempo,
channel assignments are used, all of these commands prior to that
pulse number are also copied.

If you include the -to command followed by a midi  pulse number, the
midi file is truncated beyond that point, so that when you play the file
it will stop at this point. You can also specify the window using beats
with the options -frombeats and -tobeats. The program will print out
the estimated duration of the output midi file.

All the tracks will be copied unless you specify them in the list following
keyword -trks. You start counting tracks from 1.

Similarly, all channels will be copied unless you specify them following
keyword -chns. You start counting channels from 1.

The -xchns and -xtrk options will exclude the indicated channels or
tracks from the output midi file. The -xchns does not work together
with -chns and neither does -xtrks work with -trks. (Use one or the
other.)

The option -replace allows you to overwrite a specific byte given its
track number and offset loc, by the byte whose value is val. This is
used for changing the program number associated with a channel. The byte
is replaced in the output file. If you use the -replace option, all
other options like -from, -to, -chns etc. are ignored.

The embedded tempo commands can be replaced using
-tempo n
in units beats/minute.

-speed f
where f is a decimal number between 0.1 and 10.0 will multiply
the current tempo by this factor.

-drumfocus will accentuate the specified drum (35 to 81) to level
m (0 to 127).

-focusontracks will attenuate all tracks except those specified.

-focusonchannels will attenuate all channels except those specified.

-attenuation specifies the amount to reduce the note velocities
for the focus options.

-------------------------------------------------------------------------
abcmatch.exe  - see abcmatch.txt



-------------------------------------------------------------------------
A Short Explanation of MIDI
---------------------------
MIDI stands for "Musical Instrument Digital Interface". MIDI was originally
designed to connect a controller, such as a piano-style keyboard, to a
synthesizer. A MIDI cable is similar to a serial RS232 cable but uses
different voltage levels and an unusual baud rate (31250 baud). The MIDI
standard also defines the meaning of the digital signals sent down the 
cable; pressing and releasing a key produces 2 of these signals, a 
"note on" followed by a "note off". 

There is an additional standard for MIDI files, which describes how to 
record these signals together with the time when each signal was produced. 
This allows a complete performance to be recorded in a compact digital 
form. It is also possible for a computer to write a MIDI file which can 
be played back in exactly the same way as a MIDI file of a recorded 
performance. This is what abc2midi does.

-------------------------------------------------------------------------
Note: DPMI server for DOS executables

If you have downloaded the executables compiled using DJGPP, you may get
an error message saying that a DPMI (Dos Protected Mode Interface) server
is needed. If you can run the programs from a DOS window within Windows,
this may solve the problem. Alternatively, download the DPMI server
recommended for DJGPP, called CWSDPMI.EXE. This needs to be on your path
for the executables to run.
-------------------------------------------------------------------------
Bug reports

Please report any bugs you find in abc2midi, midi2abc, midicopy, or 
abcmatch, abc2abc to fy733@ncf.ca (preferably with 
an example so that I can replicate the problem). Better still, send 
me the repaired source files which fix the problem! If you add your
own features to the code that other people might want to use then let 
me know.  I may or may not want to add them to the official version.
So far I have been maintaining the code, but I don't guarantee anything.

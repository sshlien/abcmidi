# DJGPP (DOS port of gcc) Makefile for abcMIDI package 
# 
#
# compilation #ifdefs - you may need to change these defined to get
#                       the code to compile with a different C compiler.
#
# NOFTELL in midifile.c and genmidi.c selects a version of the file-writing
#         code which doesn't use file seeking.
#
# PCCFIX in mftext.c midifile.c midi2abc.c
#        comments out various things that aren't available in PCC
#
# USE_INDEX causes index() to be used instead of strchr(). This is needed
#           by some pre-ANSI C compilers.
#
# ASCTIME causes asctime() to be used instead of strftime() in pslib.c.
#         If ANSILIBS is not set, neither routine is used.
#
# ANSILIBS causes code to include some ANSI standard headers
#
# KANDR selects functions prototypes without argument prototypes.
#
# NO_SNPRINTF causes code to use printf instead of snprintf which
# is less secure.
CC=gcc
CFLAGS=-c -ansi -DANSILIBS -DNO_SNPRINTF -Wformat -Wtraditional
# -ansi forces ANSI compliance
LNK=gcc

all : abc2midi.exe midi2abc.exe abc2abc.exe mftext.exe yaps.exe\
     midicopy.exe abcmatch.exe

abc2midi.exe : parseabc.o store.o genmidi.o queues.o midifile.o parser2.o
	$(LNK) -o abc2midi.exe parseabc.o genmidi.o store.o \
	queues.o midifile.o parser2.o stresspat.o -lm

abc2abc.exe : parseabc.o toabc.o
	$(LNK) -o abc2abc.exe parseabc.o toabc.o

midi2abc.exe : midifile.o midi2abc.o 
	$(LNK) midifile.o midi2abc.o -o midi2abc.exe -lm

mftext.exe : midifile.o mftext.o crack.o
	$(LNK) midifile.o mftext.o crack.o -o mftext.exe

midicopy.exe : midicopy.o
	$(LNK) midicopy.o -o midicopy.exe

abcmatch.exe : abcmatch.o matchsup.o parseabc.o
	$(LNK) abcmatch.o matchsup.o parseabc.o -o abcmatch.exe



yaps.exe : parseabc.o yapstree.o drawtune.o debug.o pslib.o position.o parser2.o
	$(LNK) -o yaps.exe parseabc.o yapstree.o drawtune.o debug.o \
	position.o pslib.o parser2.o

# common parser object code
#
parseabc.o : parseabc.c abc.h parseabc.h
	$(CC) $(CFLAGS) parseabc.c 

parser2.o : parser2.c parseabc.h parser2.h
	$(CC) $(CFLAGS) parser2.c

# objects needed by abc2abc
#
toabc.o : toabc.c abc.h parseabc.h
	$(CC) $(CFLAGS) toabc.c 

# objects needed by abc2midi
#
store.o : store.c abc.h parseabc.h parser2.h genmidi.h 
	$(CC) $(CFLAGS) store.c 

genmidi.o : genmidi.c abc.h midifile.h genmidi.h
	$(CC) $(CFLAGS) genmidi.c 

stresspat.o: stresspat.c
	$(CC) $(CFLAGS) stresspat.c

# could use -DNOFTELL here
tomidi.o : tomidi.c abc.h midifile.h
	$(CC) $(CFLAGS) tomidi.c

queues.o: queues.c genmidi.h
	$(CC) $(CFLAGS) queues.c

midicopy.o: midicopy.c midicopy.h
	$(CC) $(CFLAGS) midicopy.c

abcmatch.o: abcmatch.c abc.h
	$(CC) $(CFLAGS) abcmatch.c



# common midifile library
#
# could use -DNOFTELL here
midifile.o : midifile.c midifile.h
	$(CC) $(CFLAGS) midifile.c

# objects needed by yaps
#
yapstree.o: yapstree.c abc.h parseabc.h structs.h drawtune.h parser2.h
	$(CC) $(CFLAGS) yapstree.c

drawtune.o: drawtune.c structs.h sizes.h abc.h drawtune.h
	$(CC) $(CFLAGS) drawtune.c

pslib.o: pslib.c drawtune.h
	$(CC) $(CFLAGS) pslib.c

position.o: position.c abc.h structs.h sizes.h
	$(CC) $(CFLAGS) position.c

debug.o: debug.c structs.h abc.h
	$(CC) $(CFLAGS) debug.c

# objects needed by midi2abc
#
midi2abc.o : midi2abc.c midifile.h
	$(CC) $(CFLAGS) midi2abc.c

# objects for mftext
#
crack.o : crack.c
	$(CC) $(CFLAGS) crack.c 

mftext.o : mftext.c midifile.h
	$(CC) $(CFLAGS) mftext.c

# objects for abcmatch
#
matchsup.o : matchsup.c abc.h parseabc.h parser2.h
	$(CC) $(CFLAGS) matchsup.c



clean:
	del *.o
	del *.exe

zipfile: midi2abc.exe abc2midi.exe mftext.exe yaps.exe\
         abc2abc.exe midicopy.exe
	zip pcexe2.zip *.exe readme.txt abcguide.txt demo.abc yaps.txt

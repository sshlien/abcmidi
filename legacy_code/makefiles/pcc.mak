# PCC Makefile for abcMIDI package
# 
#
# compilation #ifdefs - you need to define some of these to get
#                       the code to compile with PCC.
#
# NOFTELL in midifile.c and genmidi.c selects a version of the file-writing
#         code which doesn't use file seeking.
#
# PCCFIX in mftext.c midifile.c midi2abc.c
#        comments out various things that aren't available in PCC
#        and applies a fix needed for file writing
#
# ANSILIBS causes appropriate ANSI .h files to be #included.
#
# KANDR selects function prototypes with argument prototypes.
#
# USE_INDEX replaces calls to strchr() with calls to index().
#

CC=pcc
CFLAGS=-nPCCFIX -nNOFTELL -nUSE_INDEX -nKANDR
LNK=pccl

all : abc2midi.exe midi2abc.exe abc2abc.exe mftext.exe yaps.exe midicopy.exe abcmatch.exe

abc2midi.exe : parseabc.o store.o genmidi.o queues.o midifile.o parser2.o stresspat.o -lm
	$(LNK) -Lc:\bin\pcc\ -Oabc2midi parseabc.o store.o genmidi.o queues.o midifile.o parser2.o stresspat.o

abc2abc.exe : parseabc.o toabc.o
	$(LNK) -Lc:\bin\pcc\ -Oabc2abc parseabc.o toabc.o

midi2abc.exe : midifile.o midi2abc.o 
	$(LNK) -Lc:\bin\pcc\ midifile.o midi2abc.o -Omidi2abc

mftext.exe : midifile.o mftext.o crack.o
	$(LNK) -Lc:\bin\pcc\ midifile.o mftext.o crack.o -Omftext

midicopy.exe: midicopy.o
	$(LNK) -Lc:\bin\pcc\ midicopy.o -Omidicopy $(CFLAGS)

parseabc.o : parseabc.c abc.h parseabc.h
	$(CC) parseabc.c  $(CFLAGS)

parser2.o : parser2.c abc.h parseabc.h parser2.h
	$(CC) parser2.c $(CFLAGS)

toabc.o : toabc.c abc.h parseabc.h
	$(CC) toabc.c  $(CFLAGS)

genmidi.o : genmidi.c abc.h midifile.h parseabc.h genmidi.h
	$(CC) genmidi.c $(CFLAGS)

stresspat.o : stresspat.c
	$(CC) stresspat.c $(CFLAGS)

store.o : store.c abc.h midifile.h parseabc.h
	$(CC) store.c $(CFLAGS)

queues.o : queues.c genmidi.h
	$(CC) queues.c $(CFLAGS)

midifile.o : midifile.c midifile.h
	$(CC) midifile.c $(CFLAGS)

midi2abc.o : midi2abc.c midifile.h
	$(CC) midi2abc.c $(CFLAGS)

crack.o : crack.c
	$(CC) crack.c  $(CFLAGS)

mftext.o : mftext.c midifile.h
	$(CC) mftext.c $(CFLAGS)

midicopy.o : midicopy.c
	$(CC) midicopy.c $(CFLAGS)

abcmatch.o : abcmatch.c
	$(CC) abcmatch.c $(CFLAGS)

matchsup.o : matchsup.c
	$(CC) matchsup.c $(CFLAGS)

clean:
	del *.exe
	del *.o

zipfile: abc2midi.exe midi2abc.exe abc2abc.exe mftext.exe midicopy.exe abcmatch.exe
	zip pcexe.zip *.exe readme.txt abcguide.txt demo.abc

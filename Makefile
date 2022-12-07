# Generic unix/gcc Makefile for abcMIDI package 
# 
#
# compilation #ifdefs - you need to compile with these defined to get
#                       the code to compile with PCC.
#
# NOFTELL in midifile.c and genmidi.c selects a version of the file-writing
#         code which doesn't use file seeking.
#
# PCCFIX in mftext.c midifile.c midi2abc.c
#        comments out various things that aren't available in PCC
#
# ANSILIBS includes some ANSI header files (which gcc can live without,
#          but other compilers may want).
#
# USE_INDEX causes index() to be used instead of strchr(). This is needed
#           by some pre-ANSI C compilers.
#
# ASCTIME causes asctime() to be used instead of strftime() in pslib.c.
#         If ANSILIBS is not set, neither routine is used.
#
# KANDR selects functions prototypes without argument prototypes.
#       currently yaps will only compile in ANSI mode.
#
#
# On running make, you may get the mysterious message :
#
# ', needed by `parseabc.o'. Stop `abc.h
#
# This means you are using GNU make and this file is in DOS text format. To
# cure the problem, change this file from using PC-style end-of-line (carriage 
# return and line feed) to unix style end-of-line (line feed).

VERSION = @VERSION@

CC = gcc
INSTALL = /usr/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_PROGRAM = ${INSTALL}

CFLAGS = -DANSILIBS  -O2  
CPPFLAGS = -DHAVE_CONFIG_H  -I. 
LDFLAGS =  -lm

prefix = /usr/local
exec_prefix = ${prefix}

srcdir = .

bindir = ${exec_prefix}/bin
libdir = ${exec_prefix}/lib
datadir = ${prefix}/share
docdir = ${prefix}/share/doc/abcmidi
mandir = ${prefix}/share/man/man1

binaries=abc2midi midi2abc abc2abc mftext yaps midicopy abcmatch midistats

all : abc2midi midi2abc abc2abc mftext yaps midicopy abcmatch midistats

OBJECTS_ABC2MIDI=parseabc.o store.o genmidi.o midifile.o queues.o parser2.o stresspat.o music_utils.o
abc2midi : $(OBJECTS_ABC2MIDI)
	$(CC) $(CFLAGS) -o abc2midi $(OBJECTS_ABC2MIDI) $(LDFLAGS) 
$(OBJECTS_ABC2MIDI): abc.h parseabc.h config.h Makefile

OBJECTS_ABC2ABC=parseabc.o toabc.o music_utils.o
abc2abc : $(OBJECTS_ABC2ABC)
	$(CC) $(CFLAGS) -o abc2abc $(OBJECTS_ABC2ABC) $(LDFLAGS)
$(OBJECTS_ABC2ABC): abc.h parseabc.h config.h Makefile

OBJECTS_MIDI2ABC=midifile.o midi2abc.o 
midi2abc : $(OBJECTS_MIDI2ABC)
	$(CC) $(CFLAGS) -o midi2abc $(OBJECTS_MIDI2ABC) $(LDFLAGS)
$(OBJECTS_MIDI2ABC): abc.h midifile.h config.h Makefile

OBJECTS_MIDISTATS=midifile.o midistats.o
midistats : $(OBJECTS_MIDISTATS)
	$(CC) $(CFLAGS) -o midistats $(OBJECTS_MIDISTATS) $(LDFLAGS)
$(OBJECTS_MIDISTATS): abc.h midifile.h config.h Makefile

OBJECTS_MFTEXT=midifile.o mftext.o crack.o
mftext : $(OBJECTS_MFTEXT)
	$(CC) $(CFLAGS) -o mftext $(OBJECTS_MFTEXT) $(LDFLAGS)
$(OBJECTS_MFTEXT): abc.h midifile.h config.h Makefile

OBJECTS_YAPS=parseabc.o yapstree.o drawtune.o debug.o pslib.o position.o parser2.o music_utils.o
yaps : $(OBJECTS_YAPS)
	$(CC) $(CFLAGS) -o yaps $(OBJECTS_YAPS) $(LDFLAGS)
$(OBJECTS_YAPS): abc.h midifile.h config.h Makefile

OBJECTS_MIDICOPY=midicopy.o
midicopy : $(OBJECTS_MIDICOPY)
	$(CC) $(CFLAGS) -o midicopy $(OBJECTS_MIDICOPY) $(LDFLAGS)
$(OBJECTS_MIDICOPY): abc.h midifile.h midicopy.h config.h Makefile

OBJECTS_ABCMATCH=abcmatch.o matchsup.o parseabc.o music_utils.o
abcmatch : $(OBJECTS_ABCMATCH)
	$(CC) $(CFLAGS) -o abcmatch $(OBJECTS_ABCMATCH) $(LDFLAGS)
$(OBJECTS_ABCMATCH): abc.h midifile.h config.h Makefile

music_utils.o : music_utils.c music_utils.h

parseabc.o : parseabc.c abc.h parseabc.h

parser2.o : parser2.c abc.h parseabc.h parser2.h

toabc.o : toabc.c abc.h parseabc.h

# could use -DNOFTELL here
genmidi.o : genmidi.c abc.h midifile.h genmidi.h

stresspat.o :	stresspat.c

store.o : store.c abc.h parseabc.h midifile.h genmidi.h

queues.o : queues.c genmidi.h

# could use -DNOFTELL here
midifile.o : midifile.c midifile.h

midi2abc.o : midi2abc.c midifile.h

midistats.o : midistats.c midifile.h

midicopy.o : midicopy.c midicopy.h

abcmatch.o: abcmatch.c abc.h

crack.o : crack.c

mftext.o : mftext.c midifile.h

# objects needed by yaps
#
yapstree.o: yapstree.c abc.h parseabc.h structs.h drawtune.h

drawtune.o: drawtune.c structs.h sizes.h abc.h drawtune.h

pslib.o: pslib.c drawtune.h

position.o: position.c abc.h structs.h sizes.h

debug.o: debug.c structs.h abc.h

#objects for abcmatch
#
matchsup.o : matchsup.c abc.h parseabc.h parser2.h

clean :
	rm *.o ${binaries}

install: abc2midi midi2abc abc2abc mftext midicopy yaps abcmatch midistats
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 ${binaries} $(DESTDIR)$(bindir)

	# install documentation
	$(INSTALL) -d $(DESTDIR)${docdir}
	$(INSTALL)  -m 644 doc/*.txt $(DESTDIR)$(docdir)
	$(INSTALL)  -m 644 doc/AUTHORS $(DESTDIR)$(docdir)
	$(INSTALL)  -m 644 doc/CHANGES $(DESTDIR)$(docdir)
	$(INSTALL)  -m 644 VERSION $(DESTDIR)$(docdir)

	# install manpages
	$(INSTALL)  -d $(DESTDIR)${mandir}
	$(INSTALL)  -m 644 doc/*.1 $(DESTDIR)$(mandir)


uninstall:
	echo "uninstalling...";
	#rm -f $(DESTDIR)$(bindir)/$(binaries)
	rm -f $(DESTDIR)$(bindir)/abc2midi
	rm -f $(DESTDIR)$(bindir)/abc2abc
	rm -f $(DESTDIR)$(bindir)/yaps
	rm -f $(DESTDIR)$(bindir)/midi2abc
	rm -f $(DESTDIR)$(bindir)/midistats
	rm -f $(DESTDIR)$(bindir)/mftext
	rm -f $(DESTDIR)$(bindir)/abcmatch
	rm -f $(DESTDIR)$(bindir)/midicopy
	rm -f $(DESTDIR)$(docdir)/*.txt
	rm -f $(DESTDIR)$(docdir)/AUTHORS
	rm -f $(DESTDIR)$(docdir)/CHANGES
	rm -f $(DESTDIR)$(docdir)/VERSION
	rm -f $(DESTDIR)$(mandir)/*.1
	rmdir $(DESTDIR)$(docdir)


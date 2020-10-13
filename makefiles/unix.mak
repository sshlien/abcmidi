# file: Makefile
# vim:fileencoding=utf-8:fdm=marker:ft=make
# Makefile for abcMIDI package
# Works with both GNU make and BSD make, provided that CC is defined.
# If not, use e.g. “make CC=gcc”.
#
# Written by R.F. Smith <rsmith@xs4all.nl>
# Created: 2020-07-27T14:00:05+0200
# Last modified: 2020-07-27T14:45:45+0200

INSTALL = install
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_PROGRAM = ${INSTALL} -s -m 555

CFLAGS = -O2 -DANSILIBS

PREFIX = /usr/local
BINDIR = ${PREFIX}/bin
LIBDIR = ${PREFIX}/lib
DATADIR = ${PREFIX}/share
DOCDIR = ${PREFIX}/share/doc/abcmidi
MANDIR = ${PREFIX}/man/man1

BIN= abc2midi midi2abc abc2abc mftext yaps midicopy abcmatch
MAN= abc2midi.1.gz midi2abc.1.gz abc2abc.1.gz mftext.1.gz yaps.1.gz midicopy.1.gz abcmatch.1.gz

all : ${BIN} ${MAN}

OBJ_ABC2MIDI= parseabc.o store.o genmidi.o midifile.o queues.o parser2.o stresspat.o
abc2midi : ${OBJ_ABC2MIDI}
	${CC} -o abc2midi ${OBJ_ABC2MIDI} -lm
${OBJ_ABC2MIDI}: abc.h parseabc.h Makefile

OBJ_ABC2ABC= parseabc.o toabc.o
abc2abc : ${OBJ_ABC2ABC}
	${CC} -o abc2abc ${OBJ_ABC2ABC}
${OBJ_ABC2ABC}: abc.h parseabc.h Makefile

OBJ_MIDI2ABC= midifile.o midi2abc.o
midi2abc : ${OBJ_MIDI2ABC}
	${CC} -o midi2abc ${OBJ_MIDI2ABC} -lm
${OBJ_MIDI2ABC}: abc.h midifile.h Makefile

OBJ_MFTEXT= midifile.o mftext.o crack.o
mftext : ${OBJ_MFTEXT}
	${CC} -o mftext ${OBJ_MFTEXT}
${OBJ_MFTEXT}: abc.h midifile.h Makefile

OBJ_YAPS= parseabc.o yapstree.o drawtune.o debug.o pslib.o position.o parser2.o
yaps : ${OBJ_YAPS}
	${CC} -o yaps ${OBJ_YAPS}
${OBJ_YAPS}: abc.h midifile.h Makefile

OBJ_MIDICOPY= midicopy.o
midicopy : ${OBJ_MIDICOPY}
	${CC} -o midicopy ${OBJ_MIDICOPY}
${OBJ_MIDICOPY}: abc.h midifile.h midicopy.h Makefile

OBJ_ABCMATCH= abcmatch.o matchsup.o parseabc.o
abcmatch : ${OBJ_ABCMATCH}
	${CC} ${CFLAGS} -o abcmatch ${OBJ_ABCMATCH}
${OBJ_ABCMATCH}: abc.h midifile.h Makefile

parseabc.o: parseabc.c abc.h parseabc.h

parser2.o: parser2.c abc.h parseabc.h parser2.h

toabc.o: toabc.c abc.h parseabc.h

genmidi.o: genmidi.c abc.h midifile.h genmidi.h

stresspat.o: stresspat.c

store.o: store.c abc.h parseabc.h midifile.h genmidi.h

queues.o: queues.c genmidi.h

midifile.o: midifile.c midifile.h

midi2abc.o: midi2abc.c midifile.h

midicopy.o: midicopy.c midicopy.h

abcmatch.o: abcmatch.c abc.h

crack.o: crack.c

mftext.o: mftext.c midifile.h

yapstree.o: yapstree.c abc.h parseabc.h structs.h drawtune.h

drawtune.o: drawtune.c structs.h sizes.h abc.h drawtune.h

pslib.o: pslib.c drawtune.h

position.o: position.c abc.h structs.h sizes.h

debug.o: debug.c structs.h abc.h

matchsup.o: matchsup.c abc.h parseabc.h parser2.h

abc2midi.1.gz: doc/abc2midi.1
	gzip -c doc/abc2midi.1 >abc2midi.1.gz

midi2abc.1.gz: doc/midi2abc.1
	gzip -c doc/midi2abc.1 >midi2abc.1.gz

abc2abc.1.gz: doc/abc2abc.1
	gzip -c doc/abc2abc.1 >abc2abc.1.gz

mftext.1.gz: doc/mftext.1
	gzip -c doc/mftext.1 >mftext.1.gz

yaps.1.gz: doc/yaps.1
	gzip -c doc/yaps.1 >yaps.1.gz

midicopy.1.gz: doc/midicopy.1
	gzip -c doc/midicopy.1 >midicopy.1.gz

abcmatch.1.gz: doc/abcmatch.1
	gzip -c doc/abcmatch.1 >abcmatch.1.gz


clean :
	rm -f *.o ${BIN} ${MAN}

install: ${BIN} ${MAN}
	${INSTALL} -d ${BINDIR}
	${INSTALL_PROGRAM} ${BIN} ${BINDIR}

	# install documentation
	${INSTALL} -d ${DOCDIR}
	${INSTALL_DATA} VERSION doc/CHANGES doc/AUTHORS doc/*.txt ${DOCDIR}

	# install manpages
	${INSTALL} -d ${MANDIR}
	${INSTALL_DATA} *.1.gz ${MANDIR}

uninstall:
	echo "uninstalling...";
	rm -f ${BINDIR}/abc2midi
	rm -f ${BINDIR}/abc2abc
	rm -f ${BINDIR}/yaps
	rm -f ${BINDIR}/midi2abc
	rm -f ${BINDIR}/mftext
	rm -f ${BINDIR}/abcmatch
	rm -f ${BINDIR}/midicopy
	rm -f ${DOCDIR}
	rm -f ${MANDIR}/abc2midi.1*
	rm -f ${MANDIR}/abc2abc.1*
	rm -f ${MANDIR}/yaps.1*
	rm -f ${MANDIR}/midi2abc.1*
	rm -f ${MANDIR}/mftext.1*
	rm -f ${MANDIR}/abcmatch.1*
	rm -f ${MANDIR}/midicopy.1*

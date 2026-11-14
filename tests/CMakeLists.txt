# abcmidi test suite
#
# Three layers of tests:
#
#   1. Smoke tests   - every binary runs with -ver and exits cleanly.
#                      Catches link errors, missing libraries, trivial crashes.
#
#   2. Golden tests  - each program is run on a sample input and its output
#                      is compared with a checked-in reference file.  Binary
#                      MIDI outputs are first piped through mftext to produce
#                      diffable text, and abc2midi -c gives its barloc.txt
#                      file of bar sizes.  Volatile lines (version banners,
#                      dates, tmpdir paths) are stripped before comparison.
#                      abc2midi runs with -Werror and must exit 0, so the
#                      samples stay free of warnings and errors.
#
#   3. Exit-status tests - abc2midi's exit status and messages on specific
#                      inputs and options, and every tune of every sample
#                      rendered with -Werror.  abc2abc, yaps and abcmatch
#                      also run on every sample and must exit 0.
#
# To regenerate golden files after an intentional change:
#
#   ABCMIDI_UPDATE_GOLDEN=1 ctest --preset debug
#
# Then review the diff and commit the updated tests/golden/*.txt files.

set(SAMPLES_DIR "${PROJECT_SOURCE_DIR}/samples")
set(GOLDEN_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/golden")
set(TEST_TMPDIR "${CMAKE_CURRENT_BINARY_DIR}/tmp")

# With ABCMIDI_VALGRIND, every program that a test runs is run under
# valgrind, and any valgrind error makes the program exit with status 99
# instead of its own, which fails the test.  The report is on stderr, which
# the tests print when they fail.  Empty otherwise, so that the programs run
# directly.  The checks are the strictest that memcheck offers, but for
# memory still reachable at exit, which the system takes back and which no
# program frees on all its exit paths:
#   - every leak is an error: definite, indirect and possible, and so is a
#     file left open (--track-fds);
#   - uninitialised values are tracked to their origin and checked
#     precisely, also in partly valid loads;
#   - heap blocks get 128-byte red zones, and freed blocks stay poisoned in
#     a 1 GB queue to catch late uses;
#   - new and freed memory is filled with 0xa5 and 0x5a, so that a read of
#     it shows in the output even where memcheck does not report it.
# --read-var-info=yes, which names the variables in the reports but triples
# the run time, can be added with VALGRIND_OPTS when looking into a failure.
set(ABCMIDI_LAUNCHER)
if(ABCMIDI_VALGRIND)
  find_program(VALGRIND_EXECUTABLE valgrind REQUIRED)
  set(ABCMIDI_LAUNCHER "${VALGRIND_EXECUTABLE}" --quiet --error-exitcode=99
    --error-limit=no --num-callers=50
    --leak-check=full --show-leak-kinds=definite,indirect,possible
    --errors-for-leak-kinds=definite,indirect,possible
    --track-fds=yes
    --track-origins=yes --expensive-definedness-checks=yes
    --partial-loads-ok=no
    --redzone-size=128 --freelist-vol=1000000000 --freelist-big-blocks=0
    --malloc-fill=0xa5 --free-fill=0x5a)
endif()

# --- Smoke tests: -ver on every binary ---

foreach(bin IN LISTS ABCMIDI_BINARIES)
  add_test(NAME "smoke_${bin}_ver"
    COMMAND ${ABCMIDI_LAUNCHER} $<TARGET_FILE:${bin}> -ver)
  set_tests_properties("smoke_${bin}_ver" PROPERTIES LABELS "smoke")
endforeach()

# --- Golden tests ---

# Build a list of "-D<BIN>=$<TARGET_FILE:bin>" args, one per binary, so that
# run_test.cmake receives all binary paths uniformly.
set(BINARY_DEFS)
foreach(bin IN LISTS ABCMIDI_BINARIES)
  string(TOUPPER "${bin}" BIN_UPPER)
  list(APPEND BINARY_DEFS "-D${BIN_UPPER}=$<TARGET_FILE:${bin}>")
endforeach()

# Helper: register a golden test for one program against one sample.
#
# Optional NAME overrides the default test name (${TYPE}_${stem}); use this
# when registering multiple tests against the same TYPE+SAMPLE pair (e.g. a
# plain abc2midi run and a -PMAR run on the same input).
#
# Optional ABC2MIDI_ARGS forwards extra arguments to the abc2midi invocation
# (only meaningful for TYPEs that go ABC -> MIDI -> diff).
#
# Optional TUNE selects a single tune by its X: reference number from a
# multi-tune sample file (abc2midi/mftext/midistats/midi2abc/midicopy paths).
# Combine with NAME to give each tune-level test a distinct name, e.g.
# NAME abc2midi_demo_5 TUNE 5.
#
# Option EXPECT_ERRORS is for a sample on which abc2midi reports errors: it
# must then exit 1 instead of 0 (see ABC2MIDI_ERROR_SAMPLES below).
function(add_golden_test)
  cmake_parse_arguments(T "EXPECT_ERRORS" "TYPE;SAMPLE;NAME;TUNE" "ABC2MIDI_ARGS" ${ARGN})

  if(T_NAME)
    set(test_name "${T_NAME}")
  else()
    get_filename_component(stem "${T_SAMPLE}" NAME_WE)
    set(test_name "${T_TYPE}_${stem}")
  endif()

  add_test(
    NAME "${test_name}"
    COMMAND "${CMAKE_COMMAND}"
      -DTYPE=${T_TYPE}
      -DSAMPLE=${SAMPLES_DIR}/${T_SAMPLE}
      -DGOLDEN=${GOLDEN_DIR}/${test_name}.txt
      -DTMPDIR=${TEST_TMPDIR}
      "-DABC2MIDI_ARGS=${T_ABC2MIDI_ARGS}"
      "-DABC2MIDI_TUNE=${T_TUNE}"
      -DABC2MIDI_EXPECT_ERRORS=${T_EXPECT_ERRORS}
      "-DLAUNCHER=${ABCMIDI_LAUNCHER}"
      ${BINARY_DEFS}
      -P "${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake"
  )
  set_tests_properties("${test_name}" PROPERTIES LABELS "golden")
endfunction()

# Full per-program coverage against coleraine.abc: every binary is exercised
# on this one representative sample.  (TYPE abc2midi and TYPE mftext run the
# identical ABC->MIDI->mftext pipeline and so produce byte-identical goldens;
# both are kept here to guard the dispatch wiring, but per-sample coverage
# below only needs one of them.)
foreach(type IN ITEMS abc2midi abc2abc midi2abc midistats mftext yaps midicopy abcmatch)
  add_golden_test(TYPE ${type} SAMPLE coleraine.abc)
endforeach()

# Samples on which abc2midi reports errors, so that their abc2midi runs must
# exit 1 rather than 0.  Remove a sample from this list once its error is
# fixed (the test then fails, as a reminder).
#   microtone.abc - an invalid K: microtone, on purpose (regression guard)
#   barcheck_long_warning.abc, barfly_barcheck.abc - a bar of the wrong
#                   length, on purpose (regression guards): its warning is an
#                   error with -Werror
#   key_unknown_word.abc - an unknown word in K:, on purpose (regression
#                   guard): its warning is an error with -Werror
#   microtone_accidentals.abc - invalid accidentals with a microtone, on
#                   purpose (regression guard)
#   chordname_too_long.abc - a %%MIDI chordname of 11 notes, on purpose
#                   (regression guard)
#   decoration_line_end.abc - a decoration at the end of a line, on
#                   purpose (regression guard)
set(ABC2MIDI_ERROR_SAMPLES microtone.abc barcheck_long_warning.abc
  barfly_barcheck.abc key_unknown_word.abc microtone_accidentals.abc
  chordname_too_long.abc decoration_line_end.abc)

# Broad coverage: an abc2midi (ABC->MIDI) and an abc2abc (ABC reformat) golden
# for every other sample, so each sample file has at least its MIDI rendering
# and its canonical-ABC form pinned.  coleraine is already covered above.
set(EXTRA_SAMPLES
  araber.abc
  baym_rebin.abc
  boys.abc
  daramud.abc
  demo.abc
  dergasn.abc
  detune.abc
  drums.abc
  temperament.abc
  words.abc
)
foreach(sample IN LISTS EXTRA_SAMPLES)
  if(sample IN_LIST ABC2MIDI_ERROR_SAMPLES)
    add_golden_test(TYPE abc2midi SAMPLE ${sample} EXPECT_ERRORS)
  else()
    add_golden_test(TYPE abc2midi SAMPLE ${sample})
  endif()
  add_golden_test(TYPE abc2abc  SAMPLE ${sample})
endforeach()

# Tune-level coverage: select individual tunes from the multi-tune sample
# files via their X: reference number.  These pin behaviour that the
# whole-file abc2midi golden does not exercise on its own:
#
#   demo tune 5  - @KMIDI KARAOKE FILE header, Lyric meta-events on a dedicated
#                  track, and P:(AB)3 header part spec (single voice, multi-track)
#   demo tune 4  - multiple voices (V:1..V:4)
#   demo tune 11 - multiple voices combined with a part spec
#   demo tune 12 - multiple voices
#   detune/temperament tune 2 - the second tune of each (whole-file golden
#                  only renders/locks tune 1 of these as the lead tune)
add_golden_test(TYPE abc2midi SAMPLE demo.abc NAME abc2midi_demo_4  TUNE 4)
add_golden_test(TYPE abc2midi SAMPLE demo.abc NAME abc2midi_demo_5  TUNE 5)
add_golden_test(TYPE abc2midi SAMPLE demo.abc NAME abc2midi_demo_11 TUNE 11)
add_golden_test(TYPE abc2midi SAMPLE demo.abc NAME abc2midi_demo_12 TUNE 12)
add_golden_test(TYPE abc2midi SAMPLE detune.abc      NAME abc2midi_detune_2      TUNE 2)
add_golden_test(TYPE abc2midi SAMPLE temperament.abc NAME abc2midi_temperament_2 TUNE 2)

# Karaoke tune 5 rendered with -PMAR: exercises Part Marker meta-event emission
# on the conductor track for a single-voice, multi-track tune driven by a
# header P:(AB)3 play-order spec (a path the partdemo -PMAR test does not cover).
add_golden_test(
  TYPE          abc2midi
  SAMPLE        demo.abc
  NAME          abc2midi_pmar_demo_5
  TUNE          5
  ABC2MIDI_ARGS -PMAR
)

# Karaoke tune 5 rendered with -lyrics: the w: words come out as standard MIDI
# Lyric (0x05) meta-events with bare syllables, and the Soft-Karaoke decoration
# (@KMIDI KARAOKE FILE / @I header lines and the / \ line/paragraph prefixes) is
# suppressed.  Contrast with abc2midi_demo_5, which pins the default 0x01 Text
# karaoke rendering of the same tune.
add_golden_test(
  TYPE          abc2midi
  SAMPLE        demo.abc
  NAME          abc2midi_lyrics_demo_5
  TUNE          5
  ABC2MIDI_ARGS -lyrics
)

# words.abc rendered with -lyrics, the focused companion to the default 0x01
# rendering pinned by abc2midi_words (registered above via EXTRA_SAMPLES).  The
# diff between the two goldens is the -lyrics contract on a small controlled
# input: 0x05 Lyric events with bare syllables, no @KMIDI/@I header, and no
# / \ line/paragraph prefixes, while still exercising the w: control codes
# (hyphen split, melisma _, hard space ~, blank *).
add_golden_test(
  TYPE          abc2midi
  SAMPLE        words.abc
  NAME          abc2midi_lyrics_words
  ABC2MIDI_ARGS -lyrics
)

# Regression test for the "%%MIDI beatstring" directive.  Pins the per-beat
# velocity pattern (f/m/p -> loud/medium/soft) that beatstring assigns to each
# metrical position, and guards against the heap-buffer-overflow the parser
# used to hit at the end of the directive string (see samples/beatstring.abc).
# Most valuable under the sanitizer build, where that overflow aborts abc2midi.
add_golden_test(TYPE abc2midi SAMPLE beatstring.abc)

# Regression test for -BF without a model number (issue #8): the runner puts
# other options after ABC2MIDI_ARGS, which used to turn Barfly mode off
# instead of selecting the default model 2 (see samples/barfly_option.abc).
add_golden_test(
  TYPE          abc2midi
  SAMPLE        barfly_option.abc
  ABC2MIDI_ARGS -BF
)

# Regression test for the process_microtones() stack-buffer-overflow: a K:
# field microtone modifier with an out-of-range note letter (^1/4H -> index 7
# into seven-element arrays) used to write past modmap[]/modmicrotone[].  Now
# rejected as an invalid microtone (see samples/microtone.abc).  Like the
# beatstring guard above, its value is under the sanitizer build, where the
# pre-fix overflow aborts abc2midi.
add_golden_test(TYPE abc2midi SAMPLE microtone.abc EXPECT_ERRORS)

# Regression test for GitHub issue #56: after the error for ^^ or __
# followed by a microtone, or for =^ or =_, the note is played as a plain
# note, with the key signature and the earlier accidentals of the bar, as
# abc2svg plays it (see samples/microtone_accidentals.abc).
add_golden_test(TYPE abc2midi SAMPLE microtone_accidentals.abc EXPECT_ERRORS)
add_golden_test(
  TYPE          abc2midi
  SAMPLE        microtone_accidentals.abc
  NAME          abc2midi_microtone_accidentals_2
  TUNE          2
  EXPECT_ERRORS
)

# Regression test for microtones of 2 semitones or more and for integer
# microtones without %%MIDI temperamentequal (see samples/microtone_wide.abc):
# the first were clipped to the +/- 2 semitones of the pitch bend, the
# second divided by zero.  The golden puts their whole semitones in the note,
# as abc2svg plays them.
add_golden_test(TYPE abc2midi SAMPLE microtone_wide.abc)

# Regression test for microtones in the key signature written with the
# shortcuts of the note syntax, such as K:G _/B for _1/2B (GitHub issue
# #10): process_microtones() used to ignore them.  Each bar played with the
# key signature is followed by the same bar with the microtones written on
# the notes (see samples/key_microtone.abc).
add_golden_test(TYPE abc2midi SAMPLE key_microtone.abc)

# Regression test for the gchord codes g to k and G to K: past the last note
# of a shorter chord they play that last note, k and K play the fifth note,
# and a chord in lower case plays nothing (see samples/gchord_codes.abc).
add_golden_test(TYPE abc2midi SAMPLE gchord_codes.abc)

# Regression test for GitHub issue #11: on an inverted chord such as "C/E",
# the gchord codes g to k play the notes from the lowest, the bass note,
# instead of from the root raised by an octave (see
# samples/gchord_inversion.abc).
add_golden_test(TYPE abc2midi SAMPLE gchord_inversion.abc)

# Regression test for %%MIDI chordname chords of 7 to 10 notes: the notes of
# a chord to play were written into gchordnotes[6] (genmidi.c), and those of
# a chordname in the file header into an array of 6 (store.c).  The overflow
# overwrote the count of notes, so that the test fails in every build, not
# only the sanitizer one (see samples/chordname_long.abc).
add_golden_test(TYPE abc2midi SAMPLE chordname_long.abc)

# Regression test for the pitchof_b() global-buffer-overflow: under a detune
# temperament an accidental can push a note's semitone offset outside 0..11
# (^b -> 12, _c -> -1), which used to index temperament_dt[] out of bounds.
# The lookup now folds to a pitch class (see samples/detune_accidentals.abc).
# Like the guards above, its value is under the sanitizer build, where the
# pre-fix overflow aborts abc2midi.
add_golden_test(TYPE abc2midi SAMPLE detune_accidentals.abc)

# Regression test for beat_modifier(), which applies the -BF 2 stress model
# bar by bar: it used to scan past the end of feature[] after the final bar
# line (an overflow the sanitizer build turns into a failure), and must still
# stress all the notes of a bar split across two lines (see
# samples/barfly_split_bar.abc).
add_golden_test(
  TYPE          abc2midi
  SAMPLE        barfly_split_bar.abc
  ABC2MIDI_ARGS -BF 2
)

# Regression test for the checkbar() stack-buffer-overflow: the warning for a
# bar of the wrong length was built in an 80-byte buffer, too small when the
# bar length is a fraction with large terms (see
# samples/barcheck_long_warning.abc).  Like the guards above, its value is
# under the sanitizer build, where the pre-fix overflow aborts abc2midi.
add_golden_test(TYPE abc2midi SAMPLE barcheck_long_warning.abc EXPECT_ERRORS)

# Regression test for the -BF 2 stress model on a bar longer than its time
# signature (a reel with the bar lines left out, see
# samples/barfly_missing_bar_lines.abc): beat_modifier() read the velocities
# past the end of ngain[] (an overflow the sanitizer build turns into a
# failure) and fdursum_at_segment() gave negative durations from the third
# bar on, shifting the notes that follow.  The golden equals the rendering of
# the same reel with its bar lines.
add_golden_test(
  TYPE          abc2midi
  SAMPLE        barfly_missing_bar_lines.abc
  ABC2MIDI_ARGS -BF 2
)

# Regression test for the dump_barloc() global-buffer-overflow: with -c,
# barloc.txt gives the bar sizes of each track from barloc[], which keeps
# 1024 bars, but dump_barloc() used to read it up to the number of bars of
# the track (see samples/barloc_long.abc, two voices of 1040 bars).  The
# golden pins barloc.txt, so that the test fails without the fix in every
# build: the sanitizer build aborts, the others dump 1040 values per track.
add_golden_test(TYPE barloc SAMPLE barloc_long.abc NAME abc2midi_barloc_long)

# Regression test for the -PMAR option (Part Marker meta-events).
# Locks in marker emission and verifies the markers do not perturb note
# timing or end-of-track time (bug fixed by carrying delta_time alongside
# delta_time_track0 when emitting markers on the conductor track).
add_golden_test(
  TYPE          abc2midi
  SAMPLE        partdemo.abc
  NAME          abc2midi_pmar_partdemo
  ABC2MIDI_ARGS -PMAR
)

# Regression test for GitHub issue #61: a pickup after a P: label is a bar
# of its own, like the pickup of the tune (see samples/part_pickup.abc, whose
# other tunes abc2midi_clean_part_pickup renders).  Before |: it was merged
# with the next bar, so that this bar got the beat accents and the guitar
# chords of the wrong places.  Tune 6, with -PMAR, pins [P:] labels in the
# lines of two voices: voice 1 used to play the notes of voice 2 after them.
add_golden_test(TYPE abc2midi SAMPLE part_pickup.abc)
add_golden_test(
  TYPE          abc2midi
  SAMPLE        part_pickup.abc
  NAME          abc2midi_pmar_part_pickup_6
  TUNE          6
  ABC2MIDI_ARGS -PMAR
)

# Regression test for GitHub issue #63: in a tune with w: lyrics, the
# Soft-Karaoke header of the conductor track is at time 0, with the tempo, the
# key signature and the time signature that follow it, and a tempo change is
# on its bar line, as in a tune without lyrics (see samples/words_tempo.abc).
# They were all one tick later.
add_golden_test(TYPE abc2midi SAMPLE words_tempo.abc)

# Regression test for \% in a text field, which is a percent sign and not
# the start of a comment (see samples/text_fields.abc): abc2midi named the
# track "50\" instead of "50% faster", and abc2abc must still write the
# fields as they are.
add_golden_test(TYPE abc2midi SAMPLE text_fields.abc)
add_golden_test(TYPE abc2abc  SAMPLE text_fields.abc)

# Regression test for GitHub issue #67: in a tune with several tracks, the
# conductor track ends with the other tracks after a tempo change (see
# samples/tempo_tracks.abc).  It ended later by the time of the last tempo
# change.  Tune 2 has a %%MIDI command between two tempo changes, which
# resets the delay that the end of this track was written with.
add_golden_test(TYPE abc2midi SAMPLE tempo_tracks.abc)
add_golden_test(
  TYPE          abc2midi
  SAMPLE        tempo_tracks.abc
  NAME          abc2midi_tempo_tracks_2
  TUNE          2
)

# Regression test for GitHub issue #71: the conductor track of a tune with
# several tracks has no copy of the events of the %%MIDI commands of its
# voice, such as %%MIDI control (see samples/control_tracks.abc).  The copy
# was at the time of the last tempo change of this track, or at time 0.
add_golden_test(TYPE abc2midi SAMPLE control_tracks.abc)

# Regression test for GitHub issue #73: white space may separate a shorthand
# decoration such as H or T from the note, rest or chord that it decorates
# (see samples/decoration_space.abc).  It was a "Malformed note", and the
# decoration was lost.  Tune 2 is the same music without this white space:
# its golden differs from that of tune 1 only by the title.
add_golden_test(TYPE abc2midi SAMPLE decoration_space.abc)
add_golden_test(
  TYPE          abc2midi
  SAMPLE        decoration_space.abc
  NAME          abc2midi_decoration_space_2
  TUNE          2
)

# Regression test for the bar check of a bar with fermatas: it counts the
# length written in the bar, not the longer one that the fermatas give to
# its notes, rests and chords (see samples/fermata_bars.abc), which made
# each such bar but the first one too long.  Tune 2 has %%MIDI
# fermatafixed.
add_golden_test(TYPE abc2midi SAMPLE fermata_bars.abc)
add_golden_test(
  TYPE          abc2midi
  SAMPLE        fermata_bars.abc
  NAME          abc2midi_fermata_bars_2
  TUNE          2
)

# --- Exit-status tests ---
#
# These pin abc2midi's exit status, and optionally its messages, for given
# inputs and options.  Test names start with "abc2midi_" so that the CI
# per-tool job (ctest -R '^(...|abc2midi_)') runs them too.
#
# Optional REGEX is a regular expression that abc2midi's stdout must match.
# Option ALL_TUNES renders every tune of the sample (no -o) instead of one.
function(add_exit_status_test)
  cmake_parse_arguments(T "ALL_TUNES" "NAME;SAMPLE;RC;REGEX" "ABC2MIDI_ARGS" ${ARGN})
  add_test(
    NAME "${T_NAME}"
    COMMAND "${CMAKE_COMMAND}"
      -DABC2MIDI=$<TARGET_FILE:abc2midi>
      -DSAMPLE=${SAMPLES_DIR}/${T_SAMPLE}
      -DTMPDIR=${TEST_TMPDIR}
      -DNAME=${T_NAME}
      -DEXPECTED_RC=${T_RC}
      -DALL_TUNES=${T_ALL_TUNES}
      "-DEXPECTED_REGEX=${T_REGEX}"
      "-DABC2MIDI_ARGS=${T_ABC2MIDI_ARGS}"
      "-DLAUNCHER=${ABCMIDI_LAUNCHER}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/run_exit_status.cmake"
  )
  set_tests_properties("${T_NAME}" PROPERTIES LABELS "exit-status")
endfunction()

# -BF on a tune with no stress model for its rhythm (tune 1: R:Polka in 2/2,
# from EasyABC ticket #81) or with no R: at all (tune 2) is a warning: the
# tune is rendered without stress and abc2midi exits 0.  -Werror turns the
# warning into an error and the exit status into 1.
add_exit_status_test(
  NAME          abc2midi_bf_unknown_rhythm
  SAMPLE        barfly_rhythm.abc
  ABC2MIDI_ARGS 1 -BF 2
  RC            0
  REGEX         "Warning in line-char [0-9]+-[0-9]+ : invalid R: designator"
)
add_exit_status_test(
  NAME          abc2midi_bf_unknown_rhythm_werror
  SAMPLE        barfly_rhythm.abc
  ABC2MIDI_ARGS 1 -BF 2 -Werror
  RC            1
  REGEX         "Error in line-char [0-9]+-[0-9]+ : invalid R: designator"
)
add_exit_status_test(
  NAME          abc2midi_bf_no_rhythm
  SAMPLE        barfly_rhythm.abc
  ABC2MIDI_ARGS 2 -BF 2
  RC            0
  REGEX         "Warning in line-char [0-9]+-[0-9]+ : No R: in header"
)
add_exit_status_test(
  NAME          abc2midi_bf_no_rhythm_werror
  SAMPLE        barfly_rhythm.abc
  ABC2MIDI_ARGS 2 -BF 2 -Werror
  RC            1
  REGEX         "Error in line-char [0-9]+-[0-9]+ : No R: in header"
)

# -BF must find a stress model despite spelling differences with the table
# in stresspat.c (M:2/2 for C|, Slipjig for Slip Jig) and for its last
# entry (Tango 2/4), which an off-by-one model count used to hide.  With
# -Werror a missed lookup would exit 1.
foreach(tune IN ITEMS 1 2 3)
  add_exit_status_test(
    NAME          abc2midi_bf_aliases_${tune}
    SAMPLE        barfly_aliases.abc
    ABC2MIDI_ARGS ${tune} -BF 2 -Werror
    RC            0
  )
endforeach()

# With -BF 2, beat_modifier() changes the note lengths before checkbar()
# counts them, so the bar check warning must say that its count is after the
# stress model (see samples/barfly_barcheck.abc).
add_exit_status_test(
  NAME          abc2midi_bf_barcheck
  SAMPLE        barfly_barcheck.abc
  ABC2MIDI_ARGS -BF 2
  RC            0
  REGEX         "Warning in line-char [0-9]+-[0-9]+ : Track 0 Bar 3 has 4499/500 time units \\(after the -BF stress model\\) while the time signature has 6"
)

# An unknown word in K: must be reported wherever it is in the field, not
# only as its last word (see samples/key_unknown_word.abc).
add_exit_status_test(
  NAME          abc2midi_key_unknown_word
  SAMPLE        key_unknown_word.abc
  RC            0
  REGEX         "Warning in line-char [0-9]+-[0-9]+ : Ignoring string 'xyz' in K: field"
)

# A microtone follows a single ^ or _, as in abc2svg (GitHub issue #56):
# ^^ or __ followed by a microtone, and =^ or =_, are errors that give the
# valid spelling (see samples/microtone_accidentals.abc).
add_exit_status_test(
  NAME          abc2midi_microtone_double_accidental
  SAMPLE        microtone_accidentals.abc
  ABC2MIDI_ARGS 1
  RC            1
  REGEX         "Error in line-char [0-9]+-[0-9]+ : \\^\\^/4 is not a valid microtone: write \\^5/4.*Error in line-char [0-9]+-[0-9]+ : __/2 is not a valid microtone: write _3/2.*Error in line-char [0-9]+-[0-9]+ : \\^\\^1 is not a valid microtone: a microtone follows a single \\^"
)
add_exit_status_test(
  NAME          abc2midi_microtone_natural_accidental
  SAMPLE        microtone_accidentals.abc
  ABC2MIDI_ARGS 2
  RC            1
  REGEX         "Error in line-char [0-9]+-[0-9]+ : =\\^ is not a valid accidental: write \\^ without =.*Error in line-char [0-9]+-[0-9]+ : =_ is not a valid accidental: write _ without ="
)

# A %%MIDI chordname of more than 10 notes is an error.  event_midi()
# (store.c) used to read its notes past the end of an array of 10 first, which
# the sanitizer build turns into a failure (see samples/chordname_too_long.abc).
add_exit_status_test(
  NAME          abc2midi_chordname_too_long
  SAMPLE        chordname_too_long.abc
  RC            1
  REGEX         "Error in line-char [0-9]+-[0-9]+ : Named chord cannot have more than 10 notes"
)

# A shorthand decoration at the end of a line of music is an error.
# parsenote() (parseabc.c) used to take the end of the line for a decoration
# and write past the end of its array of decorations, which the sanitizer
# build turns into a failure (see samples/decoration_line_end.abc).
add_exit_status_test(
  NAME          abc2midi_decoration_line_end
  SAMPLE        decoration_line_end.abc
  RC            1
  REGEX         "Error in line-char [0-9]+-[0-9]+ : Malformed note : expecting a-g or A-G"
)

# -Werror alone must not fail a tune that raises no warning.
add_exit_status_test(
  NAME          abc2midi_werror_clean
  SAMPLE        coleraine.abc
  ABC2MIDI_ARGS -Werror
  RC            0
)

# Every tune of every sample must be free of warnings and errors: the golden
# tests only render the tune they select (with -o, the first one by default),
# so this renders whole files.  New samples are picked up when CMake is
# re-run.
file(GLOB all_samples RELATIVE "${SAMPLES_DIR}" "${SAMPLES_DIR}/*.abc")
foreach(sample IN LISTS all_samples)
  get_filename_component(stem "${sample}" NAME_WE)
  if(sample IN_LIST ABC2MIDI_ERROR_SAMPLES)
    set(rc 1)
  else()
    set(rc 0)
  endif()
  add_exit_status_test(
    NAME          abc2midi_clean_${stem}
    SAMPLE        ${sample}
    ABC2MIDI_ARGS -Werror
    RC            ${rc}
    ALL_TUNES
  )
endforeach()

# Every sample through abc2abc, yaps and abcmatch, whose golden tests cover
# only some samples: the program must exit 0.  The value of these tests is
# under the sanitize and valgrind presets, where a memory error fails them,
# such as yaps reading past a title that ends with a backslash (see
# samples/text_fields.abc).  Test names start with the program name so that
# the CI per-tool jobs run them.
file(MAKE_DIRECTORY "${TEST_TMPDIR}")
foreach(sample IN LISTS all_samples)
  get_filename_component(stem "${sample}" NAME_WE)
  add_test(NAME abc2abc_sample_${stem}
    COMMAND ${ABCMIDI_LAUNCHER} $<TARGET_FILE:abc2abc> "${SAMPLES_DIR}/${sample}"
    WORKING_DIRECTORY "${TEST_TMPDIR}")
  add_test(NAME yaps_sample_${stem}
    COMMAND ${ABCMIDI_LAUNCHER} $<TARGET_FILE:yaps> "${SAMPLES_DIR}/${sample}"
      -o "${TEST_TMPDIR}/yaps_sample_${stem}.ps"
    WORKING_DIRECTORY "${TEST_TMPDIR}")
  add_test(NAME abcmatch_sample_${stem}
    COMMAND ${ABCMIDI_LAUNCHER} $<TARGET_FILE:abcmatch> "${SAMPLES_DIR}/${sample}"
      -pitch_hist
    WORKING_DIRECTORY "${TEST_TMPDIR}")
  set_tests_properties(abc2abc_sample_${stem} yaps_sample_${stem}
    abcmatch_sample_${stem} PROPERTIES LABELS "sample")
endforeach()

# --- Regenerate-golden convenience target ---
#
# Usage:  cmake --build build/debug --target update-golden
#
# Equivalent to setting ABCMIDI_UPDATE_GOLDEN=1 and running ctest, but easier
# to discover via cmake --build --target.

add_custom_target(update-golden
  COMMAND ${CMAKE_COMMAND} -E env ABCMIDI_UPDATE_GOLDEN=1
    ${CMAKE_CTEST_COMMAND} -L golden --output-on-failure
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Regenerating golden test references"
  USES_TERMINAL
)

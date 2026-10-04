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
#                      diffable text.  Volatile lines (version banners, dates,
#                      tmpdir paths) are stripped before comparison.  abc2midi
#                      runs with -Werror and must exit 0, so the samples stay
#                      free of warnings and errors.
#
#   3. Exit-status tests - abc2midi's exit status and messages on specific
#                      inputs and options, and every tune of every sample
#                      rendered with -Werror.
#
# To regenerate golden files after an intentional change:
#
#   ABCMIDI_UPDATE_GOLDEN=1 ctest --preset debug
#
# Then review the diff and commit the updated tests/golden/*.txt files.

set(SAMPLES_DIR "${PROJECT_SOURCE_DIR}/samples")
set(GOLDEN_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/golden")
set(TEST_TMPDIR "${CMAKE_CURRENT_BINARY_DIR}/tmp")

# --- Smoke tests: -ver on every binary ---

foreach(bin IN LISTS ABCMIDI_BINARIES)
  add_test(NAME "smoke_${bin}_ver" COMMAND $<TARGET_FILE:${bin}> -ver)
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
set(ABC2MIDI_ERROR_SAMPLES microtone.abc)

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

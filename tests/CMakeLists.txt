# abcmidi test suite
#
# Two layers of tests:
#
#   1. Smoke tests   - every binary runs with -ver and exits cleanly.
#                      Catches link errors, missing libraries, trivial crashes.
#
#   2. Golden tests  - each program is run on a sample input and its output
#                      is compared with a checked-in reference file.  Binary
#                      MIDI outputs are first piped through mftext to produce
#                      diffable text.  Volatile lines (version banners, dates,
#                      tmpdir paths) are stripped before comparison.
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
function(add_golden_test)
  cmake_parse_arguments(T "" "TYPE;SAMPLE;NAME;TUNE" "ABC2MIDI_ARGS" ${ARGN})

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
  add_golden_test(TYPE abc2midi SAMPLE ${sample})
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

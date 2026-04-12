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

# Helper: register a golden test for one program against one sample
function(add_golden_test)
  cmake_parse_arguments(T "" "TYPE;SAMPLE" "" ${ARGN})

  get_filename_component(stem "${T_SAMPLE}" NAME_WE)
  set(test_name "${T_TYPE}_${stem}")
  set(golden    "${GOLDEN_DIR}/${test_name}.txt")

  add_test(
    NAME "${test_name}"
    COMMAND "${CMAKE_COMMAND}"
      -DTYPE=${T_TYPE}
      -DSAMPLE=${SAMPLES_DIR}/${T_SAMPLE}
      -DGOLDEN=${golden}
      -DTMPDIR=${TEST_TMPDIR}
      -DABC2MIDI=$<TARGET_FILE:abc2midi>
      -DABC2ABC=$<TARGET_FILE:abc2abc>
      -DMIDI2ABC=$<TARGET_FILE:midi2abc>
      -DMIDISTATS=$<TARGET_FILE:midistats>
      -DMFTEXT=$<TARGET_FILE:mftext>
      -DYAPS=$<TARGET_FILE:yaps>
      -DMIDICOPY=$<TARGET_FILE:midicopy>
      -DABCMATCH=$<TARGET_FILE:abcmatch>
      -P "${CMAKE_CURRENT_SOURCE_DIR}/run_test.cmake"
  )
  set_tests_properties("${test_name}" PROPERTIES LABELS "golden")
endfunction()

# One golden test per program against coleraine.abc
foreach(type IN ITEMS abc2midi abc2abc midi2abc midistats mftext yaps midicopy abcmatch)
  add_golden_test(TYPE ${type} SAMPLE coleraine.abc)
endforeach()

# A few extra coverage points on different samples
add_golden_test(TYPE abc2midi SAMPLE demo.abc)
add_golden_test(TYPE abc2abc  SAMPLE demo.abc)

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

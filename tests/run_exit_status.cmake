# Exit-status test runner for abc2midi.
#
# The golden tests deliberately ignore abc2midi's exit status (they only need
# the best-effort MIDI file), so this runner covers what they cannot: whether
# a run exits 0 or 1, and whether a given message was reported.
#
# Required variables (passed via -D on the cmake command line):
#   ABC2MIDI     - absolute path to the abc2midi binary
#   SAMPLE       - path to the input ABC sample file
#   TMPDIR       - working directory for the temporary MIDI output
#   NAME         - unique test name, used for the temporary file name
#   EXPECTED_RC  - expected exit status
#
# Optional variables:
#   ABC2MIDI_ARGS   - extra arguments (CMake list), e.g. "1;-BF;2;-Werror"
#   EXPECTED_REGEX  - regular expression that stdout must match

cmake_minimum_required(VERSION 3.14)

if(NOT ABC2MIDI OR NOT SAMPLE OR NOT TMPDIR OR NOT NAME
   OR NOT DEFINED EXPECTED_RC)
  message(FATAL_ERROR
    "run_exit_status.cmake: ABC2MIDI, SAMPLE, TMPDIR, NAME, EXPECTED_RC are required")
endif()

file(MAKE_DIRECTORY "${TMPDIR}")
set(midfile "${TMPDIR}/${NAME}.mid")

execute_process(
  COMMAND "${ABC2MIDI}" "${SAMPLE}" ${ABC2MIDI_ARGS} -o "${midfile}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE  err
)

set(details
  "  ${ABC2MIDI} ${SAMPLE} ${ABC2MIDI_ARGS} -o ${midfile}\n"
  "--- stdout ---\n${out}\n--- stderr ---\n${err}")

if(NOT rc STREQUAL "${EXPECTED_RC}")
  message(FATAL_ERROR "Exit status ${rc}, expected ${EXPECTED_RC}:\n" ${details})
endif()

if(EXPECTED_REGEX AND NOT out MATCHES "${EXPECTED_REGEX}")
  message(FATAL_ERROR "stdout does not match '${EXPECTED_REGEX}':\n" ${details})
endif()

# Whatever the status, abc2midi must still write its best-effort MIDI file.
if(NOT EXISTS "${midfile}")
  message(FATAL_ERROR "abc2midi produced no MIDI file:\n" ${details})
endif()

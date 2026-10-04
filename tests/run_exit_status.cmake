# Exit-status test runner for abc2midi.
#
# The golden tests check abc2midi's exit status only for the plain runs that
# render their goldens; this runner pins it for given inputs and options,
# can check that a given message was reported, and can render every tune of
# a file rather than one.
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
#   ALL_TUNES       - true to render every tune of SAMPLE instead of only the
#                     first (or selected) one

cmake_minimum_required(VERSION 3.14)

if(NOT ABC2MIDI OR NOT SAMPLE OR NOT TMPDIR OR NOT NAME
   OR NOT DEFINED EXPECTED_RC)
  message(FATAL_ERROR
    "run_exit_status.cmake: ABC2MIDI, SAMPLE, TMPDIR, NAME, EXPECTED_RC are required")
endif()

file(MAKE_DIRECTORY "${TMPDIR}")

if(ALL_TUNES)
  # Without -o abc2midi writes one <stem>N.mid per tune next to its input,
  # so it runs on a copy of the sample in a directory of its own.
  set(workdir "${TMPDIR}/${NAME}")
  file(REMOVE_RECURSE "${workdir}")
  file(MAKE_DIRECTORY "${workdir}")
  file(COPY "${SAMPLE}" DESTINATION "${workdir}")
  get_filename_component(input "${SAMPLE}" NAME)
  set(command "${ABC2MIDI}" "${input}" ${ABC2MIDI_ARGS})
else()
  set(workdir "${TMPDIR}")
  set(midfile "${TMPDIR}/${NAME}.mid")
  set(command "${ABC2MIDI}" "${SAMPLE}" ${ABC2MIDI_ARGS} -o "${midfile}")
endif()

execute_process(
  COMMAND ${command}
  WORKING_DIRECTORY "${workdir}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE  err
)

string(REPLACE ";" " " command_line "${command}")
set(details
  "  (in ${workdir}) ${command_line}\n"
  "--- stdout ---\n${out}\n--- stderr ---\n${err}")

if(NOT rc STREQUAL "${EXPECTED_RC}")
  message(FATAL_ERROR "Exit status ${rc}, expected ${EXPECTED_RC}:\n" ${details})
endif()

if(EXPECTED_REGEX AND NOT out MATCHES "${EXPECTED_REGEX}")
  message(FATAL_ERROR "stdout does not match '${EXPECTED_REGEX}':\n" ${details})
endif()

# Whatever the status, abc2midi must still write its best-effort MIDI output.
if(ALL_TUNES)
  file(GLOB midfiles "${workdir}/*.mid")
  if(NOT midfiles)
    message(FATAL_ERROR "abc2midi produced no MIDI file:\n" ${details})
  endif()
elseif(NOT EXISTS "${midfile}")
  message(FATAL_ERROR "abc2midi produced no MIDI file:\n" ${details})
endif()

# Generic test runner for abcmidi golden-file tests.
#
# Required variables (passed via -D on the cmake command line):
#   TYPE     - one of: abc2midi, abc2abc, midi2abc, midistats, mftext,
#              yaps, midicopy, abcmatch
#   SAMPLE   - path to the input ABC sample file
#   GOLDEN   - path to the golden reference file
#   TMPDIR   - working directory for temporary outputs
#
#   ABC2MIDI, ABC2ABC, MIDI2ABC, MIDISTATS, MFTEXT, YAPS, MIDICOPY, ABCMATCH
#            - absolute paths to the binaries (only those needed for TYPE)
#
# Behaviour:
#   - Runs the requested binary (and any pipeline steps required to make the
#     output diffable, e.g. mftext for binary MIDI output).
#   - Strips non-deterministic lines (version banners, dates) from the output.
#   - If the environment variable ABCMIDI_UPDATE_GOLDEN is set, the filtered
#     output is written to GOLDEN (used to regenerate references after an
#     intentional behavioural change).
#   - Otherwise, the filtered output is compared with GOLDEN; on mismatch, a
#     unified diff is printed and the test fails.

cmake_minimum_required(VERSION 3.14)

if(NOT TYPE OR NOT SAMPLE OR NOT GOLDEN OR NOT TMPDIR)
  message(FATAL_ERROR "run_test.cmake: TYPE, SAMPLE, GOLDEN, TMPDIR are required")
endif()

file(MAKE_DIRECTORY "${TMPDIR}")
get_filename_component(stem "${SAMPLE}" NAME_WE)

set(raw     "${TMPDIR}/${TYPE}_${stem}.raw")
set(out     "${TMPDIR}/${TYPE}_${stem}.out")
set(midfile "${TMPDIR}/${TYPE}_${stem}.mid")

# --- Command helpers ---------------------------------------------------------

# Run a command; abort with a detailed message on non-zero exit.
function(run_or_die)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE captured_out
    ERROR_VARIABLE  captured_err
  )
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR
      "Command failed (rc=${rc}):\n  ${ARGN}\n"
      "--- stdout ---\n${captured_out}\n"
      "--- stderr ---\n${captured_err}")
  endif()
endfunction()

# Run a command and capture its stdout into a file.
function(run_to_file outfile)
  execute_process(
    COMMAND ${ARGN}
    OUTPUT_FILE "${outfile}"
    RESULT_VARIABLE rc
    ERROR_VARIABLE  captured_err
  )
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR
      "Command failed (rc=${rc}):\n  ${ARGN}\n--- stderr ---\n${captured_err}")
  endif()
endfunction()

# Convert ${SAMPLE} to MIDI, then run ${bin} on the resulting MIDI file and
# capture its stdout.  Any extra arguments are inserted BEFORE the MIDI path
# (needed e.g. for "midi2abc -f <file>").
function(run_via_mid outfile bin)
  run_or_die("${ABC2MIDI}" "${SAMPLE}" -o "${midfile}" -quiet -silent)
  run_to_file("${outfile}" "${bin}" ${ARGN} "${midfile}")
endfunction()

# Run ${bin} directly on ${SAMPLE} and capture its stdout.  Extra arguments
# are appended after the sample path.
function(run_on_sample outfile bin)
  run_to_file("${outfile}" "${bin}" "${SAMPLE}" ${ARGN})
endfunction()

# --- Dispatch: populate ${raw} based on TYPE ---------------------------------
#
# The binary path is derived from TYPE via TOUPPER + double-dereference:
# for TYPE=midistats, ${bin} resolves to ${MIDISTATS}.  The only exception is
# abc2midi, where we render the MIDI output through mftext for diffing — it
# is therefore an alias for the mftext test.

string(TOUPPER "${TYPE}" TYPE_UPPER)
if(TYPE STREQUAL "abc2midi")
  set(bin "${MFTEXT}")
else()
  set(bin "${${TYPE_UPPER}}")
endif()

if(TYPE MATCHES "^(abc2midi|mftext|midistats)$")
  run_via_mid("${raw}" "${bin}")

elseif(TYPE STREQUAL "midi2abc")
  run_via_mid("${raw}" "${bin}" -f)

elseif(TYPE STREQUAL "abc2abc")
  run_on_sample("${raw}" "${bin}")

elseif(TYPE STREQUAL "abcmatch")
  # -pitch_hist gives a short, deterministic, useful summary of the tune.
  run_on_sample("${raw}" "${bin}" -pitch_hist)

elseif(TYPE STREQUAL "yaps")
  set(psfile "${TMPDIR}/yaps_${stem}.ps")
  run_or_die("${bin}" "${SAMPLE}" -o "${psfile}")
  configure_file("${psfile}" "${raw}" COPYONLY)

elseif(TYPE STREQUAL "midicopy")
  # ABC -> MIDI -> midicopy -> mftext
  set(copied "${TMPDIR}/midicopy_${stem}_out.mid")
  run_or_die("${ABC2MIDI}" "${SAMPLE}" -o "${midfile}" -quiet -silent)
  run_or_die("${bin}" "${midfile}" "${copied}")
  run_to_file("${raw}" "${MFTEXT}" "${copied}")

else()
  message(FATAL_ERROR "Unknown TYPE: ${TYPE}")
endif()

# --- Normalize: strip non-deterministic lines --------------------------------
#
# Each program prints a version line as the first line of stdout.  yaps
# PostScript output contains a CreationDate.  Filenames embedded in output
# (e.g. yaps "%%Title: /tmp/...") are also volatile and stripped.

file(READ "${raw}" content)

# Strip program version banners (e.g. "5.02 February 16 2025 abc2midi")
string(REGEX REPLACE "[0-9]+\\.[0-9]+ +[A-Za-z]+ +[0-9]+ +[0-9]+ +[a-z2]+\n" "" content "${content}")

# Strip yaps PostScript volatile headers
string(REGEX REPLACE "%%Title:[^\n]*\n"        "%%Title: <stripped>\n"        content "${content}")
string(REGEX REPLACE "%%CreationDate:[^\n]*\n" "%%CreationDate: <stripped>\n" content "${content}")

# Strip absolute paths to TMPDIR (filenames embedded in some outputs)
string(REPLACE "${TMPDIR}/" "" content "${content}")

file(WRITE "${out}" "${content}")

# --- Update or compare -------------------------------------------------------

if(DEFINED ENV{ABCMIDI_UPDATE_GOLDEN})
  get_filename_component(golden_dir "${GOLDEN}" DIRECTORY)
  file(MAKE_DIRECTORY "${golden_dir}")
  file(WRITE "${GOLDEN}" "${content}")
  message(STATUS "Updated golden: ${GOLDEN}")
  return()
endif()

if(NOT EXISTS "${GOLDEN}")
  message(FATAL_ERROR
    "Golden file does not exist: ${GOLDEN}\n"
    "Run 'ABCMIDI_UPDATE_GOLDEN=1 ctest ...' to generate it.")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E compare_files "${out}" "${GOLDEN}"
  RESULT_VARIABLE diff_rc
  OUTPUT_QUIET
  ERROR_QUIET
)

if(NOT diff_rc EQUAL 0)
  execute_process(
    COMMAND diff -u "${GOLDEN}" "${out}"
    OUTPUT_VARIABLE diff_text
  )
  message(FATAL_ERROR
    "Output differs from golden ${GOLDEN}\n"
    "To accept the new output: ABCMIDI_UPDATE_GOLDEN=1 ctest ...\n"
    "${diff_text}")
endif()

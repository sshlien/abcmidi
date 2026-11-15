# Smoke test runner: a program run with -ver must exit 0 and print its
# version, "<n.nn> <date> <program>", as the first line of stdout.
#
# Required variables (passed via -D on the cmake command line):
#   BIN   - absolute path to the binary
#   NAME  - program name, the last word of the version line
#
# Optional variables:
#   LAUNCHER - command (CMake list) that the program is run with, such as
#              valgrind and its options; empty to run it directly

cmake_minimum_required(VERSION 3.14)

if(NOT BIN OR NOT NAME)
  message(FATAL_ERROR "run_smoke.cmake: BIN and NAME are required")
endif()

execute_process(
  COMMAND ${LAUNCHER} "${BIN}" -ver
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE  err
)

set(details "--- stdout ---\n${out}\n--- stderr ---\n${err}")

if(NOT rc STREQUAL "0")
  message(FATAL_ERROR "${NAME} -ver exited with status ${rc}:\n" ${details})
endif()

# The date has a day, as in "5.14 November 14 2026 abc2midi", or not.
if(NOT out MATCHES "^[0-9]+\\.[0-9]+ [A-Z][a-z]+ ([0-9]+ )?[0-9]+ ${NAME}\n")
  message(FATAL_ERROR "${NAME} -ver does not print its version:\n" ${details})
endif()

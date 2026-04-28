cmake_minimum_required(VERSION 3.14)

# The package release date lives in the VERSION file at the project root —
# single source of truth, also read by configure.ac (via m4_esyscmd_s) and
# installed as a doc file.  We do not pass it to project(VERSION ...) because
# the date format ("April 25 2026") is not the MAJOR.MINOR.PATCH that CMake
# expects there; PROJECT_VERSION is not used elsewhere in this build.
file(STRINGS VERSION ABCMIDI_VERSION LIMIT_COUNT 1)

project(abcmidi
  DESCRIPTION "ABC music notation tools: converters between ABC, MIDI, and PostScript"
  LANGUAGES C
)

# Modern CMake: export compile_commands.json for clangd/LSP
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# --- Options ---

option(ABCMIDI_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# --- Compiler configuration ---

# Common compile options applied to every target via an INTERFACE library
add_library(abcmidi_common INTERFACE)

target_compile_definitions(abcmidi_common INTERFACE
  ANSILIBS
)

# The codebase mixes K&R empty-parens "()" (meaning unspecified args) with
# ANSI typed prototypes.  In C23/gnu23 (GCC 15+ default), "()" means "(void)",
# making these a hard error.  Pin to gnu89 until the ANSI migration is complete,
# then switch to c_std_11 or later.
target_compile_options(abcmidi_common INTERFACE
  $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:
    -std=gnu89
    -Wall
  >
)

if(ABCMIDI_SANITIZERS)
  target_compile_options(abcmidi_common INTERFACE
    -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(abcmidi_common INTERFACE
    -fsanitize=address,undefined)
endif()

# --- Shared object libraries (compiled once, linked into multiple binaries) ---

add_library(obj_midifile OBJECT midifile.c)
target_link_libraries(obj_midifile PUBLIC abcmidi_common)

add_library(obj_parseabc OBJECT parseabc.c music_utils.c)
target_link_libraries(obj_parseabc PUBLIC abcmidi_common)

add_library(obj_parser2 OBJECT parser2.c)
target_link_libraries(obj_parser2 PUBLIC abcmidi_common)

# --- Executables ---

# abc2midi: ABC notation → MIDI
add_executable(abc2midi
  store.c genmidi.c queues.c stresspat.c
)
target_link_libraries(abc2midi PRIVATE
  obj_parseabc obj_parser2 obj_midifile abcmidi_common m)

# abc2abc: ABC notation → ABC notation (transposition, reformatting)
add_executable(abc2abc toabc.c)
target_link_libraries(abc2abc PRIVATE obj_parseabc abcmidi_common m)

# midi2abc: MIDI → ABC notation
add_executable(midi2abc midi2abc.c)
target_link_libraries(midi2abc PRIVATE obj_midifile abcmidi_common m)

# midistats: MIDI file statistics
add_executable(midistats midistats.c)
target_link_libraries(midistats PRIVATE obj_midifile abcmidi_common m)

# mftext: MIDI → human-readable text
add_executable(mftext mftext.c crack.c)
target_link_libraries(mftext PRIVATE obj_midifile abcmidi_common m)

# yaps: ABC notation → PostScript
add_executable(yaps
  yapstree.c drawtune.c debug.c pslib.c position.c
)
target_link_libraries(yaps PRIVATE
  obj_parseabc obj_parser2 abcmidi_common m)

# midicopy: MIDI file filtering/extraction
add_executable(midicopy midicopy.c)
target_link_libraries(midicopy PRIVATE abcmidi_common m)

# abcmatch: ABC tune matching/comparison
add_executable(abcmatch abcmatch.c matchsup.c)
target_link_libraries(abcmatch PRIVATE obj_parseabc abcmidi_common m)

# --- Install ---

include(GNUInstallDirs)

set(ABCMIDI_BINARIES abc2midi abc2abc midi2abc midistats mftext yaps midicopy abcmatch)

install(TARGETS ${ABCMIDI_BINARIES}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Match the legacy Makefile: install only top-level doc files, not subdirectories
install(FILES
  doc/abcguide.txt
  doc/abcmatch.txt
  doc/gpl.txt
  doc/history.txt
  doc/hudsonshift.txt
  doc/readme.txt
  doc/yapshelp.txt
  doc/AUTHORS
  doc/CHANGES
  VERSION
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

install(FILES
  doc/abc2abc.1
  doc/abc2midi.1
  doc/abcmatch.1
  doc/mftext.1
  doc/midi2abc.1
  doc/midicopy.1
  doc/midistats.1
  doc/yaps.1
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
)

# --- Tests ---

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

# TODO: install-validation test that verifies all binaries, doc files, and man
# pages are installed to the expected paths.

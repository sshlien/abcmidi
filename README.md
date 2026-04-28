### abcMIDI package

[![Tests](https://github.com/keryell/abcmidi/actions/workflows/test.yml/badge.svg)](https://github.com/keryell/abcmidi/actions/workflows/test.yml)

abcMIDI is a package of programs written in C for handling [abc music notation](http://abcnotation.com/) files. The software was created by James Allwright in the early 1990 and presently maintained by Seymour Shlien. It initially included the following programs:

1. abc2midi for converting an abc file to a midi file,
2. abc2abc for transposing abc notation to another key signature,
3. midi2abc for creating an abc file from a midi file,
4. yaps for producing a PostScript file displaying the abc file in common music notation and,
5. mftext for creating a text representation of a midi file.

Seymour added two more programs:

1. abcmatch for finding common elements in a collection of abc tunes and,
2. midicopy for copying parts of a midi file to a new midi file.

Yaps has been superceded by [Jef Moine](http://moinejf.free.fr/) abcm2ps and abc2svg programs. Midi2abc has been expanded to include mftext and various other features for supporting the [runabc](https://ifdo.ca/~seymour/runabc/top.html) application. Abc2midi has numerous new features that are described in its own web page [abc2midi guide](https://abcmidi.sourceforge.io).

Components of the abcMIDI package are parts of numerous applications for creating and editing abc files. Compilations of these components for various operating systems can be found on [The ABC Plus Project](http://abcplus.sourceforge.net/) web page.


The latest version of the abcMIDI package supported by James Allwright can be found can be found [here](http://abc.sourceforge.net/abcMIDI/original/). More recent versions can be found on [sourceforge](https://sourceforge.net/projects/abc/) and on the [runabc](https://ifdo.ca/~seymour/runabc/top.html) web page.


### Building

#### Autoconf (legacy)

The traditional build uses autoconf:

```sh
./configure
make
sudo make install
```

#### CMake (modern)

A CMake build system is available alongside the legacy one, with
[presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
for common configurations:

```sh
# Configure and build (pick a preset: default, debug, sanitize)
cmake --preset default
cmake --build --preset default

# Install
cmake --install build/default
```

Available presets:

| Preset     | Build type | Description                                  |
|------------|------------|----------------------------------------------|
| `default`  | Release    | Optimized build                              |
| `debug`    | Debug      | Debug symbols, no optimization               |
| `sanitize` | Debug      | Debug + AddressSanitizer + UndefinedBehaviorSanitizer |

The CMake build generates `compile_commands.json` for use with
clangd and other LSP-based editors.

### Testing

The CMake build includes a test suite covering all 8 programs:

- **Smoke tests** verify each binary runs cleanly with `-ver`.
- **Golden-file tests** run each program on a sample input and compare the
  (normalized) output to a checked-in reference. Binary MIDI outputs are
  piped through `mftext` to produce diffable text. Volatile lines (version
  banners, dates, temporary paths) are stripped before comparison.

```sh
# Run all tests
ctest --preset debug

# Run only golden-file tests / only smoke tests
ctest --preset debug -L golden
ctest --preset debug -L smoke
```

To regenerate the golden files after an intentional behavioural change,
review the diff, then commit:

```sh
cmake --build build/debug --target update-golden
git diff tests/golden/
```

### Maintainers / releasing

The package release date in the `VERSION` file is the single source of
truth for the package version. It is read by `CMakeLists.txt` (into
`ABCMIDI_VERSION`) and spliced into `configure.ac`'s `AC_INIT` via
`m4_esyscmd_s` at autoreconf time. Each individual program also keeps its
own `#define VERSION "<n.nn> <date> <toolname>"` in its `.c` file
(e.g. `store.c` for `abc2midi`); these are bumped per-tool when that
tool's behaviour changes.

To cut a release:

1. Update the `VERSION` file (e.g. `April 25 2026`).
2. For each tool whose behaviour changed since the last release, bump
   its `#define VERSION` string in the corresponding source file.
3. Run `autoreconf -f` so the committed `configure` picks up the new
   `AC_INIT` arguments. The CMake build does not need this step — it
   reads `VERSION` directly at configure time.
4. Run `ctest --preset debug` and, if a golden-file test fails because
   of an intentional output change, regenerate with
   `cmake --build build/debug --target update-golden` and review the
   diff.
5. Append an entry to `doc/CHANGES` and update the per-tool version
   listing at the top of `doc/readme.txt`.
6. Commit and tag.

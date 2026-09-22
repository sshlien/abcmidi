# Exit status of the abcMIDI programs

September 21 2026 [RK]

This file lists every way each program in the package can terminate and
the exit status it returns, as found by reading every `exit()` call and
every return from `main()` in the sources. It is meant both for people
scripting around these tools (what can you rely on from `$?`) and for
maintainers (so that a change to an exit path also updates this file).

## Overview

There is no package-wide convention; each program grew its own. Only four
status values are ever produced:

| Status | Meaning in practice |
|-------:|---------------------|
| 0 | Normal completion, `-h`/usage, `-ver` — and, in several programs, genuine errors as well (see [Known inconsistencies](#known-inconsistencies)) |
| 1 | Every fatal error in the shared MIDI reader/writer (`midifile.c`), failure to open the input file in the shared abc parser (`parseabc.c`), most internal consistency errors; for `abc2midi` also any non-fatal parse error reported by `event_error()` |
| 2 | `midicopy` only: the output file cannot be opened |
| 255 | `abc2midi` only: a malformed custom stress-model file (`-CSM`); the code calls `exit(-1)`, which POSIX reports as 255 |

Only `abc2midi` returns a non-zero status from `main()` based on errors
accumulated during a run. The seven other programs return 0
unconditionally once `main()` reaches its end; their non-zero statuses all
come from explicit `exit()` calls part way through.

## Shared modules

These functions are linked into several programs, so their exit paths
apply to each of them.

### `parseabc.c` — abc2midi, abc2abc, yaps, abcmatch

| Status | Function | Condition |
|-------:|----------|-----------|
| **0** | `checkmalloc()` | `malloc()` failed (`Out of memory error - malloc failed!`). Note the status is 0. |
| 1 | `parsefile()` | The input abc file cannot be opened (`Failed to open file ...`). The names `stdin` and `-` read standard input and never fail here. |

### `music_utils.c` — abc2midi, abc2abc, yaps, abcmatch

| Status | Function | Condition |
|-------:|----------|-----------|
| 1 | `note_index()` | Internal error: called with a character that is not a note letter `a`–`g` / `A`–`G`. |

### `midifile.c` — abc2midi, midi2abc, midistats, mftext

`mferror()` is the single exit path for every MIDI read/write error and
always exits 1. If the program installed an `Mf_error` callback it is
called first (it only prints; see the per-program notes), otherwise the
message is printed on stdout as `MIDI read/write error : ...`.

| Status | Function | Condition / message |
|-------:|----------|---------------------|
| 1 | `readmt()` via `mferror()` | `expecting MThd` / `expecting MTrk` |
| 1 | `egetc()` via `mferror()` | `premature EOF` |
| 1 | `readtrack()` via `mferror()` | `premature EOF`, `bad time increment`, `didn't find expected continuation of a sysex`, `unexpected running status` |
| 1 | `badbyte()` via `mferror()` | `unexpected byte: 0x.. at byte N` |
| 1 | `msginit()` via `mferror()` | `malloc error!` |
| 1 | `mfread()` / `mfreadtrk()` via `mferror()` | `called without setting Mf_getc` |
| 1 | `mfwrite()` via `mferror()` | `called without setting Mf_putc` / `Mf_writetrack` |
| 1 | `mf_write_track_chunk()` via `mferror()` | `NOFTELL workaround failed to predict tracklength`, `error seeking during final stage of write` |
| 1 | `mf_write_midi_event()` via `mferror()` | `error: MIDI channel greater than 16` |
| 1 | `eputc()` via `mferror()` | `Mf_putc undefined`, `error writing` |
| 1 | `metaevent()` | A sequence-number meta-event of length zero (`Error: zero length meta seqnumber`). |
| 1 | `eputc()` | More than 500 000 bytes written to one file (`aborting because of file runaway (infinite loop)`). Only `abc2midi` writes MIDI through this module. |

## abc2midi

Sources: `store.c`, `genmidi.c`, `queues.c`, `stresspat.c` + shared
modules.

`main()` returns `error_count > 0 ? 1 : 0`, where `error_count` is
incremented by every call to `event_error()`. `event_fatal_error()` prints
its message (counting it as an error) and exits 1.

| Status | Where | Condition |
|-------:|-------|-----------|
| 0 | `main()` | Success and no error reported during parsing. |
| 0 | `event_init()` | `-h`, or no argument at all: prints usage. |
| 0 | `event_init()` | `-ver`: prints the version string. |
| 1 | `main()` | At least one `event_error()` was reported while parsing. A best-effort MIDI file is still written in that case, which is why `tests/run_test.cmake` tolerates a non-zero status as long as the `.mid` file exists. |
| 1 | `event_init()` via `event_fatal_error()` | `-n` stem-length limit outside 3..252. |
| 1 | `event_init()` via `event_fatal_error()` | `-Q` default tempo below 3 (`Enter -Q 240 not -Q 1/4=240`). |
| 1 | `setup_trackstructure()` via `event_fatal_error()` | More than 39 tracks would be needed. |
| 1 | `event_midi()` via `event_fatal_error()` | `%%MIDI drumon must occur after the first K: header` |
| 1 | `event_note()` / `event_rest()` via `event_fatal_error()` | `Internal error : no voice allocated` |
| 1 | `finishfile()` via `event_fatal_error()` | The output MIDI file cannot be opened (`File open failed`). |
| 1 | `queues.c` via `event_fatal_error()` | `Internal error - nothing to remove from queue`, `Internal error - queue head has non-zero time`, `Qcheck failed` |
| 1 | `genmidi.c` `writetrack()` | A note length denominator of zero or less after trimming (guards an infinite loop). |
| 1 | `stresspat.c` `read_custom_stress_file()` | The `-CSM` file cannot be opened. |
| 1 | `stresspat.c` `read_custom_stress_file()` | A gain/expansion pair fails to parse. |
| **255** | `stresspat.c` `read_custom_stress_file()` | The `nseg nval` line fails to parse, or `nval` is greater than 16 (`exit(-1)`). |
| **0** | shared `checkmalloc()` | Out of memory. |
| 1 | shared | Input file cannot be opened (`parsefile()`), `note_index()` internal error, any `mferror()` path (`abc2midi` installs no `Mf_error` callback, so the message goes to stdout). |

## abc2abc

Sources: `toabc.c` + shared modules.

`main()` always returns 0; `event_error()` only prints `%Error : ...`
(and only when `-e` is given), so a run with errors still exits 0.

| Status | Where | Condition |
|-------:|-------|-----------|
| 0 | `main()` | Success, with or without errors. |
| 0 | `event_init()` | `-h`, or no argument at all: prints usage. |
| 0 | `event_init()` | `-ver`: prints the version string. |
| **0** | shared `checkmalloc()` | Out of memory. |
| 1 | shared | Input file cannot be opened (`parsefile()`), `note_index()` internal error. |

## midi2abc

Sources: `midi2abc.c` + `midifile.c`.

`fatal_error()` prints its message on stderr and exits 1. The `Mf_error`
callback (`error()`) prints `Error: ...` on stderr before `mferror()`
exits.

| Status | Where | Condition |
|-------:|-------|-----------|
| 0 | `main()` | Success. |
| 0 | `process_command_line_arguments()` | `-ver`: prints the version string. |
| 0 | `process_command_line_arguments()` | `-h`, or no input file name could be found on the command line: prints usage. |
| 1 | `process_command_line_arguments()` | `-stats`: this option was removed; prints `use the new application midistats`. |
| 1 | `efopen()` via `fatal_error()` | The input file cannot be opened. |
| 1 | `checkmalloc()` via `fatal_error()` | Out of memory. |
| 1 | `guessana()` via `fatal_error()` | `Bar size exceeds static limit of 64 units!` |
| 1 | `advancechord()` via `fatal_error()` | `Error - note too short!` |
| 1 | `printtrack()` / `testtrack()` via `fatal_error()` | `Advancing by 0 in printtrack!` / `Advancing by 0 in testtrack!` |
| 1 | `printchordlist()` via `fatal_error()` | `Loopback problem!`, `chordhead == NULL and chordtail != NULL` and the other chord-list consistency messages. |
| 1 | `readsig()` via `fatal_error()` | `-m` time signature malformed: `Expecting / in time signature`, `n/m is not a valid time signature`, `Bad key signature, divisor must be a power of 2`. |
| 1 | `main()` via `fatal_error()` | `MIDI file has no notes!` |
| 1 | shared `mferror()` | Any MIDI read error. |

## midistats

Sources: `midistats.c` + `midifile.c`.

`fatal_error()` prints its message on stderr and exits 1. The `Mf_error`
callback (`stats_error()`) prints the message on stderr and then dumps
the statistics gathered so far (`stats_finish()`) before `mferror()`
exits.

| Status | Where | Condition |
|-------:|-------|-----------|
| 0 | `main()` | Success. |
| 0 | `process_command_line_arguments()` | `-ver`: prints the version string. |
| 0 | `process_command_line_arguments()` | `-h`, or no input file name could be found: prints usage. |
| 1 | `efopen()` via `fatal_error()` | The input file cannot be opened. |
| 1 | `checkmalloc()` via `fatal_error()` | Out of memory. |
| 1 | `stats_header()` | Division (PPQN) below 12. |
| 1 | `stats_header()` | More than 40 tracks. |
| 1 | `stats_noteon()` | Pulse position within the beat of 2047 or more. |
| 1 | `load_finish()` | Pulse position within the beat above 2047. |
| 1 | `record_noteon()` | More than 49 999 note events (`ran out of space in midievents structure`). |
| 1 | `drumanalysis()` | A percussion pitch outside 0..100. |
| 1 | shared `mferror()` | Any MIDI read error (statistics printed first). |

## mftext

Sources: `mftext.c`, `crack.c` + `midifile.c`.

The `Mf_error` callback (`error()`) prints `Error: ...` on stderr before
`mferror()` exits.

| Status | Where | Condition |
|-------:|-------|-----------|
| 0 | `main()` | Success (`main()` ends with an explicit `exit(0)`). |
| **0** | `efopen()` | The input file cannot be opened (message on stderr). Note the status is 0. |
| **0** | `crack()` | Unknown command-line flag (`no such flag`). Note the status is 0. |
| 1 | shared `mferror()` | Any MIDI read error. |

## yaps

Sources: `yapstree.c`, `drawtune.c`, `position.c` + shared modules.

`main()` always returns 0; `event_error()` only prints
`Error in line N : ...`, so a run with errors still exits 0.

| Status | Where | Condition |
|-------:|-------|-----------|
| 0 | `main()` | Success, with or without errors. |
| 0 | `event_init()` | `-ver`: prints the version string. |
| 0 | `event_init()` | `-h`, or no argument at all: prints usage. |
| 1 | `event_init()` | The `-e` match string is 255 characters or longer. |
| 1 | `event_init()` | The `-o` output file name, or the output name implied by the input name, exceeds `MAX_OUTPUTROOT`. |
| 1 | `event_init()` | The input file name already ends in `.ps` (`argument must be abc file, not PostScript file`). |
| 1 | `closebeam()` | `closebeam: internal data error` |
| 1 | `drawtune.c` `handlegracebeam()` | `Internal beaming error` |
| 1 | `position.c` | Internal error, unknown layout phase. |
| **0** | `addfeature()` / `addnumberfeature()` | No current voice, or `expecting NULL at list end!` (internal). Note the status is 0. |
| **0** | `count_dots()` | `Problem with n / m` while working out a note length. Note the status is 0. |
| **0** | `newvoice()` | `Trying to set up voice with no key signature`. Note the status is 0. |
| **0** | `drawtune.c` `drawbeam()` | `Internal error: beam does not start with NOTE`. Note the status is 0. |
| **0** | `drawtune.c` `beamline()` / `measureline()` | `Missing NOTE!!!!` / `Missing REST!!!!` (a NULL item in the feature list). Note the status is 0. |
| **0** | `drawtune.c` `open_output_file()` | The PostScript output file cannot be opened (`Could not open file!!`). Note the status is 0. |
| **0** | shared `checkmalloc()` | Out of memory. |
| 1 | shared | Input file cannot be opened (`parsefile()`), `note_index()` internal error. |

## midicopy

Sources: `midicopy.c` only; self-contained, with its own `mferror()`
that prints `Error: ...` on stderr and exits 1.

| Status | Where | Condition |
|-------:|-------|-----------|
| 0 | `main()` | Success. |
| 0 | `main()` | `-ver`: prints the version string. |
| **1** | `main()` | `-h`, or fewer than two positional arguments: prints usage. This is the only program whose usage output exits non-zero. |
| 1 | `main()` | The input file (second-to-last argument) cannot be opened. |
| **2** | `main()` | The output file (last argument) cannot be opened. This is the only `exit(2)` in the package. |
| 1 | `main()` | More than 149 tracks in the input. |
| 1 | `append_to_string()` | The per-track data buffer overflowed. |
| 1 | local `mferror()` | `expecting MThd` / `expecting MTrk`, `premature EOF`, `unexpected byte: 0x..`, `didn't find expected continuation of a sysex`, `unexpected running status`, `malloc error!`, `error: MIDI channel greater than 16`. |

## abcmatch

Sources: `abcmatch.c`, `matchsup.c` + shared modules.

| Status | Where | Condition |
|-------:|-------|-----------|
| 0 | `main()` | Success. |
| 0 | `event_init()` | `-ver`, `-h`, or no argument at all. |
| **0** | `analyze_abc_file()` / `main()` | The abc file cannot be opened (both the histogram path and the matching path). Note the status is 0. In the matching path the template file (`match.abc` unless `-tp` is given) is parsed first, so a missing template exits 1 via the shared `parsefile()` before the input file is even tried. |
| **0** | `event_init()` | `-lev`, `-tp`, `-br` or `-fixed` given without their argument, or a `-tp` file name beginning with `-`. Note the status is 0. |
| **0** | `main()` | The `X:` reference number requested with `-tp` is not found in the template file. Note the status is 0. |
| **0** | `make_note_representation()` | A tune has more notes or more bar lines than the per-tune buffers hold (`ran out of space for midipitch` / `for barlineptr`). Note the status is 0. |
| 1 | `matchsup.c` via `event_fatal_error()` | `Internal error : no voice allocated` |
| **0** | shared `checkmalloc()` | Out of memory. |
| 1 | shared | The template file (`-tp`, default `match.abc`) cannot be opened (`parsefile()`), `note_index()` internal error. |

## Known inconsistencies

These are documented rather than fixed so that scripts written against
the current behaviour are not surprised. Anyone changing one of them
should update the corresponding entry above.

1. **Errors that exit 0.** A script that tests `$?` will not see:
   - out-of-memory in `checkmalloc()` (abc2midi, abc2abc, yaps, abcmatch);
   - every abcmatch error, including `cannot open file`;
   - mftext failing to open its input, or rejecting a flag;
   - yaps failing to open its PostScript output, and all of its internal
     `Missing NOTE/REST`, no-voice and no-key-signature errors.

   The `checkmalloc()` case is the most consequential since it is shared.

2. **`exit(-1)`** in `stresspat.c` yields 255, unlike every other abc2midi
   error (1).

3. **`midicopy -h` exits 1** while the seven other programs exit 0 for
   usage; midicopy is also the only user of status 2.

4. **Only abc2midi propagates non-fatal parse errors** into its exit
   status. abc2abc, yaps and abcmatch print the error and still exit 0.

5. In `midifile.c` `readtrack()` the `premature EOF` branch calls
   `mferror()` and then `return(0)`; the return is dead code since
   `mferror()` never returns.

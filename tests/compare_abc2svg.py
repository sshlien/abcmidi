#!/usr/bin/env python3
"""Compare the notes that abc2midi and abc2svg play for ABC files.

abc2svg is the reference followed for playback questions such as
microtones (GitHub issue #56).  This script renders every tune of the
given ABC files with both programs and compares, tune by tune, the
pitches of the notes in the order in which they start.  It only reports:
it exits 0 whatever the differences, and 2 if a program cannot be run.

Both programs read a copy of each file with two directives added after
each X: line:
  %%MIDI gchordoff      the two programs generate the accompaniment of
                        guitar chords differently
  %%MIDI chordattack 0  abc2midi otherwise starts the notes of a chord
                        10 ticks apart

Each tune becomes two sequences of onsets, one for the drums (MIDI
channel 10) and one for the other notes, each onset holding the sorted
pitches of the notes that start together, abc2midi's pitch bend included
(its default range of +/- 2 semitones).  Onsets rather than times keep
the comparison independent of the tempo.  Pitches within 0.015 semitone
are equal, abc2svg printing them with 2 decimals.

Usage:
  tests/compare_abc2svg.py --abc2midi build/default/abc2midi \\
      --mftext build/default/mftext --abc2svg ABC2SVG_DIR samples/*.abc

ABC2SVG_DIR is a built abc2svg tree (NOMIN=1 ninja) with abcnode,
tonotes.js and snd-1.js; node must be in PATH.  When GITHUB_STEP_SUMMARY
is set, the report is also appended to it.
"""

import argparse
import difflib
import math
import os
import re
import subprocess
import sys
import tempfile

X_LINE = re.compile(r"X:\s*(\d+)")
TUNE_HEADER = re.compile(r"# -+ tune (\d+)\.")
VOICE_CHANNEL = re.compile(r"v:(\S+)\s+MIDI channel (\d+)")
ABC2SVG_MESSAGE = re.compile(r":(\d+):\d+ (Error|Warning)")
PITCHBEND = re.compile(r"Pitchbend, chan=(\d+) msb=(\d+) lsb=(\d+)")
NOTE_ON = re.compile(r"Time=(\d+)\s+Note on, chan=(\d+) pitch=(\d+) vol=(\d+)")
TOLERANCE = 0.015


def prepare(source, copy):
    """Write copy of source with the directives added after each X: line.

    Return the tune numbers and the line where each tune starts in copy."""
    tunes = []
    lines = []
    with open(source, encoding="latin-1") as f:
        for line in f:
            if not line.endswith("\n"):
                line += "\n"
            lines.append(line)
            m = X_LINE.match(line)
            if m:
                tunes.append((m.group(1), len(lines)))
                lines += ["%%MIDI gchordoff\n", "%%MIDI chordattack 0\n"]
    with open(copy, "w", encoding="latin-1") as f:
        f.writelines(lines)
    return tunes


def onsets(notes, drums):
    """Group the (time, pitch, drum) triples whose drum is drums into a
    time-ordered list of (time, sorted pitches)."""
    groups = {}
    for time, pitch, drum in notes:
        if drum == drums:
            groups.setdefault(time, []).append(pitch)
    return [(t, tuple(sorted(groups[t]))) for t in sorted(groups)]


def streams(notes):
    """Split notes into the (melody, drums) onset lists."""
    return onsets(notes, False), onsets(notes, True)


def run(command):
    return subprocess.run(command, capture_output=True, text=True,
                          errors="replace")


def abc2midi_tune(args, abc, tune, workdir):
    """Return the (melody, drums) onsets of a tune and abc2midi's error
    and warning counts, or None for the onsets if abc2midi wrote no MIDI
    file."""
    mid = os.path.join(workdir, "tune%s.mid" % tune)
    result = run([args.abc2midi, abc, tune, "-o", mid])
    messages = (result.stdout.count("Error in"),
                result.stdout.count("Warning in"))
    if not os.path.exists(mid):
        return None, messages
    notes = []
    bend = {}
    for line in run([args.mftext, mid]).stdout.splitlines():
        if line.startswith("Track start"):
            bend = {}
        m = PITCHBEND.search(line)
        if m:
            bend[m.group(1)] = int(m.group(2)) * 128 + int(m.group(3))
            continue
        m = NOTE_ON.search(line)
        if m and int(m.group(4)) > 0:
            pitch = int(m.group(3)) + (bend.get(m.group(2), 8192) - 8192) / 4096
            notes.append((int(m.group(1)), pitch, m.group(2) == "10"))
    return streams(notes), messages


def abc2svg_file(args, abc, tunes):
    """Return {tune: onsets} and {tune: (errors, warnings)} for a file."""
    # abcnode is a shell script without a #! line
    result = run(["sh", os.path.join(args.abc2svg, "abcnode"), "tonotes.js",
                  abc])
    if result.returncode != 0 and not result.stdout:
        raise RuntimeError("abc2svg failed on %s: %s" % (abc, result.stderr))
    notes = {}
    tune = None
    channel = {}                # voice -> MIDI channel
    for line in result.stdout.splitlines():
        m = TUNE_HEADER.match(line)
        if m:
            tune = m.group(1)
            notes.setdefault(tune, [])
            channel = {}
            continue
        m = VOICE_CHANNEL.search(line)
        if m:
            channel[m.group(1)] = m.group(2)
            continue
        fields = line.split()
        if tune is None or len(fields) < 5:
            continue
        try:
            # time instrument pitch [note name] duration voice
            notes[tune].append((int(fields[0]), float(fields[2]),
                                channel.get(fields[-1]) == "10"))
        except ValueError:
            pass                # comment or other MIDI directive
    messages = {x: [0, 0] for x, _ in tunes}
    for line in result.stderr.splitlines():
        m = ABC2SVG_MESSAGE.search(line)
        if m:
            owner = [x for x, start in tunes if start <= int(m.group(1))]
            if owner:
                messages[owner[-1]][m.group(2) == "Warning"] += 1
    return {x: streams(n) for x, n in notes.items()}, messages


def key(group):
    """Quantize a group of pitches for difflib, to the cent."""
    return tuple(math.floor(p * 100 + 0.5) for p in group)


def same(a, b):
    return len(a) == len(b) and all(abs(x - y) <= TOLERANCE
                                     for x, y in zip(a, b))


def show(group):
    return "+".join("%g" % round(p, 2) for p in group) if group else "-"


def compare(midi, svg):
    """Describe how the pitches of two onset lists differ."""
    a = [g for _, g in midi]
    b = [g for _, g in svg]
    matcher = difflib.SequenceMatcher(None, [key(g) for g in a],
                                      [key(g) for g in b], autojunk=False)
    count = 0
    first = None
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == "equal":
            continue
        for k in range(max(i2 - i1, j2 - j1)):
            ga = a[i1 + k] if i1 + k < i2 else ()
            gb = b[j1 + k] if j1 + k < j2 else ()
            if same(ga, gb):
                continue        # quantized apart, within tolerance
            count += 1
            if first is None:
                when = (svg[j1 + k][0] if j1 + k < j2
                        else svg[j1][0] if j1 < len(svg) else None)
                where = " at %.2f s" % (when / 100) if when is not None else ""
                first = "onset %d%s: abc2midi %s, abc2svg %s" % (
                    i1 + k + 1, where, show(ga), show(gb))
    if count == 0:
        return "same (%d onsets)" % len(a) if a else ""
    return "**%d differ%s** (abc2midi %d onsets, abc2svg %d); first %s" % (
        count, "s" if count == 1 else "", len(a), len(b), first)


def messages_text(name, counts):
    errors, warnings = counts
    parts = []
    if errors:
        parts.append("%d error%s" % (errors, "s" if errors > 1 else ""))
    if warnings:
        parts.append("%d warning%s" % (warnings, "s" if warnings > 1 else ""))
    return "%s: %s" % (name, ", ".join(parts)) if parts else ""


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--abc2midi", required=True)
    parser.add_argument("--mftext", required=True)
    parser.add_argument("--abc2svg", required=True,
                        help="built abc2svg directory")
    parser.add_argument("abc", nargs="+", help="ABC files")
    args = parser.parse_args()

    rows = []
    differing = 0
    total = 0
    with tempfile.TemporaryDirectory() as workdir:
        for source in args.abc:
            name = os.path.basename(source)
            copy = os.path.join(workdir, name)
            tunes = prepare(source, copy)
            try:
                svg, svg_messages = abc2svg_file(args, copy, tunes)
            except (OSError, RuntimeError) as e:
                print("cannot run abc2svg: %s" % e, file=sys.stderr)
                return 2
            for tune, _ in tunes:
                try:
                    midi, midi_messages = abc2midi_tune(args, copy, tune,
                                                        workdir)
                except OSError as e:
                    print("cannot run abc2midi: %s" % e, file=sys.stderr)
                    return 2
                total += 1
                if midi is None:
                    melody, drums = "abc2midi wrote no MIDI file", ""
                elif tune not in svg:
                    melody, drums = "abc2svg played no note", ""
                else:
                    melody = compare(midi[0], svg[tune][0])
                    drums = compare(midi[1], svg[tune][1])
                if "same" not in melody or (drums and "same" not in drums):
                    differing += 1
                notes = "; ".join(t for t in (
                    messages_text("abc2midi", midi_messages),
                    messages_text("abc2svg", svg_messages.get(tune, (0, 0))))
                    if t)
                rows.append((name, tune, melody, drums, notes))

    report = ["## abc2midi compared with abc2svg", "",
              "%d of %d tunes differ." % (differing, total), "",
              "| file | X: | notes | drums | messages |",
              "|---|---|---|---|---|"]
    report += ["| %s | %s | %s | %s | %s |" % row for row in rows]
    text = "\n".join(report) + "\n"
    print(text)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as f:
            f.write(text)
        print("::notice title=abc2svg comparison::%d of %d tunes differ, "
              "see the job summary" % (differing, total))
    return 0


if __name__ == "__main__":
    sys.exit(main())

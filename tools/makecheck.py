#!/usr/bin/env python3
"""run the same makefile with ours and with GNU make, and compare.

the third tool checked this way, after the assembler against nasm and the
linker against ld, and the one where the comparison is least obviously
needed and most useful. an assembler produces bytes nobody can read; make
produces *commands*, which are perfectly readable, so it looks like a
thing you could check by eye.

it is not, because what make actually produces is a decision: whether to
run a command at all. that decision comes out of timestamps, and a
timestamp bug does not look like a bug. it looks like a build that
works, until the day it silently does not rebuild something and you
spend an afternoon debugging a binary that was never recompiled.

so the check is not "did the right commands run" but "did **exactly** the
same commands run, in the same order, with the same output and the same
exit status, after the same sequence of edits". the edits are the point:
a make that gets a cold build right and an incremental one wrong is the
normal kind of broken.

a fixture is a directory holding a `Makefile`, a `steps` script, and
whatever else the build needs. everything but `steps` is copied into two
temporary directories, one for GNU make, one for ours, and the steps
are replayed in both.

    write <file>...     create them, with a mtime one tick later than the last
    touch <file>...     bump their mtimes, same way
    rm <file>...        delete them
    run [args...]       run make, and compare everything it said and did

several files named in one step share a tick, which is not a shortcut:
"the prerequisite is exactly as old as the target" is its own case, and
make counts it as up to date. a build that thought otherwise would
rebuild the world every time a fast machine wrote two files inside the
same nanosecond, and nothing else in a fixture ever produces two equal
timestamps.

timestamps are *set* rather than taken from the clock, which is why
there is no `sleep` in any of this. both directories get identical
mtimes to the nanosecond, so a disagreement is a disagreement about the
rules rather than about how fast the disk was.

that includes the files the build itself writes. after every run, each
directory's newly written files are found, put in the order they were
written, and given the next ticks, so the two directories go into the
next step agreeing about which file is older than which, without either
of them having been asked what time it is.

usage:  tools/makecheck.py tests/make/<fixture> [...]
"""
import subprocess, sys, os, shutil, tempfile, difflib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OURS = os.path.join(ROOT, "bin", "tests", "make")

# far enough in the past that nothing in the fixture is newer than it by
# accident, and a round number so a printed mtime is recognisable
BASE = 1000000000


def state(d):
    """every file in the directory and what is in it. a make that runs
    the right commands and leaves the wrong files behind has still got
    it wrong, and only looking at the result would show it."""
    out = {}
    for dirpath, _, names in os.walk(d):
        for n in names:
            p = os.path.join(dirpath, n)
            out[os.path.relpath(p, d)] = open(p, "rb").read()
    return out


def restamp(d, tick):
    """give everything the build just wrote a tick of its own.

    a recipe writes files with whatever time it is now, and the two
    directories are built seconds apart, so leaving those alone would
    have the next step comparing 2026 against 2026-and-a-bit. so the new
    files are found, anything stamped later than the last tick handed
    out, put back in the order they were written, and renumbered from
    there. the order within a run is kept, which is the part the next
    run's decisions actually turn on."""
    fresh = []
    for dirpath, _, names in os.walk(d):
        for n in names:
            p = os.path.join(dirpath, n)
            when = os.stat(p).st_mtime
            if when > BASE + tick:
                fresh.append((when, os.path.relpath(p, d)))
    fresh.sort()
    for i, (_, rel) in enumerate(fresh):
        os.utime(os.path.join(d, rel), (BASE + tick + 1 + i,
                                        BASE + tick + 1 + i))
    return len(fresh)


def run_make(program, cwd, args):
    # everything gnu make leaves in the environment for a sub-make, taken
    # back out. run from inside `make test` it would otherwise see
    # MAKELEVEL, decide it is a recursive invocation, and announce which
    # directory it is entering, so the check would pass when run by
    # hand and fail when run by the suite, which is the worst way round
    env = dict(os.environ, LC_ALL="C")
    for leaked in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL",
                   "MAKE_TERMOUT", "MAKE_TERMERR", "MAKEOVERRIDES"):
        env.pop(leaked, None)
    r = subprocess.run([program] + args, cwd=cwd, env=env,
                       capture_output=True, text=True)
    return r.stdout + r.stderr, r.returncode


def check(fixture):
    steps = os.path.join(fixture, "steps")
    if not os.path.exists(steps):
        print("  %s has no steps file" % fixture)
        return 1

    dirs = {}
    for who in ("theirs", "ours"):
        d = tempfile.mkdtemp()
        for name in os.listdir(fixture):
            if name == "steps":
                continue
            shutil.copy(os.path.join(fixture, name), os.path.join(d, name))
            os.utime(os.path.join(d, name), (BASE, BASE))
        dirs[who] = d

    tick = 0
    bad = 0
    for line in open(steps):
        line = line.split("#")[0].strip()
        if not line:
            continue
        what, rest = (line.split(None, 1) + [""])[:2]

        if what in ("write", "touch", "rm"):
            tick += 1
            for d in dirs.values():
                for name in rest.split():
                    p = os.path.join(d, name)
                    if what == "rm":
                        if os.path.exists(p):
                            os.unlink(p)
                        continue
                    if what == "write" or not os.path.exists(p):
                        open(p, "a").close()
                    os.utime(p, (BASE + tick, BASE + tick))
            continue

        if what != "run":
            print("  %s: step i do not understand: %s" % (fixture, line))
            return 1

        args = rest.split()
        theirs, their_rc = run_make("make", dirs["theirs"], args)
        ours, our_rc = run_make(OURS, dirs["ours"], args)

        if theirs != ours:
            print("  %s: `make %s` said something else" % (fixture, rest))
            for d in difflib.unified_diff(theirs.splitlines(),
                                          ours.splitlines(),
                                          "gnu make", "ours", lineterm=""):
                print("      %s" % d)
            bad += 1
        elif their_rc != our_rc:
            print("  %s: `make %s` exited %d, gnu make exited %d"
                  % (fixture, rest, our_rc, their_rc))
            bad += 1
        else:
            left, right = state(dirs["theirs"]), state(dirs["ours"])
            if left != right:
                only = sorted(set(left) ^ set(right))
                differ = sorted(f for f in set(left) & set(right)
                                if left[f] != right[f])
                print("  %s: `make %s` left a different directory: %s"
                      % (fixture, rest, ", ".join(only + differ)))
                bad += 1

        wrote = [restamp(d, tick) for d in dirs.values()]
        if wrote[0] != wrote[1] and not bad:
            print("  %s: `make %s` wrote %d files, gnu make wrote %d"
                  % (fixture, rest, wrote[1], wrote[0]))
            bad += 1
        tick += max(wrote)

        if bad:
            break

    for d in dirs.values():
        shutil.rmtree(d, ignore_errors=True)
    return bad


def main():
    if len(sys.argv) < 2:
        print(__doc__.strip().splitlines()[-1])
        return 2
    bad = 0
    for fixture in sys.argv[1:]:
        bad += check(fixture)
    if not bad:
        print("  %d fixtures agree with gnu make" % (len(sys.argv) - 1))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())

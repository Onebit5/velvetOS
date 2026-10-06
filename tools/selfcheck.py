#!/usr/bin/env python3
"""build the kernel with this project's own tools, and hold it against
the one the machine's tools built.

`make toolchain` has done a small version of this since 0.3.16: our make
reads a makefile, our assembler turns two sources into objects, our
linker links them, and the result is compared against nasm and ld's.
this is that claim made about the thing the whole project is for,
ninety objects, four assembly files, a script with PHDRS and symbol
assignments in it, and a kernel at the end.

what is compared is the *loaded image*: every byte at the address it
will be at when the machine is running. not the file, which differs in
ways nothing executes, ours carries no debug sections, and section
order and padding are decisions rather than results.

and the answer is not "close enough". it is **identical, except for the
clock**: `version.h` compiles `__TIME__` into the kernel, so two builds
started in different seconds differ by the six bytes that says so, and
nothing else. this checks that every difference is inside one of those
timestamps, which is a stricter test than a byte count, since it means
naming what each difference is rather than allowing a budget of them.

usage:  tools/selfcheck.py [ours.elf] [theirs.elf]
"""
import re, subprocess, sys, os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
CLOCK = re.compile(rb"^\d\d:\d\d:\d\d$")


def loaded(path):
    """address -> byte, for everything a PT_LOAD puts in memory"""
    r = subprocess.run(["readelf", "-lW", path], capture_output=True, text=True)
    data = open(path, "rb").read()
    out = {}
    for line in r.stdout.splitlines():
        f = line.split()
        if len(f) < 6 or f[0] != "LOAD":
            continue
        off, vaddr, filesz = int(f[1], 16), int(f[2], 16), int(f[4], 16)
        for i in range(filesz):
            out[vaddr + i] = data[off + i]
    return out


def string_around(image, at):
    """the nul-terminated string the byte at `at` is part of, if it is
    part of one. this is what turns "six bytes differ" into "the build
    time differs", which is the difference between a check and a
    tolerance."""
    lo = at
    while lo - 1 in image and image[lo - 1] not in (0,):
        lo -= 1
    hi = at
    while hi in image and image[hi] not in (0,):
        hi += 1
    return bytes(image[i] for i in range(lo, hi))


def main():
    ours = sys.argv[1] if len(sys.argv) > 1 else \
        os.path.join(ROOT, "bin", "toolchain", "velvetos")
    theirs = sys.argv[2] if len(sys.argv) > 2 else \
        os.path.join(ROOT, "bin", "velvetos")

    for path in (ours, theirs):
        if not os.path.exists(path):
            print("  %s is not there (run make first)" % path)
            return 1

    a, b = loaded(ours), loaded(theirs)

    only_ours = set(a) - set(b)
    only_theirs = set(b) - set(a)
    if only_ours or only_theirs:
        print("  ours loads %d bytes ld's kernel does not, and misses %d"
              % (len(only_ours), len(only_theirs)))
        if only_ours:
            print("      first at 0x%x" % min(only_ours))
        if only_theirs:
            print("      first at 0x%x" % min(only_theirs))
        return 1

    differ = sorted(at for at in a if a[at] != b[at])
    unexplained = []
    clocks = set()
    for at in differ:
        mine, yours = string_around(a, at), string_around(b, at)
        if CLOCK.match(mine) and CLOCK.match(yours):
            clocks.add((mine, yours))
        else:
            unexplained.append(at)

    if unexplained:
        at = unexplained[0]
        print("  %d bytes differ for a reason that is not the clock, "
              "first at 0x%x" % (len(unexplained), at))
        print("      ld:   %s" % " ".join("%02x" % b.get(at + i, 0)
                                          for i in range(8)))
        print("      ours: %s" % " ".join("%02x" % a.get(at + i, 0)
                                          for i in range(8)))
        return 1

    if clocks:
        said = ", ".join("%s against %s" % (m.decode(), y.decode())
                         for m, y in sorted(clocks))
        print("  the kernel    ok      %d bytes identical, %d that are the "
              "build clock (%s)" % (len(a) - len(differ), len(differ), said))
    else:
        print("  the kernel    ok      %d bytes, identical to ld's, "
              "to the byte" % len(a))
    return 0


if __name__ == "__main__":
    sys.exit(main())

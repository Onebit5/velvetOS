#!/usr/bin/env python3
"""link the same objects with ours and with ld, and compare.

the same principle the assembler is checked by, and for the same reason:
a linker produces an ELF nobody can read, and a wrong relocation is a
program that loads perfectly and jumps four bytes past where it meant
to. there is no eyeballing it.

so every case is linked both ways. what is compared is the *loadable
content*, the bytes of each allocated section, at the addresses they
were placed, rather than the whole file, because ld and this differ in
things that have no effect on what runs: section ordering, alignment
padding, the section-header string table, and a great deal of optional
apparatus neither the processor nor a bootloader ever looks at.

comparing the file byte for byte would be comparing decisions rather
than results, and would fail on differences that mean nothing.

usage:  tools/linkcheck.py <script.ld> <a.asm|a.o> [b.asm|b.o ...]

sources are assembled with nasm rather than with ours, deliberately: this
is a check of the linker, and giving it objects our own assembler made
would let one wrong answer cancel out another.
"""
import subprocess, sys, os, tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OURS = os.path.join(ROOT, "bin", "tests", "link")


def segments_of(path):
    """every PT_LOAD, as (vaddr, bytes).

    segments rather than sections, and deliberately: a segment is what
    the machine is actually told to load, and it is the only part of an
    ELF whose contents have to agree for two programs to be the same
    program. sections are how a *linker* thinks, their names, order and
    padding are decisions, and comparing decisions would fail on
    differences that change nothing about what runs."""
    r = subprocess.run(["readelf", "-l", "-W", path],
                       capture_output=True, text=True)
    data = open(path, "rb").read()
    out = []
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) < 6 or parts[0] != "LOAD":
            continue
        off   = int(parts[1], 16)
        vaddr = int(parts[2], 16)
        filesz = int(parts[4], 16)
        if filesz == 0:
            continue
        out.append((vaddr, data[off:off + filesz]))
    return sorted(out)


def loads_of(path):
    """every PT_LOAD as (vaddr, memsz, flags), including the part of it
    that has no bytes in the file. `.bss` is exactly that part, and a
    linker that reserved the wrong amount of it, or none, would look
    perfect to a comparison that only read what was written down."""
    r = subprocess.run(["readelf", "-l", "-W", path],
                       capture_output=True, text=True)
    out = []
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) < 8 or parts[0] != "LOAD":
            continue
        vaddr = int(parts[2], 16)
        memsz = int(parts[5], 16)
        flags = "".join(parts[6:-1])
        if memsz:
            out.append((vaddr, memsz, flags))
    return out


def permissions(path):
    """what each address may be *done to*, as merged runs.

    which addresses share a segment is a decision. what a given address
    ends up readable, writable or executable is not, it is the whole
    reason a kernel script page-aligns its sections, and getting it wrong
    produces a machine that runs perfectly and can be rewritten by
    anything that manages to jump into it."""
    segs = loads_of(path)
    edges = sorted({e for v, m, _ in segs for e in (v, v + m)})
    runs = []
    for i in range(len(edges) - 1):
        lo, hi = edges[i], edges[i + 1]
        flags = set()
        for v, m, f in segs:
            if v <= lo and hi <= v + m:
                flags |= set(f.replace(" ", ""))
        if not flags:
            continue
        f = "".join(sorted(flags))
        if runs and runs[-1][1] == lo and runs[-1][2] == f:
            runs[-1] = (runs[-1][0], hi, f)
        else:
            runs.append((lo, hi, f))
    return runs


def entry_of(path):
    r = subprocess.run(["readelf", "-h", path], capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if "Entry point" in line:
            return line.split()[-1]
    return None


def main():
    if len(sys.argv) < 3:
        print("usage: linkcheck.py <script.ld> <a.o> [b.o ...]")
        return 2
    script, inputs = sys.argv[1], sys.argv[2:]

    work = tempfile.mkdtemp()
    objs = []
    for path in inputs:
        if not path.endswith(".asm"):
            objs.append(path)
            continue
        o = os.path.join(work, os.path.basename(path)[:-4] + ".o")
        r = subprocess.run(["nasm", "-f", "elf64", path, "-o", o],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print("  nasm would not assemble %s: %s"
                  % (path, r.stderr.strip()[:120]))
            return 1
        objs.append(o)

    theirs = tempfile.NamedTemporaryFile(suffix=".elf", delete=False).name
    mine   = tempfile.NamedTemporaryFile(suffix=".elf", delete=False).name

    r = subprocess.run(["ld", "-T", script, "-o", theirs] + objs,
                       capture_output=True, text=True)
    if r.returncode != 0:
        print("  ld would not link it: %s" % r.stderr.strip()[:120])
        return 1

    r = subprocess.run([OURS, "-T", script, "-o", mine] + objs,
                       capture_output=True, text=True)
    if r.returncode != 0:
        print("  ours would not: %s" % r.stderr.strip()[:160])
        return 1

    bad = 0

    # what ends up at each address, rather than how the segments were
    # divided up. how many PT_LOADs a linker uses, and which sections it
    # groups into each, are decisions; what lands at a given address and
    # what may be done to it there are results.
    #
    # so both sides are flattened into address -> byte and compared
    # there: the thing that has to agree for two programs to be the
    # same program.
    def flatten(segs):
        m = {}
        for vaddr, data in segs:
            for i, byte in enumerate(data):
                m[vaddr + i] = byte
        return m

    mine_map, theirs_map = flatten(segments_of(mine)), flatten(segments_of(theirs))

    only_theirs = sorted(set(theirs_map) - set(mine_map))
    only_mine   = sorted(set(mine_map) - set(theirs_map))
    if only_theirs:
        print("  ld loads %d bytes we do not, from 0x%x"
              % (len(only_theirs), only_theirs[0]))
        bad += 1
    if only_mine:
        print("  we load %d bytes ld does not, from 0x%x"
              % (len(only_mine), only_mine[0]))
        bad += 1

    shared = sorted(set(mine_map) & set(theirs_map))
    wrong = [a for a in shared if mine_map[a] != theirs_map[a]]
    if wrong:
        at = wrong[0]
        print("  %d bytes differ, first at 0x%x" % (len(wrong), at))
        print("      ld:   %s" % " ".join("%02x" % theirs_map.get(at + i, 0)
                                          for i in range(8)))
        print("      ours: %s" % " ".join("%02x" % mine_map.get(at + i, 0)
                                          for i in range(8)))
        bad += 1
    elif not bad:
        print("  loaded image  ok      %d bytes at %d addresses"
              % (len(shared), len(shared)))

    ours_perms, theirs_perms = permissions(mine), permissions(theirs)
    if ours_perms != theirs_perms:
        print("  the memory image differs:")
        for a, b in zip(ours_perms + [None] * len(theirs_perms),
                        theirs_perms + [None] * len(ours_perms)):
            if a == b or (a is None and b is None):
                continue
            def show(r):
                return "none" if r is None else \
                    "0x%x..0x%x %s" % (r[0], r[1], r[2])
            print("      ld:   %s" % show(b))
            print("      ours: %s" % show(a))
        bad += 1

    if entry_of(theirs) != entry_of(mine):
        print("  entry point  ours %s, ld says %s"
              % (entry_of(mine), entry_of(theirs)))
        bad += 1

    os.unlink(theirs)
    os.unlink(mine)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())

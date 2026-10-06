#!/usr/bin/env python3
"""assemble a file with ours and with nasm, and compare the bytes.

the only opinion worth having about an encoding is somebody else's. an
assembler that emits *plausible* bytes cannot be eyeballed, unlike a
diff or a parser, wrong output looks exactly like right output until
something executes it, and then the failure is a triple fault with no
explanation.

so nothing here is ever checked by reading it. every instruction the
assembler supports is assembled both ways and compared byte for byte,
and the first divergence is reported with its offset.

nasm also *chooses* between valid encodings, `mov rax,1` becomes the
five-byte `mov eax,1` because the upper half is zeroed anyway, so
matching it means matching those choices too, not merely emitting
something legal. that is the part this catches and reading never would.

usage:  tools/asmcheck.py <file.asm> [...]
"""
import subprocess, sys, os, tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OURS = os.path.join(ROOT, "bin", "tests", "asm")


def assemble_nasm(path):
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
        out = f.name
    r = subprocess.run(["nasm", "-f", "bin", path, "-o", out],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return None, r.stderr.strip()
    data = open(out, "rb").read()
    os.unlink(out)
    return data, None


def assemble_ours(path):
    r = subprocess.run([OURS, path], capture_output=True)
    if r.returncode != 0:
        return None, r.stderr.decode(errors="replace").strip()
    return r.stdout, None


def elf_objects(path):
    """the same source assembled to an *object* both ways.

    the flat comparison below is about the bytes, and an object is more
    than its bytes: it carries the symbol table and the relocations,
    which are what a linker reads and what a byte comparison cannot see.
    two bugs lived here for a version because of that, `global` was
    accepted and ignored, so exported names came out local, and every
    relocation went into .rela.text whatever section it patched. neither
    changes a byte of .text, and the manual check counted relocations
    and got the same *total*."""
    theirs = tempfile.NamedTemporaryFile(suffix=".o", delete=False).name
    mine = tempfile.NamedTemporaryFile(suffix=".o", delete=False).name
    r = subprocess.run(["nasm", "-f", "elf64", path, "-o", theirs],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return None, None
    r = subprocess.run([OURS, "-o", mine, path], capture_output=True, text=True)
    if r.returncode != 0:
        return None, None
    return theirs, mine


def relocations(path):
    """every relocation, grouped by the section it patches. the grouping
    is the point: which section a relocation belongs to is exactly what
    a count of them cannot tell you."""
    r = subprocess.run(["readelf", "-rW", path], capture_output=True, text=True)
    out, section = {}, None
    for line in r.stdout.splitlines():
        if line.startswith("Relocation section"):
            section = line.split("'")[1]
            out[section] = []
        elif section and line[:1].isdigit():
            f = line.split()
            if len(f) >= 5:
                # offset, kind, symbol and addend, not the symbol's
                # index, which is a numbering decision rather than a fact
                out[section].append((f[0], f[2], " ".join(f[4:])))
    return out


def globals_of(path):
    """every name this object offers the linker, and where it is.

    the section is named rather than numbered. which index a section
    gets is a decision, nasm calls .rodata 2 and this calls it 6, and
    both are right, while *which section the symbol is in* is a fact,
    and confusing the two would fail on a difference that changes
    nothing."""
    r = subprocess.run(["readelf", "-SW", path], capture_output=True, text=True)
    names = {}
    for line in r.stdout.splitlines():
        if "] " in line and line.strip().startswith("["):
            head = line.split("]")[0].strip("[ ")
            rest = line.split("]", 1)[1].split()
            if head.isdigit() and rest:
                names[head] = rest[0]

    r = subprocess.run(["readelf", "-sW", path], capture_output=True, text=True)
    out = []
    for line in r.stdout.splitlines():
        f = line.split()
        if len(f) >= 8 and f[0].endswith(":") and f[4] == "GLOBAL":
            out.append((f[7], f[4], names.get(f[6], f[6]), f[1]))
    return sorted(out)


def show(data, at):
    lo = max(0, at - 4)
    hi = min(len(data), at + 8)
    return " ".join("%02x" % b for b in data[lo:hi])


def main():
    files = sys.argv[1:]
    if not files:
        print("usage: asmcheck.py <file.asm> [...]")
        return 2

    bad = 0
    for path in files:
        theirs, err = assemble_nasm(path)
        if theirs is None and "external references" in err:
            # a file that calls into C cannot be assembled flat by
            # anybody, nasm included, a name it does not define has no
            # answer until a linker provides one. so these are compared
            # as objects only, which is the comparison that matters for
            # them anyway
            a, b = elf_objects(path)
            if a is None:
                print("  %-28s neither could assemble it" % path)
                bad += 1
                continue
            ra, rb, ga, gb = (relocations(a), relocations(b),
                              globals_of(a), globals_of(b))
            os.unlink(a)
            os.unlink(b)
            if ra != rb or ga != gb:
                print("  %-28s the object differs" % path)
                for section in sorted(set(ra) | set(rb)):
                    if ra.get(section) != rb.get(section):
                        print("      %-14s nasm %s" % (section, ra.get(section)))
                        print("      %-14s ours %s" % ("", rb.get(section)))
                if ga != gb:
                    print("      globals       nasm %s" % (ga,))
                    print("      %-14s ours %s" % ("", gb,))
                bad += 1
                continue
            print("  %-28s ok      objects only, %d relocations"
                  % (path, sum(len(v) for v in rb.values())))
            continue
        if theirs is None:
            print("  %-28s nasm would not assemble it: %s" % (path, err))
            bad += 1
            continue

        mine, err = assemble_ours(path)
        if mine is None:
            print("  %-28s ours would not: %s" % (path, err))
            bad += 1
            continue

        if mine == theirs:
            a, b = elf_objects(path)
            if a is None:
                print("  %-28s ok      %d bytes" % (path, len(mine)))
                continue
            ra, rb = relocations(a), relocations(b)
            ga, gb = globals_of(a), globals_of(b)
            os.unlink(a)
            os.unlink(b)
            if ra != rb:
                print("  %-28s bytes agree, relocations do not" % path)
                for section in sorted(set(ra) | set(rb)):
                    if ra.get(section) != rb.get(section):
                        print("      %-14s nasm %s" % (section, ra.get(section)))
                        print("      %-14s ours %s" % ("", rb.get(section)))
                bad += 1
                continue
            if ga != gb:
                print("  %-28s bytes agree, the symbol table does not" % path)
                print("      nasm %s" % (ga,))
                print("      ours %s" % (gb,))
                bad += 1
                continue
            print("  %-28s ok      %d bytes, %d relocations" %
                  (path, len(mine), sum(len(v) for v in rb.values())))
            continue

        at = 0
        while at < min(len(mine), len(theirs)) and mine[at] == theirs[at]:
            at += 1
        print("  %-28s DIFFERS at byte %d" % (path, at))
        print("      nasm: %s" % show(theirs, at))
        print("      ours: %s" % show(mine, at))
        print("      (%d bytes vs %d)" % (len(mine), len(theirs)))
        bad += 1

    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())

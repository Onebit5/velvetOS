#!/usr/bin/env python3
"""build the same archive with ours and with ar, and then link with both
crossed over.

an archive is the easiest format in this repository to get *nearly*
right. it is text fields and a list, and something that looks exactly
like an archive is four lines of code, so the danger here is not a
crash, it is a file that only this machine can read, wearing a name
everybody else's tools recognise.

three questions, and the second and third are the ones that matter:

  **is it the same file?** ours and GNU ar over the same objects,
  compared byte for byte. deterministic mode is the default for both,
  no timestamp, no uid, so this is a fair comparison rather than an
  approximate one.

  **can somebody else read what we wrote?** an archive built by ours,
  linked by ld.

  **can we read what somebody else wrote?** an archive built by ar,
  linked by ours. either one alone would pass on a private format; both
  together will not.

and the rule underneath all of it: **a member is pulled in only if it
resolves something still undefined.** that is what an archive is *for*,
and it is checked here by linking against an archive with an unwanted
member in it, one that defines a name nothing asks for and, if it were
pulled in, would define a second `answer` and fail the link outright.

usage:  tools/archeck.py
"""
import subprocess, sys, os, tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OURS_AR   = os.path.join(ROOT, "bin", "tests", "ar")
OURS_LINK = os.path.join(ROOT, "bin", "tests", "link")
SCRIPT    = os.path.join(ROOT, "tests", "link", "simple.ld")


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode, (r.stdout + r.stderr).strip()


def assemble(work, name):
    src = os.path.join(ROOT, "tests", "link", name + ".asm")
    out = os.path.join(work, name + ".o")
    code, say = run(["nasm", "-f", "elf64", src, "-o", out])
    if code != 0:
        print("  nasm would not assemble %s: %s" % (name, say[:120]))
        sys.exit(1)
    return out


def loaded(path):
    """what a linked file actually puts in memory: every PT_LOAD, as
    address and bytes. the same comparison linkcheck makes, and for the
    same reason, how the segments were divided is a decision, what
    lands at an address is a result."""
    code, say = run(["readelf", "-lW", path])
    data = open(path, "rb").read()
    out = {}
    for line in say.splitlines():
        f = line.split()
        if len(f) < 6 or f[0] != "LOAD":
            continue
        off, vaddr, filesz = int(f[1], 16), int(f[2], 16), int(f[4], 16)
        for i in range(filesz):
            out[vaddr + i] = data[off + i]
    return out


def main():
    work = tempfile.mkdtemp()
    bad = 0

    main_o = assemble(work, "usesone")
    one_o  = assemble(work, "libone")
    two_o  = assemble(work, "libtwo")

    ours   = os.path.join(work, "ours.a")
    theirs = os.path.join(work, "theirs.a")
    code, say = run([OURS_AR, "rcs", ours, one_o, two_o])
    if code != 0:
        print("  ours would not write an archive: %s" % say[:160])
        return 1
    # `D` asks for deterministic mode, no timestamp, no uid, one mode
    # for every member, which is what ours writes and always writes.
    #
    # it is spelled out rather than relied on. whether determinism is
    # ar's *default* is decided when binutils is compiled, so the same
    # `ar rcs` writes a zero where one distribution builds it and the
    # time of day where another does, and a check that assumed the
    # first passes on one machine and fails on the next with a
    # difference that is not about this program at all
    code, say = run(["ar", "rcsD", theirs, one_o, two_o])
    if code != 0:
        print("  ar would not write one: %s" % say[:160])
        return 1

    if open(ours, "rb").read() != open(theirs, "rb").read():
        a, b = open(ours, "rb").read(), open(theirs, "rb").read()
        at = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]),
                  min(len(a), len(b)))
        print("  the archives differ at byte %d (ours %d bytes, ar %d)"
              % (at, len(a), len(b)))
        print("      ar:   %s" % " ".join("%02x" % c for c in b[at:at + 8]))
        print("      ours: %s" % " ".join("%02x" % c for c in a[at:at + 8]))
        bad += 1
    else:
        print("  archive       ok      %d bytes, identical to ar's"
              % os.path.getsize(ours))

    # the same program four ways: each linker over each archive. all four
    # have to load the same bytes at the same addresses, and the member
    # nothing wants has to be absent from all of them
    images = {}
    for who, linker in (("ld", ["ld", "-T", SCRIPT]),
                        ("ours", [OURS_LINK, "-T", SCRIPT])):
        for which, archive in (("ours", ours), ("ar", theirs)):
            out = os.path.join(work, "%s-%s.elf" % (who, which))
            code, say = run(linker + ["-o", out, main_o, archive])
            if code != 0:
                print("  %s would not link the archive %s wrote: %s"
                      % (who, which, say[:160]))
                return 1
            images["%s over %s" % (who, which)] = loaded(out)

    first, crossed = None, True
    for name, image in sorted(images.items()):
        if first is None:
            first = (name, image)
            continue
        if image != first[1]:
            print("  %s and %s load different things" % (first[0], name))
            crossed = False
            bad += 1

    # said on its own merits rather than on the tally so far: the two
    # questions are separate, and a difference in the *file* should not
    # take the answer to "can each side read what the other wrote" off
    # the screen, which is the line that says where the fault is
    if crossed:
        print("  crossed over  ok      %d bytes, the same four ways"
              % len(first[1]))

    # and the rule itself, stated as a fact about the result rather than
    # about the link: the unwanted member's bytes are not in it
    if b"unwanted" not in open(os.path.join(work, "ours-ours.elf"), "rb").read():
        print("  the rule      ok      the member nothing wanted stayed out")
    else:
        print("  the member nothing wanted was linked in anyway")
        bad += 1

    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""the projects this system is built on: fetch them, move them.

velvetOS carries neither its kernel nor its loader. it names one release
of each by its version, velvet in kernel-release.mk and philemon in
philemon-release.mk, and this clones the tag for that version, vX.Y.Z,
into the directory it is given, which the makefile keeps in obj/velvet/
and obj/philemon/. a released tag is never moved, which both projects
promise, so the version is all a build needs to know what it ships. a
clone that is already at that tag is built as it is, and the network is
asked only when the version changes.

  release.py NAME fetch DIR [URL VERSION]  make DIR the release named
  release.py NAME local DIR [VERSION]      say what DIR is, to build it as is
  release.py NAME latest [URL]             the newest stable release
  release.py NAME update DIR [URL]         pin the newest one, and fetch it

NAME is velvet or philemon. missing values are that project's pin file's.
the makefile passes its own, so `make KERNEL_URL=../velvet` clones the
same release out of a local copy of velvet, which has to have the tag.
`make KERNEL_SRC=../velvet` builds that checkout as it is instead, which
is how the kernel and the system are worked on at once: `local` checks
there is a kernel there and says which commit it is, and that is the one
build nothing here pins. PHILEMON_URL and PHILEMON_SRC do the same for the
loader.

a stable release is a tag spelled vX.Y.Z and nothing else: v0.5.0-rc1 is
somebody asking for testers, not a kernel to ship.
"""

import os
import re
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
STABLE = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")


class Project:
    """one thing this system is built on: what it is called, the file
    that pins it and the prefix of that file's variables, what it is to
    the build, and a file only a checkout of it has."""

    def __init__(self, name, pin, prefix, what, marker):
        self.name = name
        self.pin = os.path.join(ROOT, pin)
        self.prefix = prefix
        self.what = what
        self.marker = marker


PROJECTS = {
    "velvet": Project("velvet", "kernel-release.mk", "KERNEL", "KERNEL",
                      "sources.mk"),
    "philemon": Project("philemon", "philemon-release.mk", "PHILEMON",
                        "LOADER", os.path.join("src", "philemon.asm")),
}


def say(msg):
    print("  " + msg, file=sys.stderr)


def die(msg):
    print("release.py: " + msg, file=sys.stderr)
    sys.exit(1)


def git(*args):
    r = subprocess.run(("git",) + args, capture_output=True, text=True)
    if r.returncode != 0:
        die("git %s: %s" % (" ".join(args), (r.stderr or r.stdout).strip()))
    return r.stdout.strip()


def pinned(p):
    values = {}
    for line in open(p.pin):
        m = re.match(r"^%s_([A-Z]+)\s*[?:]?=\s*(\S+)" % p.prefix, line)
        if m:
            values[m.group(1)] = m.group(2)
    return values


def resolve(d, rev):
    """the commit rev names in the checkout d, or None."""
    if not os.path.exists(os.path.join(d, ".git")):
        return None
    r = subprocess.run(["git", "-C", d, "rev-parse", "-q", "--verify",
                        rev + "^{commit}"], capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else None


def fetch(p, d, url, version):
    if not STABLE.match(version):
        die("%s is not a release: %s names one as X.Y.Z"
            % (version, os.path.basename(p.pin)))
    tag = "v" + version
    at = resolve(d, "HEAD")
    if at is not None and at == resolve(d, tag):
        return
    if at is not None:
        if git("-C", d, "status", "--porcelain"):
            die("%s is at %s and has changes in it, and %s says %s. it is "
                "not replaced while it holds work: move the work to a "
                "checkout of %s and build that with %s_SRC"
                % (d, at[:12], os.path.basename(p.pin), tag, p.name,
                   p.prefix))
    elif os.path.isdir(d) and os.listdir(d):
        die("%s has files in it and is not a checkout. move it out of "
            "the way and the release is fetched in its place" % d)
    else:
        git("init", "-q", d)
    say("%-7s %s %s, from %s" % (p.what, p.name, tag, url))
    # the fetch runs inside the clone, so a local path given relative to
    # this tree has to be made absolute first, or ../velvet means the
    # clone itself
    if os.path.exists(url):
        url = os.path.abspath(url)
    git("-C", d, "fetch", "-q", "--depth", "1", url,
        "+refs/tags/%s:refs/tags/%s" % (tag, tag))
    git("-C", d, "-c", "advice.detachedHead=false", "checkout", "-q",
        "--detach", tag)


def local(p, d, version):
    """a checkout somebody is working in, built as it is. the one check
    is that it is the project at all, since a typo here would otherwise
    surface as a missing file a hundred lines into the build."""
    if not os.path.isfile(os.path.join(d, p.marker)):
        die("%s is not a checkout of %s: there is no %s in it"
            % (d, p.name, p.marker))
    at = resolve(d, "HEAD")
    if at is None:
        say("%-7s %s as it is, and not v%s" % (p.what, d, version))
        return
    dirty = git("-C", d, "status", "--porcelain")
    say("%-7s %s as it is, at %s%s, and not v%s"
        % (p.what, d, at[:12], " with changes" if dirty else "", version))


def latest(url):
    """the highest X.Y.Z there is a vX.Y.Z tag for. an annotated tag is
    listed twice, as itself and peeled with ^{}, and both are one tag."""
    versions = set()
    for line in git("ls-remote", "--tags", url).splitlines():
        name = line.split("\t")[1][len("refs/tags/"):]
        if name.endswith("^{}"):
            name = name[:-3]
        if name.startswith("v") and STABLE.match(name[1:]):
            versions.add(name[1:])
    if not versions:
        die("there is no stable release at %s" % url)
    return max(versions, key=lambda v: tuple(map(int, v.split("."))))


def update(p, d, url):
    was = pinned(p)["VERSION"]
    best = latest(url)
    if best == was:
        say("%-7s %s v%s is already the newest stable release"
            % (p.what, p.name, best))
        return
    text = open(p.pin).read()
    text = re.sub(r"^(%s_VERSION\s*:=\s*)\S+" % p.prefix, r"\g<1>" + best,
                  text, flags=re.M)
    open(p.pin, "w").write(text)
    say("%-7s %s v%s -> v%s. %s is changed, and committing it is what "
        "moves the system" % (p.what, p.name, was, best,
                              os.path.basename(p.pin)))
    fetch(p, d, url, best)


def main():
    args = sys.argv[1:]
    usage = ("usage: release.py NAME fetch DIR [URL VERSION] "
             "| NAME local DIR [VERSION] | NAME latest [URL] "
             "| NAME update DIR [URL], where NAME is %s"
             % " or ".join(sorted(PROJECTS)))
    if len(args) < 2 or args[0] not in PROJECTS \
            or args[1] not in ("fetch", "local", "latest", "update"):
        die(usage)
    p = PROJECTS[args.pop(0)]
    if args[0] != "latest" and len(args) < 2:
        die(usage)
    pin = pinned(p)

    def arg(i, name):
        return args[i] if len(args) > i and args[i] else pin[name]

    if args[0] == "fetch":
        fetch(p, args[1], arg(2, "URL"), arg(3, "VERSION"))
    elif args[0] == "local":
        local(p, args[1], arg(2, "VERSION"))
    elif args[0] == "latest":
        print(latest(arg(1, "URL")))
    else:
        update(p, args[1], arg(2, "URL"))


if __name__ == "__main__":
    main()

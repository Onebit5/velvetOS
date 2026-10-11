#!/usr/bin/env python3
"""the kernel this system is built on: fetch it, move it.

velvetOS does not carry velvet. it names one release of it in
kernel-release.mk, by its version, and this clones the tag for that
version, vX.Y.Z, into the directory it is given, which the makefile keeps
in obj/velvet/. a released tag is never moved, which velvet's
docs/versioning.rst promises, so the version is all a build needs to know
which kernel it ships. a clone that is already at that tag is built as it
is, and the network is asked only when the version changes.

  kernel.py fetch DIR [URL VERSION]  make DIR the release named
  kernel.py local DIR [VERSION]      say what DIR is, to build it as is
  kernel.py latest [URL]             the newest stable release
  kernel.py update DIR [URL]         pin the newest one, and fetch it

missing values are kernel-release.mk's. the makefile passes its own, so
`make KERNEL_URL=../velvet` clones the same release out of a local copy of
velvet, which has to have the tag. `make KERNEL_SRC=../velvet` builds that
checkout as it is instead, which is how the kernel and the system are
worked on at once: `local` checks there is a kernel there and says which
commit it is, and that is the one build nothing here pins.

a stable release is a tag spelled vX.Y.Z and nothing else: v0.5.0-rc1 is
somebody asking for testers, not a kernel to ship.
"""

import os
import re
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PIN = os.path.join(ROOT, "kernel-release.mk")
STABLE = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")


def say(msg):
    print("  " + msg, file=sys.stderr)


def die(msg):
    print("kernel.py: " + msg, file=sys.stderr)
    sys.exit(1)


def git(*args):
    r = subprocess.run(("git",) + args, capture_output=True, text=True)
    if r.returncode != 0:
        die("git %s: %s" % (" ".join(args), (r.stderr or r.stdout).strip()))
    return r.stdout.strip()


def pinned():
    values = {}
    for line in open(PIN):
        m = re.match(r"^(KERNEL_[A-Z]+)\s*[?:]?=\s*(\S+)", line)
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


def fetch(d, url, version):
    if not STABLE.match(version):
        die("%s is not a release: kernel-release.mk names one as X.Y.Z"
            % version)
    tag = "v" + version
    at = resolve(d, "HEAD")
    if at is not None and at == resolve(d, tag):
        return
    if at is not None:
        if git("-C", d, "status", "--porcelain"):
            die("%s is at %s and has changes in it, and kernel-release.mk "
                "says %s. it is not replaced while it holds work: move the "
                "work to a checkout of velvet and build that with KERNEL_SRC"
                % (d, at[:12], tag))
    elif os.path.isdir(d) and os.listdir(d):
        die("%s has files in it and is not a checkout. move it out of "
            "the way and the release is fetched in its place" % d)
    else:
        git("init", "-q", d)
    say("KERNEL  velvet %s, from %s" % (tag, url))
    git("-C", d, "fetch", "-q", "--depth", "1", url,
        "+refs/tags/%s:refs/tags/%s" % (tag, tag))
    git("-C", d, "-c", "advice.detachedHead=false", "checkout", "-q",
        "--detach", tag)


def local(d, version):
    """a checkout somebody is working in, built as it is. the one check
    is that it is a kernel at all, since a typo here would otherwise
    surface as a missing header a hundred lines into the build."""
    if not os.path.isfile(os.path.join(d, "sources.mk")):
        die("%s is not a checkout of velvet: there is no sources.mk in it"
            % d)
    at = resolve(d, "HEAD")
    if at is None:
        say("KERNEL  %s as it is, and not v%s" % (d, version))
        return
    dirty = git("-C", d, "status", "--porcelain")
    say("KERNEL  %s as it is, at %s%s, and not v%s"
        % (d, at[:12], " with changes" if dirty else "", version))


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


def update(d, url):
    was = pinned()["KERNEL_VERSION"]
    best = latest(url)
    if best == was:
        say("KERNEL  velvet v%s is already the newest stable release" % best)
        return
    text = open(PIN).read()
    text = re.sub(r"^(KERNEL_VERSION\s*:=\s*)\S+", r"\g<1>" + best, text,
                  flags=re.M)
    open(PIN, "w").write(text)
    say("KERNEL  velvet v%s -> v%s. kernel-release.mk is changed, and "
        "committing it is what moves the system" % (was, best))
    fetch(d, url, best)


def main():
    args = sys.argv[1:]
    usage = ("usage: kernel.py fetch DIR [URL VERSION] | local DIR [VERSION] "
             "| latest [URL] | update DIR [URL]")
    if not args or args[0] not in ("fetch", "local", "latest", "update"):
        die(usage)
    if args[0] != "latest" and len(args) < 2:
        die(usage)
    pin = pinned()

    def arg(i, name):
        return args[i] if len(args) > i and args[i] else pin[name]

    if args[0] == "fetch":
        fetch(args[1], arg(2, "KERNEL_URL"), arg(3, "KERNEL_VERSION"))
    elif args[0] == "local":
        local(args[1], arg(2, "KERNEL_VERSION"))
    elif args[0] == "latest":
        print(latest(arg(1, "KERNEL_URL")))
    else:
        update(args[1], arg(2, "KERNEL_URL"))


if __name__ == "__main__":
    main()

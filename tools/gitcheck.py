#!/usr/bin/env python3
"""a repository written by ours, read by real git.

the roadmap names this check itself, and it is the only one worth
having: a store that only this machine can read is a store that agrees
with itself. so the objects are written by our code and then handed to
`git fsck`, `git log` and `git cat-file`, and the tree hash our code
computes for a directory is compared against the one git computes for
the same directory, forty characters that cover every blob, every
name, every mode and the order they are in.

the directory is not arbitrary. it contains `lib/` and `lib.c` side by
side, which is the case git's sort order is peculiar about: a directory
sorts as though its name ended in a slash, so `lib.c` comes first, and
plain strcmp puts it second. one entry out of place changes the tree
hash and every hash above it, and git says only "not properly sorted".

usage:  tools/gitcheck.py
"""
import subprocess, sys, os, tempfile, shutil, random

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OURS = os.path.join(ROOT, "bin", "tests", "git")


def main():
    if not os.path.exists(OURS):
        print("  bin/tests/git is not built")
        return 1
    if shutil.which("git") is None:
        print("  no git to check against")
        return 1

    work = tempfile.mkdtemp()
    env = dict(os.environ, GIT_AUTHOR_NAME="velvet",
               GIT_AUTHOR_EMAIL="velvet@example.invalid",
               GIT_COMMITTER_NAME="velvet",
               GIT_COMMITTER_EMAIL="velvet@example.invalid",
               GIT_AUTHOR_DATE="1700000000 +0000",
               GIT_COMMITTER_DATE="1700000000 +0000")

    def git(*args, **kw):
        return subprocess.run(["git"] + list(args), cwd=work, env=env,
                              capture_output=True, text=True, **kw)

    def ours(*args):
        return subprocess.run([OURS] + list(args), cwd=work, env=env,
                              capture_output=True, text=True)

    bad = 0
    rng = random.Random(20250819)

    git("init", "-q")
    objects = os.path.join(work, ".git", "objects")

    # the content, including the two names that decide the sort order
    os.makedirs(os.path.join(work, "lib"), exist_ok=True)
    os.makedirs(os.path.join(work, "lib", "deep"), exist_ok=True)
    open(os.path.join(work, "lib.c"), "w").write("int main(void){return 0;}\n")
    open(os.path.join(work, "lib", "a.c"), "w").write("/* a */\n")
    open(os.path.join(work, "lib", "deep", "b.c"), "w").write("/* b */\n")
    open(os.path.join(work, "readme"), "w").write("hello velvet\n")
    open(os.path.join(work, "empty"), "w").write("")
    with open(os.path.join(work, "noise.bin"), "wb") as f:
        f.write(rng.randbytes(40000))

    # ours writes the whole tree
    r = ours("--write-tree", objects, ".")
    if r.returncode != 0:
        print("  ours would not write the tree: %s" % r.stderr.strip())
        shutil.rmtree(work, ignore_errors=True)
        return 1
    our_tree = r.stdout.strip()

    # and git computes its own answer for the same directory
    git("add", "-A")
    their_tree = git("write-tree").stdout.strip()
    if our_tree != their_tree:
        print("  the tree we wrote is %s, git says %s" % (our_tree, their_tree))
        print("  " + git("cat-file", "-p", their_tree).stdout.strip()
              .replace("\n", "\n  "))
        bad += 1

    checked = 1

    # a commit of it, written by ours, with the branch pointed at it,
    # and then git asked to walk its own history
    if not bad:
        r = ours("--commit", objects, our_tree, "-", "the first one\n")
        commit = r.stdout.strip()
        if r.returncode != 0:
            print("  ours would not write a commit: %s" % r.stderr.strip())
            bad += 1
        else:
            head = git("symbolic-ref", "HEAD").stdout.strip()
            path = os.path.join(work, ".git", head)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            open(path, "w").write(commit + "\n")

            # the whole point, in four commands
            r = git("fsck", "--strict")
            if r.returncode != 0:
                print("  git fsck refuses it: %s"
                      % (r.stdout + r.stderr).strip()[:300])
                bad += 1
            r = git("log", "--format=%H %s")
            if r.returncode != 0 or commit not in r.stdout:
                print("  git log will not walk it: %s"
                      % (r.stdout + r.stderr).strip()[:200])
                bad += 1
            elif "the first one" not in r.stdout:
                print("  git read the commit and not the message: %s"
                      % r.stdout.strip())
                bad += 1
            r = git("status", "--porcelain")
            if r.returncode != 0 or r.stdout.strip() != "":
                print("  git thinks the tree differs from the commit: %s"
                      % r.stdout.strip()[:200])
                bad += 1
            checked += 4

    # and the other direction: git's objects, read by ours
    if not bad:
        for name in ("lib.c", "readme", "empty", "noise.bin"):
            sha = git("hash-object", name).stdout.strip()
            want = open(os.path.join(work, name), "rb").read()
            r = subprocess.run([OURS, "--cat-file", objects, sha], cwd=work,
                               capture_output=True)
            if r.returncode != 0 or r.stdout != want:
                print("  ours cannot read git's blob for %s" % name)
                bad += 1
            t = subprocess.run([OURS, "--type", objects, sha], cwd=work,
                               capture_output=True, text=True).stdout.strip()
            if t != "blob":
                print("  ours calls git's blob for %s a %s" % (name, t))
                bad += 1
            checked += 2

        r = subprocess.run([OURS, "--type", objects, our_tree], cwd=work,
                           capture_output=True, text=True)
        if r.stdout.strip() != "tree":
            print("  ours does not recognise its own tree")
            bad += 1
        checked += 1

    shutil.rmtree(work, ignore_errors=True)
    if not bad:
        print("  %d checks, and git reads the repository we wrote" % checked)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())

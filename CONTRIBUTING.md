# contributing

velvetOS is a 64-bit system for x86_64: [velvet](https://github.com/Onebit5/velvet)
underneath, and the libc, the programs and the toolchain on top of it. contributions are
welcome, and they do not have to be code: a bug report that reproduces something the
machine gets wrong is worth as much as a patch, and so is the test that catches it next
time. the fastest way in is to build it, boot it, and find the moment where what happens
is not what should.

**first, the right repository.** this one is for what runs in ring 3 or builds the
machine: the programs, the libc, the assembler, linker, make and `ar`, git, what goes on
the medium, and which kernel `kernel/` is pinned to. everything under `kernel/` is
velvet's, and a change to it is sent there. a change that needs both, a new
syscall being the usual one, lands in velvet first and here once the pin moves to
a kernel that has it.

the full guide is [`docs/contributing.rst`](docs/contributing.rst). it covers
which repository a change goes to, the three ways to contribute, what a pull request has
to do, the commit message format, the sign-off, how review works, and one rule that
outranks the rest: **ci is the gate**. a change that does not pass it is not merged,
however good it is, because a tree that does not build is itself the bug. read
[`docs/coding-style.rst`](docs/coding-style.rst) before writing code, and run
`make lint` before you push.

in short: open an issue for a bug. for a feature, open an issue or send an
email to the maintainer of the subsystem it touches, after reading
[ROADMAP.md](ROADMAP.md) and the non-goals it lists. branch from the latest
`main`, keep one subsystem per commit, sign off with `git commit -s`, and make
`make test`, `make boottest` and `make selfboot` pass on your own machine
before ci ever sees them.

velvetOS is GPL-2.0-only, and a contribution is accepted under the same terms:
the `Signed-off-by` trailer is that agreement in the log, and there is no
exception: the console font, which lives in the kernel, is linux's, GPL-2.0 too.

a patch may also arrive as mail, which is how kernel projects have always taken
them. it is not the recommended route: with no mailing list there is no public
archive, so the discussion would live in one inbox. it is there for the people
who already work that way, and [`docs/contributing.rst`](docs/contributing.rst)
has the `git send-email` recipe for anyone who wants it.

docs
====

velvetOS is a tiny 64-bit system for x86_64: a kernel of its own, a libc and
programs to run on it, and an assembler, linker and make that build it. this
is its documentation: what each part is, why it is the way it is, and how to
work on it.

the kernel is velvet, a project of its own since 0.4.0. this tree builds
the release of it that ``kernel-release.mk`` names, cloned into
``obj/velvet/`` by the first build that needs it. its documentation is in
``obj/velvet/docs/``: the subsystems, the
system calls, and how the kernel is built and tested on its own.

the loader is philemon, a project of his own as well: the release
``philemon-release.mk`` names, cloned into ``obj/philemon/``, with his
documentation in ``obj/philemon/docs/``.

it is split in two. the pages here at the root are *about the project*: how
to build it, how to test it, how to write and send a change. the pages
under `subsystems/ <subsystems/index.rst>`_ are *about the code*. one page
per part of the system, saying what it is for and what it decided.

the short version of the machine is in `README.md <../README.md>`_; where it
is going is `ROADMAP.md <../ROADMAP.md>`_, and where it has been is
`CHANGELOG.md <../CHANGELOG.md>`_.

start here
----------

if you are building it for the first time: `building <building.rst>`_.

if you are changing it: `code style <coding-style.rst>`_, then `testing
<testing.rst>`_, then `contributing <contributing.rst>`_, which also says
whether the change belongs here or in the kernel.

if you are reading it: `layout <layout.rst>`_ for the map, then the
subsystem page you care about.

about the project
-----------------

- `code style <coding-style.rst>`_: how to write the code, name things,
  comment, and handle errors; the TODO/FIXME convention.
- `contributing <contributing.rst>`_: which repository a change goes to,
  the workflow, commit messages, sign-off, review, patches by email, and the
  rule that CI is the gate.
- `building <building.rst>`_: the toolchain, the kernel release, every
  make target, running it, and the self-hosting build.
- `testing <testing.rst>`_: the host suites, the checks against other
  implementations, the boot test, and what CI runs.
- `layout <layout.rst>`_: the source tree.
- `releases <versioning.rst>`_: how versions are cut, how the system
  moves to a new kernel release, and why neither is in the log.

the system
----------

- `the userland <subsystems/userland.rst>`_: the libc, the syscall stubs,
  the programs.
- `the toolchain <subsystems/toolchain.rst>`_: the assembler, linker, make,
  ar, git, and the self-hosted kernel.
- `the tools <subsystems/tools.rst>`_: the toolchain's checkers and the
  boot test.
- `booting <subsystems/boot.rst>`_: philemon, or a loader of your own, and
  why only one of those boots.

other files
-----------

- `README.md <../README.md>`_. what the machine is, in one screen.
- `ROADMAP.md <../ROADMAP.md>`_. what it will be, version by version.
- `CHANGELOG.md <../CHANGELOG.md>`_. what it has been.
- `LICENSE <../LICENSE>`_.
- `kernel-release.mk <../kernel-release.mk>`_. which kernel this system is
  built on.
- `.github/workflows/ci.yml <../.github/workflows/ci.yml>`_. what CI runs.

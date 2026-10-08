testing
=======

most of this system can be tested without booting anything, and that is a
design decision rather than a happy accident. the parts that *think* are
kept separate from the parts that *touch hardware*, so the suites compile
the real sources as ordinary linux programs and poke at them.

the kernel's suites are the kernel's, in ``kernel/tests/``, run by its own
``make test`` and by ``make kernel-test`` here, and described in
``kernel/docs/testing.rst``. what is here checks what this tree wrote: the
library, the programs' shared code, git, and the toolchain, and then the
whole machine, booted.

every behaviour change arrives with the suite that proves it. see `tests
come with the change <contributing.rst>`_ in the contributing guide.

how it works
------------

a function that would otherwise reach for the machine reaches through one
narrow seam instead, and the suite replaces the seam and nothing else:

- the libc's allocator gets its memory from ``libc_get_pages()``, so the
  suite includes the real ``stdlib.c`` and hands it pages of the host's,
  and resets its free list between sections.
- git's object store reaches the world through ``userland/gitio.h``, five
  functions: ``gitio_velvet.c`` puts this machine's syscalls under them and
  ``gitio_host.c`` the host's stdio, so the store a program runs is the
  store the suite and ``gitcheck`` drive.
- the text buffer and the diff are pure: a structure and the operations on
  it, with the drawing and the reading left to the program that uses them.

the kernel does the same thing at a larger scale, and its ``testing.rst``
says how. see `testability <coding-style.rst>`_ for the rule that keeps a
seam alive.

``make test``
-------------

the entry point, and the first thing CI runs. no qemu, a minute or two,
most of it the self-hosted kernel:

.. code-block:: sh

   make test

it builds and runs three kinds of thing.

the suites
~~~~~~~~~~

the suites in ``tests/test_*.c``, linked against the real sources and run
as host programs. each prints ``ok`` or ``FAILED`` and a failure prints the
failing case. all of them:

``libc``, ``args``, ``textbuf``, ``difflib``
   the userland library, argument parsing, the editor's text buffer, and the
   diff every program that compares two files shares.

``git``
   the git object store, with no arguments; with some, the tool
   ``gitcheck`` builds a whole repository with.

the foreign-implementation checks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

a check written by the code it checks proves nothing. where another
implementation exists, the project tests against that instead:

``asmcheck``
   assemble every fixture and all four kernel ``.asm`` files with this
   project's assembler and with nasm, and compare the bytes.

``linkcheck`` / ``kernelcheck``
   link with this project's linker and with ``ld`` and compare every byte
   at every address. ``kernelcheck`` does it with the kernel's own objects,
   the ones the kernel's build lists in ``obj/kernel/x86_64/kernel.objects``.

``makecheck``
   run the same makefiles with this project's make and with GNU make.

``archeck``
   build an archive both ways, and link a program with the two crossed over.

``gitcheck``
   write a repository with this project's git and read it with real git.
   ``fsck --strict`` and all.

``srccheck``
   read the source tree back out of the boot image and hold it against the
   working tree, the kernel's files included, and require the archive's
   sha-1 to appear inside the kernel shipped beside it. the checker is the
   kernel's, ``kernel/tools/srccheck.py``, run over this tree's image.

the static checks
~~~~~~~~~~~~~~~~~

``lint``
   the style guide, checked mechanically: the file header, the comment shape,
   the function brace, dashes and first person in prose, tabs and the
   ``//`` rule. ``make lint`` covers the tree and
   ``make lint FILES='userland/ls.c'`` covers the files named. the kernel is
   not in the tree as far as git is concerned, it is one submodule entry,
   and its own ``make lint`` covers it. naming prefixes are warnings until
   the tree is converted. it runs before anything is built, on its own and
   in ci, so it is covered here rather than under the suites.

``toolchain``
   the project's own toolchain standing on its own: our make, our assembler
   and our linker build a program, and it is compared against nasm's and
   ld's.

``selfhost``
   the kernel built by this project's own make, assembler and linker,
   and held against the ordinary build.

``boottest``
------------

the one that needs a machine:

.. code-block:: sh

   make boottest

it builds the image, boots it headless in qemu, and **types commands at
the shell over the serial port**, checking the answers. that is only
possible because com1 feeds the same input queue the keyboard does. the
serial log is left in ``bin/*-boottest.log``, and CI uploads it, with the
boot image, when a run fails.

this is the whole machine, and the only check that is. the kernel's own boot
test boots a medium with no programs on it; this one runs them, in ring 3,
on the pinned kernel: programs that exit with a code, fork, spawn and wait,
read input, and are stopped by ctrl+c, the guest refused a file, and the
source on the medium read with ``cat``.

``selfboot``
------------

the same boot, on a kernel built by this project's own make, assembler and
linker rather than by the machine's. ``make test`` already proves the two
kernels are the same bytes; this proves the one our tools wrote is a
machine that runs. it is the last thing CI does.

running one suite
-----------------

each suite is an ordinary binary in ``bin/tests/``:

.. code-block:: sh

   make bin/tests/libc
   ./bin/tests/libc

``make test`` is still the thing to run before pushing, because it also
runs the static checks and the foreign-implementation comparisons, and a
suite passing on its own does not mean the tree is clean.

what CI does with all of this
-----------------------------

the workflow checks out the kernel at the pinned commit, and runs, in
order: ``make test``, ``make``, ``make boottest``, ``make selfhost``,
``make selfboot``. see `ci is the gate <contributing.rst>`_ for what
happens when one of them fails, which is: the pull request does not merge.

the smoke test
--------------

`contributing`_ covers the pre-push sequence. for a hand check of a running
machine, ``make run`` gives you a shell on com1 and on the qemu window. the
version-specific things worth poking after each change are listed by version
in `CHANGELOG.md <../CHANGELOG.md>`_. each entry is a machine that booted,
and its sentence is what to try.

.. _contributing: contributing.rst

the analysers, the fuzzer and ubsan
-----------------------------------

``sparse``, the parser fuzzer and ubsan in trap mode are all pointed at the
kernel, and moved with it: ``kernel/docs/testing.rst`` has what each is
worth there and how to run it.

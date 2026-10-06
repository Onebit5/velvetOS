building
========

velvetOS builds with a normal toolchain and boots in qemu. there is no
configure step and no dependencies to fetch beyond four programs.

requirements
------------

- ``gcc``. the machine's compiler, used for the whole tree, kernel included
  built by the project's own toolchain (see `self-hosting`_)
- ``nasm``, for the ``.asm`` sources under ``arch/x86_64/``
- ``make``. GNU make, for the ``GNUmakefile``
- ``xorriso``, only for building a bootable image
- ``qemu-system-x86_64``, only for ``make run`` and ``make boottest``

on fedora:

.. code-block:: sh

   dnf install gcc nasm make xorriso qemu-system-x86

on debian or ubuntu:

.. code-block:: sh

   apt-get install gcc nasm make xorriso qemu-system-x86

the python checks in ``tools/`` need a python 3 interpreter and nothing
else. no third-party packages. ``make test`` uses them.

the targets
-----------

``make``
   the default target, and the real deliverable: ``velvetos.img``, the
   bootable disk image philemon wrote. the kernel elf lands in
   ``bin/velvetos`` alongside it.

``make run``
   boot ``velvetos.img`` in qemu with a second drive carrying the data
   filesystem. defaults are four cpus and 2 GiB:

   .. code-block:: sh

      make run                 # four cpus, 2G
      make run CPUS=1          # one cpu, which must always still work
      make run MEM=512M        # less memory, for the pressure paths
      make run QEMU_EXTRA="-d int"

   qemu's window opens *and* com1 is wired to your terminal, and both feed
   the same input queue. the shell cannot tell the difference. tcp port
   5555 and udp port 5556 are forwarded to the host, so a program running
   inside can be reached from outside; the forwarding is the *only* way in,
   so from another terminal it is ``nc -u localhost 5556`` and not the
   guest's own address.

``make lint``
   the style guide, as much of it as a machine can check: the header, the
   comment shape, the function brace, dashes and first person, tabs, markers.
   the whole tree by default, or ``make lint FILES='userland/ls.c'`` for the files
   you name. it is the first thing CI runs, because it needs nothing built.

``make test``
   the host test suites and every checker. no qemu, about a second. this is
   the same command CI runs first, and `testing`_ is the full account.

``make boottest``
   build the image, boot it headless, and type commands at the shell over
   the serial port, checking the answers. needs qemu. the serial log is
   written to ``bin/*-boottest.log``, which is what CI uploads when the
   run fails.

``make clean``
   remove ``bin/``, ``obj/`` and the generated root.

``make distclean``
   the same, and then some.

the architecture knobs
----------------------

``ARCH`` selects the architecture and defaults to ``x86_64``, the only one
there is. the mechanism for more than one is kept deliberately. an
aarch64 port was written and removed, and the boundary it forced is most of
why ``arch/`` is worth anything. see `the architecture boundary
<subsystems/arch.rst>`_.

``make portable-check``
   compile the portable half of the kernel. ``mm/``, ``sched/``,
   ``fs/``, ``lib/``, most of ``drivers/``, against ``arch/none/``, an
   architecture whose every function is an empty stub. if it compiles,
   nothing above the line needs a machine. add ``LIST=1`` and the undefined
   symbols that come back are the porting checklist: every name a port
   would have to define.

``make arch-check ARCH=x86_64``
   compile ``arch/<ARCH>/`` and nothing else. this is what can honestly be
   said about an architecture with no boot code yet. ``CROSS=`` sets the
   toolchain prefix for a cross build.

self-hosting
------------

the project has its own assembler, linker and make, in ``userland/`` and
``tools/``. they are not a toy: the point of 0.4.0 is that the kernel those
three build is the same bytes the machine's toolchain builds.

``make toolchain``
   build the project's assembler, linker, make and ``ar`` as host binaries.

``make selfhost``
   build the kernel with the project's own make, assembler and linker
   instead of the machine's.

``make selfboot``
   boot that kernel. this is the last thing CI runs.

``make kernelcheck``
   link the kernel with both linkers and compare every byte at every
   address.

``make asmcheck`` / ``make linkcheck`` / ``make makecheck`` / ``make archeck``
   the same idea one level down: assemble a fixture with both assemblers,
   link with both linkers, run a makefile with both makes, build an archive
   with both ``ar`` implementations, and compare.

`testing`_ has the rest of the checks and what each one is for.

the build tree
--------------

everything generated comes back out with ``make clean``:

``bin/``
   the kernel, the boot image, the user programs, and the test binaries.

``obj/``
   one object tree per architecture. ``obj/x86_64/``, so switching
   ``ARCH`` and rebuilding does not link x86 objects into another
   machine's kernel and report a relocation error rather than what you did.

``base/ramdisk/bin/``
   the user programs, as they are packed into the ramdisk.

``velvetos.img``
   the bootable image: philemon and its second-stage loader, the kernel,
   the ramdisk tar, and the project's own source tree in the space past it.

``base/ramdisk/`` and ``base/diskroot/`` are inputs, not output: the tar the loader
hands the kernel is built from ``base/ramdisk/``, and the files that go onto the
data disk in ``make run`` come from ``base/diskroot/``.

notes
-----

- the kernel builds ``-Wall -Wextra`` and takes both seriously. a warning
  nobody can justify is a bug that has not happened yet.
- there is no ``make iso`` target any more. the image is written by
  philemon in ``tools/mkboot.py`` and boots directly; the old iso went away
  in 0.1.12, when philemon replaced the bootloader before it.

.. _testing: testing.rst

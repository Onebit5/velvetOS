building
========

velvetOS builds with a normal toolchain and boots in qemu. there is no
configure step and no dependencies to fetch beyond a handful of programs and
the kernel's submodule.

requirements
------------

- ``gcc``, the machine's compiler, used for the whole tree, kernel included,
  and the one program in the chain the project's own toolchain does not
  replace (see `self-hosting`_)
- ``nasm``, for the kernel's ``.asm`` sources and philemon, and as the
  assembler this project's own is checked against
- ``make``. GNU make, for the ``GNUmakefile``
- ``python3``, for the checkers in ``tools/`` and the kernel's image
  builders. no third-party packages
- ``git``, for the submodule, and as the git this project's own is checked
  against
- ``qemu-system-x86_64``, only for ``make run`` and the boot tests

on fedora:

.. code-block:: sh

   dnf install gcc nasm make python3 git qemu-system-x86

on debian or ubuntu:

.. code-block:: sh

   apt-get install gcc nasm make python3 git qemu-system-x86

the kernel
----------

the kernel is velvet, in ``kernel/`` as a git submodule. a fresh clone
needs it fetched:

.. code-block:: sh

   git clone --recurse-submodules https://github.com/Onebit5/velvetOS.git
   # or, in a clone made without it:
   git submodule update --init

the submodule is **pinned**: ``kernel/`` is the commit this tree names, not
whatever velvet's ``main`` is today, and a commit in velvet
reaches this tree only when the pin is moved and that move is committed
here. see `releases <versioning.rst>`_ for when and how.

the kernel is built by its own makefile, which this one calls with
``make -C kernel``, telling it to put its output in ``bin/kernel/`` and
``obj/kernel/`` and to stamp the kernel with the hash of *this* tree's source
archive, because the source shipped beside the kernel on a velvetOS disk is
all of velvetOS. nothing is ever built inside ``kernel/``, so the
submodule's checkout stays exactly what was pinned.

the targets
-----------

``make``
   the default target, and the real deliverable: ``velvetos.img``, the
   bootable disk image philemon wrote, with the kernel, the ramdisk and its
   programs, and the system's own source in the space past it.

``make kernel``
   the kernel, philemon and the kernel's C build tools, built out of
   ``kernel/`` into ``bin/kernel/``. every target that needs any of them
   asks for this first; it answers in a moment when nothing has changed.

``make userland``
   the programs and ``libc.a``, into ``base/ramdisk/bin/``.

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

``make disk``
   throw ``disk.img`` away and build a fresh one. ``make run`` builds it once
   and then leaves it alone, because what you write to it is meant to stay
   written.

``make lint``
   the style guide, as much of it as a machine can check: the header, the
   comment shape, the function brace, dashes and first person, tabs, markers.
   the whole tree by default, or ``make lint FILES='userland/ls.c'`` for the
   files you name. the kernel is a submodule and is linted by its own
   ``make lint``. it is the first thing CI runs, because it needs nothing
   built.

``make test``
   the host test suites and every checker. no qemu, a minute or two, most
   of it the self-hosted kernel. this is the same command CI runs first,
   and `testing`_ is the full account.

``make kernel-test``
   the kernel's own suites, run in ``kernel/`` the way velvet's CI
   runs them. not part of ``make test``: the pinned kernel passed them
   before it was pinned, and running them again here checks nothing new.

``make boottest``
   build the image, boot it headless, and type commands at the shell over
   the serial port, checking the answers. needs qemu. the serial log is
   written to ``bin/*-boottest.log``, which is what CI uploads when the
   run fails.

``make clean``
   remove ``bin/`` and ``obj/``, the kernel's outputs included.

``make distclean``
   the same, and then some.

the build runs in parallel, ``make -j``, with everything above.

self-hosting
------------

the project has its own assembler, linker, make and ``ar``, in
``toolchain/``. they are not a toy: the point of 0.4.0 is that the kernel
those tools build is the same bytes the machine's toolchain builds.

``make toolchain``
   build the project's assembler, linker and make as host binaries, and use
   them to build a small program that is compared against nasm's and ld's.

``make selfhost``
   build the kernel in ``kernel/`` with the project's own make, assembler and
   linker instead of the machine's. ``tests/toolchain/kernel.mk`` reads what
   the kernel is made of out of ``kernel/sources.mk``, so the two builds
   compile the same files and link them in the same order.

``make selfboot``
   boot that kernel. this is the last thing CI runs.

``make kernelcheck``
   link the kernel's own objects, the ones its build lists in
   ``obj/kernel/x86_64/kernel.objects``, with both linkers and compare every
   byte at every address.

``make asmcheck`` / ``make linkcheck`` / ``make makecheck`` / ``make archeck``
   the same idea one level down: assemble a fixture with both assemblers,
   link with both linkers, run a makefile with both makes, build an archive
   with both ``ar`` implementations, and compare.

`testing`_ has the rest of the checks and what each one is for.

the build tree
--------------

everything generated comes back out with ``make clean``:

``bin/``
   the boot image's pieces, the source archive, the test binaries and the
   toolchain.

``bin/kernel/``, ``obj/kernel/``
   the kernel, philemon and the kernel's C tools, as the kernel's makefile
   built them for this tree.

``obj/``
   the userland's objects and ``libc.a``.

``base/ramdisk/bin/``
   the user programs, as they are packed into the ramdisk.

``velvetos.img``
   the bootable image: philemon and its second-stage loader, the kernel,
   the ramdisk tar, and the project's own source tree in the space past it.

``disk.img``
   the data disk ``make run`` attaches.

``base/ramdisk/`` and ``base/diskroot/`` are inputs, not output: the tar the
loader hands the kernel is built from ``base/ramdisk/``, and the files that
go onto the data disk in ``make run`` come from ``base/diskroot/``.

notes
-----

- the tree builds ``-Wall -Wextra`` and takes both seriously. a warning
  nobody can justify is a bug that has not happened yet.
- there is no ``make iso`` target any more. the image is written by
  philemon's own tool, ``kernel/tools/mkboot.py``, and boots directly; the
  old iso went away in 0.1.12, when philemon replaced the bootloader before
  it.

.. _testing: testing.rst

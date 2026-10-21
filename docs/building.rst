building
========

velvetOS builds with a normal toolchain and boots in qemu. there is no
configure step and nothing to fetch beyond a handful of programs and the
kernel release the first build clones for itself.

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
- ``git``, to fetch the kernel's release, and as the git this project's own
  is checked against
- ``qemu-system-x86_64``, only for ``make run`` and the boot tests

on fedora:

.. code-block:: sh

   dnf install gcc nasm make python3 git qemu-system-x86

on debian or ubuntu:

.. code-block:: sh

   apt-get install gcc nasm make python3 git qemu-system-x86

the kernel
----------

the kernel is velvet, and this tree does not carry it.
``kernel-release.mk`` names one release of it by its version, and the first
build that needs the kernel clones that release's tag, ``v`` and the
version, into ``obj/velvet/``. it is cloned again only when the version
changes. it is in ``obj/`` because it is something the build makes, the
same as everything else there: git here never sees it, and ``make clean``
throws it away. a fresh clone is all there is to it:

.. code-block:: sh

   git clone https://github.com/Onebit5/velvetOS.git
   cd velvetOS
   make

the release is **pinned**: ``obj/velvet/`` is the release this tree names, not
whatever velvet's newest is today, and a velvet release reaches this tree
only when ``kernel-release.mk`` is moved and that move is committed here.
``make kernel-update`` does the moving: it finds the newest stable release,
``vX.Y.Z`` with nothing after it, rewrites the file, and fetches it. see
`releases <versioning.rst>`_ for when.

two variables change where the kernel comes from without touching the file:

``KERNEL_URL``
   the same release from somewhere else, a local copy of velvet for
   instance: ``make KERNEL_URL=../velvet``. the copy has to have the tag.

``KERNEL_SRC``
   a checkout of velvet to build as it is, unchecked, for working on the
   kernel and the system at once: ``make KERNEL_SRC=../velvet``. commit or
   branch there as in any checkout, and build the system on it. nothing is
   fetched, and the build says which commit it used and that it was not
   the pinned release. ``obj/velvet/`` is never that checkout: a fetched
   release with changes in it stops the build rather than being replaced,
   since replacing it would throw the changes away.

the kernel is built by its own makefile, which this one calls with
``make -C obj/velvet``, telling it to put its output in ``bin/kernel/`` and
``obj/kernel/`` and to stamp the kernel with the hash of *this* tree's source
archive, because the source shipped beside the kernel on a velvetOS disk is
all of velvetOS, the kernel's files under ``kernel/`` in it. so a build here
leaves the kernel's checkout exactly as it was. ``make kernel-test`` is the
one thing that builds inside it, into the ``bin/`` and ``obj/`` velvet's own
``.gitignore`` keeps out of its tree.

the targets
-----------

``make``
   the default target, and the real deliverable: ``velvetos.img``, the
   bootable disk image philemon wrote, with the kernel, the ramdisk and its
   programs, and the system's own source in the space past it.

``make kernel``
   the kernel and the kernel's C build tools, built out of ``obj/velvet/``
   into ``bin/kernel/``. every target that needs any of them asks for this
   first; it answers in a moment when nothing has changed.

``make philemon``
   the loader, built out of ``obj/philemon/`` into ``bin/philemon/``: his
   two halves and his image writer in C. the same: asked for by whatever
   needs it, and quick when nothing changed.

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
   files you name. the kernel is not part of this tree and is linted by its
   own ``make lint``. it is the first thing CI runs, because it needs nothing
   built.

``make test``
   the host test suites and every checker. no qemu, a minute or two, most
   of it the self-hosted kernel. this is the same command CI runs first,
   and `testing`_ is the full account.

``make kernel-test``
   the kernel's own suites, run in its checkout the way velvet's CI
   runs them. not part of ``make test``: the release passed them before it
   was released, and running them again here checks nothing new.

``make kernel-update``
   move ``kernel-release.mk`` to the newest stable velvet release and fetch
   it. it commits nothing: moving the system to a new kernel is a commit
   somebody makes, and the one line in the log that says every build after
   it ships that kernel.

``make boottest``
   build the image, boot it headless, and type commands at the shell over
   the serial port, checking the answers. needs qemu. the serial log is
   written to ``bin/*-boottest.log``, which is what CI uploads when the
   run fails.

``make clean``
   remove ``bin/`` and ``obj/``, the kernel's outputs and the fetched
   release included. a checkout named with ``KERNEL_SRC`` is not touched.

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
   build the kernel with the project's own make, assembler and linker
   instead of the machine's. ``tests/toolchain/kernel.mk`` reads what the
   kernel is made of out of the kernel's ``sources.mk``, so the two builds
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
   the kernel and its C tools, as the kernel's makefile built them for this
   tree.

``bin/philemon/``, ``obj/loader/``
   philemon and his image writer, as his makefile built them.

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
  philemon's own tool, his ``tools/mkboot.py``, and boots directly; the old
  iso went away in 0.1.12, when philemon replaced the bootloader before it.
  why the image has to be his, and what it takes to boot with another loader,
  is `booting <subsystems/boot.rst>`_.

.. _testing: testing.rst

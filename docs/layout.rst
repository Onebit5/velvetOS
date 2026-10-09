layout
======

the tree, and what each part of it is for.

the top level says what the system is: the kernel in ``kernel/``, what runs
on it in ``userland/``, what builds it in ``toolchain/`` and ``tools/``, and
what goes on the medium in ``base/``.

at a glance
-----------

::

    kernel/            velvet, a git submodule: the kernel, philemon,
                       its suites, and the tools that write images and
                       disks. its own tree, with its own docs/layout.rst
    userland/          what runs on the machine: the libc and the programs
      libc/            start, stdio, stdlib, format, string
    toolchain/         what builds the machine: the assembler, the linker,
                       make and ar
    base/ramdisk/      the files packed into the boot tar (input), and the
                       programs, once built, in base/ramdisk/bin/
    base/diskroot/     the files on the data disk in `make run` (input)
    tests/             the host suites, run by `make test`, and the
                       fixtures the toolchain is checked against
    tools/             the toolchain's checkers and the boot test
    docs/              this
    GNUmakefile        the build
    .gitmodules        where kernel/ comes from
    ROADMAP.md         where it is going
    CHANGELOG.md       where it has been

the parts
---------

.. list-table::
   :header-rows: 1

   * - part
     - directory
     - page
   * - the userland
     - ``userland/``
     - `subsystems/userland.rst <subsystems/userland.rst>`_
   * - the toolchain
     - ``toolchain/``
     - `subsystems/toolchain.rst <subsystems/toolchain.rst>`_
   * - the checkers and the boot test
     - ``tools/``
     - `subsystems/tools.rst <subsystems/tools.rst>`_
   * - the kernel
     - ``kernel/``
     - ``kernel/docs/layout.rst``, and a page per subsystem under
       ``kernel/docs/subsystems/``

the line between this tree and the kernel
-----------------------------------------

**this tree uses the kernel's files and the kernel never uses this tree's.**
the libc compiles ``kernel/lib/string.c``, ``hash.c``, ``deflate.c``,
``inflate.c`` and ``epoch.c`` rather than keeping copies; git's object store
includes the kernel's headers, from ``kernel/include/``, for the same
reason; every program includes the syscall numbers and what the calls take
from ``kernel/include/uapi/velvet/``; the disk image is written
by ``kernel/tools/mkboot.py`` and the disks by the kernel's ``mkext4``; and
the self-hosted build reads ``kernel/sources.mk`` for what the kernel is made
of. every one of those reaches into the pinned commit, so moving the pin
moves them all together, and nothing in ``kernel/`` names a path out here.
that direction is what lets the kernel be a project of its own.

the syscall table used to be the one thing written down on both sides,
equal by care rather than by construction. it is written once now, in the
kernel's ``include/uapi/velvet/``, and ``userland/syscall.h`` includes it.
see `the userland <subsystems/userland.rst>`_.

inputs and outputs
------------------

``base/ramdisk/`` and ``base/diskroot/`` are *inputs*: the first is packed into
the boot tar the loader hands the kernel, the second is copied onto the data
disk by ``make run``. ``base/ramdisk/etc/os-release`` is where the system's
own version is written; the kernel's is at the top of
``kernel/GNUmakefile``.

everything under ``bin/`` and ``obj/`` is generated and comes back out with
``make clean``, the kernel's outputs included: the kernel is built into
``bin/kernel/`` and ``obj/kernel/``, never inside ``kernel/``, so the
submodule's checkout stays exactly what was pinned. the boot image,
``velvetos.img``, is written by ``kernel/tools/mkboot.py`` and carries
philemon, the kernel, the ramdisk and the system's own source tree, the
kernel's included. `building`_ is the details.

.. _building: building.rst

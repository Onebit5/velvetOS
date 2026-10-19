layout
======

the tree, and what each part of it is for.

the top level says what the system is: what runs on the kernel in
``userland/``, what builds it in ``toolchain/`` and ``tools/``, and what goes
on the medium in ``base/``. the kernel is not in it. it is velvet, and the
build fetches the release this tree names into ``obj/velvet/``.

at a glance
-----------

::

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
    kernel-release.mk  which velvet release the build fetches, by its
                       version
    ROADMAP.md         where it is going
    CHANGELOG.md       where it has been
    obj/velvet/        that release, cloned by the build with the rest of
                       what it makes: the kernel, philemon, its suites,
                       and the tools that write images and disks. its own
                       tree, with its own docs/layout.rst

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
     - ``obj/velvet/``
     - its ``docs/layout.rst``, and a page per subsystem under its
       ``docs/subsystems/``

the line between this tree and the kernel
-----------------------------------------

**this tree uses the kernel's files and the kernel never uses this tree's.**
every program includes the syscall numbers and what the calls take from its
``include/uapi/velvet/``, which is the one part of the kernel's source a
program sees; the library's string functions, hashes, deflate and dates
are its own, in ``userland/libc/``, and not the kernel's ``lib/``, which
is the kernel's insides; the disk image is written by its
``tools/mkboot.py`` and the disks by its ``mkext4``; and the self-hosted
build reads its ``sources.mk`` for what the kernel is made of. every one of
those reaches into the release ``kernel-release.mk`` names, through the one
variable ``KERNEL_SRC`` that says where it is, so moving that file moves
them all together, and nothing in the kernel names a path out here. that direction is what lets the kernel be a project
of its own.

the syscall table used to be the one thing written down on both sides,
equal by care rather than by construction. it is written once now, in the
kernel's ``include/uapi/velvet/``, and ``userland/syscall.h`` includes it.
see `the userland <subsystems/userland.rst>`_.

inputs and outputs
------------------

``base/ramdisk/`` and ``base/diskroot/`` are *inputs*: the first is packed into
the boot tar the loader hands the kernel, the second is copied onto the data
disk by ``make run``. ``base/ramdisk/etc/os-release`` is where the system's
own version is written; the kernel's is at the top of its own
``GNUmakefile``.

everything under ``bin/`` and ``obj/`` is generated and comes back out with
``make clean``, the fetched release and the kernel's outputs included: the
kernel is built into ``bin/kernel/`` and ``obj/kernel/``, never inside its
checkout, so the release stays exactly what was fetched. the boot image,
``velvetos.img``, is written by the kernel's ``tools/mkboot.py`` and carries
philemon, the kernel, the ramdisk and the system's own source tree, with
the kernel's files under ``kernel/`` in it. `building`_ is the details.

.. _building: building.rst

layout
======

the tree, and what each part of it is for.

the top level says what the project is: the machine in ``kernel/`` and
``boot/``, what runs on it in ``userland/``, what builds it in ``toolchain/``
and ``tools/``, and what goes on the medium in ``base/``.

at a glance
-----------

::

    kernel/            the kernel
      arch/            the boundary: irq, cpu, mmu, context, machine
        x86_64/        gdt, idt, isr stubs, irq dispatch, pic, apic, port io
        none/          an architecture that does nothing, for portable-check
      drivers/         serial, console, tty, ps/2 keyboard and mouse, pit,
                       rtc, pci, ahci, e1000, partitions, the font
      net/             ethernet, arp, ip, icmp, udp, tcp, sockets, dhcp,
                       dns, http
      fs/              vfs, disk, bcache, ext4, fat32, jbd2, path, pipe,
                       ramdisk, source, mkfs, fsck, install
      lib/             kprintf, panic, string, hash, deflate/inflate,
                       epoch, ksyms, backtrace
      mm/              pmm, buddy, slab, kmalloc, vmm, addrspace, pressure
      sched/           threads, run queue, context switch, processes,
                       signals, auth, init, locks
      shell/           the velvet room terminal
      main.c           kmain
      boot.c           the loader handoff
      linker.ld        the higher-half layout and the section symbols
    boot/              philemon, the bootloader
    userland/          what runs on the machine: the libc and the programs
      libc/            start, stdio, stdlib, format, string
    toolchain/         what builds the machine: the assembler, the linker,
                       make and ar
    base/ramdisk/      the files packed into the boot tar (input)
    base/diskroot/     the files on the data disk in `make run` (input)
    tests/             the host suites, run by `make test`
    tools/             the checkers and image builders: the python ones, and
                       the C ones that replace them
    docs/              this
    GNUmakefile        the build
    ROADMAP.md         where it is going
    CHANGELOG.md       where it has been

the subsystems
--------------

each directory under ``kernel/`` is a subsystem with a page of its own:

.. list-table::
   :header-rows: 1

   * - subsystem
     - directory
     - page
   * - the architecture boundary
     - ``arch/``
     - `subsystems/arch.rst <subsystems/arch.rst>`_
   * - memory
     - ``mm/``
     - `subsystems/mm.rst <subsystems/mm.rst>`_
   * - scheduling and processes
     - ``sched/``
     - `subsystems/sched.rst <subsystems/sched.rst>`_
   * - filesystems
     - ``fs/``
     - `subsystems/fs.rst <subsystems/fs.rst>`_
   * - drivers
     - ``drivers/``
     - `subsystems/drivers.rst <subsystems/drivers.rst>`_
   * - networking
     - ``net/``
     - `subsystems/net.rst <subsystems/net.rst>`_
   * - the library
     - ``lib/``
     - `subsystems/lib.rst <subsystems/lib.rst>`_
   * - the shell
     - ``shell/``
     - `subsystems/shell.rst <subsystems/shell.rst>`_
   * - the bootloader
     - ``boot/``
     - `subsystems/boot.rst <subsystems/boot.rst>`_
   * - the userland
     - ``userland/``
     - `subsystems/userland.rst <subsystems/userland.rst>`_
   * - the toolchain
     - ``toolchain/``
     - `subsystems/toolchain.rst <subsystems/toolchain.rst>`_
   * - the checkers and image builders
     - ``tools/``
     - `subsystems/tools.rst <subsystems/tools.rst>`_

the one rule that crosses all of them
-------------------------------------

**machine-specific code lives under ``arch/<machine>/`` and nowhere else.**
everything above that line is portable, and ``make portable-check`` and
``tools/checkarch.py`` fail the build when something crosses it. the full
account is in `the architecture boundary <subsystems/arch.rst>`_ and in
`code style <coding-style.rst>`_.

inputs and outputs
------------------

``base/ramdisk/`` and ``base/diskroot/`` are *inputs*: the first is packed into the
boot tar the loader hands the kernel, the second is copied onto the data
disk by ``make run``. everything under ``bin/`` and ``obj/`` is generated
and comes back out with ``make clean``. the boot image, ``velvetos.img``, is
written by ``tools/mkboot.py`` and carries philemon, the kernel, the ramdisk
and the project's own source tree. `building`_ is the details.

.. _building: building.rst

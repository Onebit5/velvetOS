subsystems
==========

one page per part of the system: what it is for, what it decided, and what
it deliberately does not do. the map of the whole tree is in `layout
<../layout.rst>`_.

- `the userland <userland.rst>`_. ``userland/``: the libc and the programs
- `the toolchain <toolchain.rst>`_. ``toolchain/``: the assembler, linker,
  make and ar, and git
- `the tools <tools.rst>`_. ``tools/``: the toolchain's checkers and the boot
  test
- `booting <boot.rst>`_: philemon, or a loader of your own, and why only
  one of those boots

the kernel's subsystems are the kernel's: ``arch/``, ``mm/``, ``sched/``,
``fs/``, ``drivers/``, ``net/``, ``lib/`` and ``shell/``, each with a page
in velvet's ``docs/subsystems/``, beside its ``docs/syscalls.rst``, the abi
the userland is written against. the loader is philemon's, with a page of
his own in his ``docs/``, and how the system is booted, with him or
without him, is `booting <boot.rst>`_.

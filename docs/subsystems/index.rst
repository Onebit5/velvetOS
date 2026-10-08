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

the kernel's subsystems are the kernel's: ``arch/``, ``mm/``, ``sched/``,
``fs/``, ``drivers/``, ``net/``, ``lib/``, ``shell/`` and philemon in
``boot/``, each with a page in ``kernel/docs/subsystems/``, beside
``kernel/docs/syscalls.rst``, the abi the userland is written against.

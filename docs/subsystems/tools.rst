the tools
=========

``tools/`` is the python that checks the toolchain this tree builds, and the
boot test that checks the whole machine. it is deliberately ordinary python 3
with no third-party imports: a tool here has to run on a machine that has
nothing but an interpreter, which is the same constraint the machine itself
is built under. see `python <../coding-style.rst>`_ for the style.

the image builders and the kernel's own checkers are not here. the disk
builders and checkers are the kernel's, in its ``tools/``, because what they
build and check is the kernel and the formats it reads: ext4 and fat32, the
partition tables, the source stamp. the image writer is philemon's, in his
``tools/``, because the image is his format. this tree calls them where it
needs them, ``mkboot.py`` out of philemon's checkout to write
``velvetos.img`` and ``srccheck.py`` out of the kernel's to read it back
(``obj/philemon/`` and ``obj/velvet/``, unless ``PHILEMON_SRC`` or
``KERNEL_SRC`` names another), and each project's ``docs/`` describes its
own.

the second opinion
------------------

the checkers here exist for one reason, stated in `contributing
<../contributing.rst>`_ as a rule: a check written by the code it checks
proves nothing. where another implementation of a format exists, the project
tests against that instead.

``asmcheck.py``
   assemble a file with this project's assembler and with nasm, and compare.
   stricter than it sounds, because nasm *chooses* between valid encodings.
   see `the toolchain <toolchain.rst>`_.

``linkcheck.py``
   link the same objects with this project's linker and with ``ld``, and
   compare what actually gets loaded. ``make kernelcheck`` runs it over the
   kernel's own objects, the ones the kernel's build lists in
   ``obj/kernel/x86_64/kernel.objects``.

``makecheck.py``
   run the same makefile with this project's make and with GNU make, and
   compare every command, message, exit status and the directory left behind.

``archeck.py``
   build the same archive with this project's ``ar`` and with GNU ``ar``, and
   then link with both crossed over.

``gitcheck.py``
   write a repository with this project's git and read it with real git.
   ``fsck --strict``, ``log``, ``status``, and the tree hash against ``git
   write-tree``.

``selfcheck.py``
   hold the kernel built by this project's tools against the one the
   machine's tools built; the only differences allowed are the ``__TIME__``
   bytes.

the static checks
-----------------

``lint.py``
   the style guide, as much of it as a machine can hold. it lints what this
   tree wrote: the kernel is fetched into ``obj/velvet/``, which git here
   ignores, so ``git ls-files`` does not name it at all, and the kernel's
   own lint holds it to the kernel's copy of the guide.

the boot test
-------------

``boottest.sh``
   build the image, boot it headless in qemu, and **type commands at the
   shell over the serial port**, checking the answers. this is the whole
   machine: the kernel, and the programs running in ring 3 on top of it,
   which the kernel's own boot test cannot reach because its medium has no
   programs on it. it is a shell script rather than python because it is
   mostly qemu's own flags. the serial log is left in ``bin/*-boottest.log``,
   and CI uploads it with the boot image when a run fails. see `testing
   <../testing.rst>`_.

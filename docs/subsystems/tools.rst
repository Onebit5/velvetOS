the tools
=========

``tools/`` is the python that checks the project and builds the images it
runs. it is deliberately ordinary python 3 with no third-party imports: a
tool here has to run on a machine that has nothing but an interpreter, which
is the same constraint the machine itself is built under. see `python
<../coding-style.rst>`_ for the style.

there are two jobs in here, and the split matters: tools that **build**
something the machine boots from or reads, and tools that **check** the
project against somebody else's implementation of the same thing.

the second opinion
------------------

most of the checkers exist for one reason, stated in `contributing
<../contributing.rst>`_ as a rule: a check written by the code it checks
proves nothing. where another implementation of a format exists, the project
tests against that instead.

``asmcheck.py``
   assemble a file with this project's assembler and with nasm, and compare.
   stricter than it sounds, because nasm *chooses* between valid encodings.
   see `the toolchain <toolchain.rst>`_.

``linkcheck.py``
   link the same objects with this project's linker and with ``ld``, and
   compare what actually gets loaded.

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

``hashcheck.py``
   the three sums against python, ``zlib`` and git itself.

``zlibcheck.py``
   deflate and inflate against python's ``zlib`` at all ten levels, because
   a decompressor tested only against this project's compressor has been
   tested against one third of the format and the third it wrote itself.

``readext4.py``
   read an ext4 or ext2 image and complain about it. written from the
   on-disk layout, sharing no line with the driver or the formatter.

``readfat.py``
   the same, for fat32.

``fsckcheck.py``
   break a filesystem on purpose, one deliberate break per check, and demand
   the project's fsck finds each. the repaired image is then handed to
   ``readext4.py``, which shares no line with the checker, because an fsck
   saying it fixed something is an fsck's opinion of itself.

``srccheck.py``
   read the source tree back out of the boot image with python's own
   ``tarfile``; somebody else's tar reader, which is the entire point.
   compare every file byte for byte, require that everything git tracks is in
   there, and require the archive's sha-1 to appear inside the kernel shipped
   beside it.

``selfcheck.py``
   hold the kernel built by this project's tools against the one the
   machine's tools built; the only differences allowed are the ``__TIME__``
   bytes.

``toolcheck.py``
   the C build tools against the python ones they replace.

the static checks
-----------------

``checkarch.py``
   does the arch boundary hold? grep for inline assembly and x86 headers
   outside ``arch/``. a line drawn and not checked decays the first time
   somebody needs a ``hlt`` in a hurry. silently, because the kernel goes
   on building and booting perfectly either way.

``portable.py``
   the half ``checkarch.py`` cannot see: it greps, and cannot catch a
   portable file that quietly *depends* on something only x86 supplies. this
   compiles the portable kernel against ``arch/none/``. ``--list`` prints the
   porting checklist. every name a port would have to define.

``checkfmt.py``
   every ``kprintf``/``panic`` format string against what ``lib/kprintf.c``
   actually implements. this exists because of a bug that cost an afternoon:
   gcc checks a format string against *real* printf, which accepts far more
   than this formatter does, so a ``%-7s`` slipped through, printed itself
   literally, and read every argument after it from the wrong slot.

the image builders
------------------

``mkboot.py``
   build the bootable disk image for philemon: the bootloader, the table at
   sector 32 naming where everything is, the kernel and the ramdisk. there is
   no filesystem in the first 512 bytes, because there is barely room for
   anything.

``mkfat.py``
   build a fat32 image and fill it from a directory. there is no
   ``mkfs.vfat`` on this machine, so this writes the filesystem by hand.

``mkext4.py``
   the same for ext2/ext4. note that the machine can now also format ext4
   itself (``fs/mkfs.c``), and the two are checked against each other.

``mkdisk.py``
   wrap filesystem images in a partition table, because a disk is not a
   filesystem.

``srcstamp.py``
   hash the source archive and write the C file that tells the kernel what its
   own source hashes to. see `the source on the medium
   <fs.rst>`_.

``gensyms.py``
   turn the kernel's function symbols into a C table, so a panic can say
   ``kmain+0x42`` rather than making you run ``addr2line`` by hand. reads the
   ELF's symbol table itself rather than asking ``nm``, because a
   self-hosting machine has no more binutils on it than it has python.

``bin2c.py``
   turn a flat binary into a C array. used for the code a second cpu wakes
   up in.

``font2c.py``
   bdf to C array, for the console font. run once; the output is committed,
   since regenerating it needs the original bdf.

the boot test
-------------

``boottest.sh``
   build the image, boot it headless in qemu, and **type commands at the
   shell over the serial port**, checking the answers. it is a shell script
   rather than python because it is mostly qemu's own flags. the serial log
   is left in ``bin/*-boottest.log``, and CI uploads it with the boot image
   when a run fails. see `testing <../testing.rst>`_.

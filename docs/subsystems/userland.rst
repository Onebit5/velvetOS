the userland
============

``userland/`` is the programs that run in ring 3, and the library they link
against. they are separate from the kernel by hardware rather than by
convention: their own address space, their own privilege level, and a
syscall as the only way across.

the syscall interface
---------------------

``userland/syscall.h`` is the user side of the door: six inline stubs around the
``syscall`` instruction, and a wrapper for each call. the numbers are the
kernel's, included from its ``include/uapi/velvet/``. there is no libc out here;
these stubs are the whole runtime. ``rcx`` and ``r11`` are destroyed by the
``syscall`` instruction itself, and the kernel may clobber anything the abi
allows a call to; the five-argument form puts the fourth and fifth in ``r10``
and ``r8`` by name, because the instruction destroys ``rcx`` before anyone
could have read it.

the calls, grouped:

- **process**: ``exit``, ``getpid``, ``fork``, ``spawn``, ``execv``,
  ``wait`` and ``waitpid``, ``getuid``, ``setuid``, ``setpgid``,
  ``alarm``, ``now``, ``yield``, ``sleep``, ``uptime``, ``reboot``.
- **files**: ``open``, ``close``, ``read``, ``write``, ``create``,
  ``lseek``, ``pipe``, ``dup2``, ``readdir``, ``stat``, ``mkdir``,
  ``rmdir``, ``unlink``, ``rename``, ``chdir``, ``getcwd``, ``chmod``,
  ``chown``, ``symlink``, ``readlink``.
- **filesystems**: ``sync``, ``fsck``, ``resize``, and ``source_tree``
  for the source on the boot medium.
- **memory**: ``mmap``, ``munmap``.
- **the terminal**: ``getkey``, ``screen``, ``cursor``, ``clear``,
  ``ttymode``, ``winsize``, ``ttyattach``, ``tcsetpgrp``, ``tcgetpgrp``.
- **the kernel's log**: ``klogctl``.
- **environment**: ``getenv``, ``setenv``.
- **network**: ``socket``, ``sendto``, ``recvfrom``, ``recvwait``,
  ``select``, ``connect``, ``listen``, ``accept``, ``send``, ``recv``,
  ``shutdown``.
- **signals**: ``signal``, ``signal_ignore``, ``signal_default``,
  ``sigsend``, ``sigreturn``, ``sigmask``.

a syscall argument that is a user address is validated before it is
dereferenced, in one place, in the syscall layer, and no kernel function
trusts a pointer it did not create.

the other side of the door is the kernel's, and so is the table: every call,
what it takes and what it gives back, is in velvet's ``docs/syscalls.rst``.
the numbers are written down once, in the kernel's
``include/uapi/velvet/syscall.h``, and ``userland/syscall.h`` includes them
from the pinned kernel, along with the signal numbers, the keys ``getkey``
hands back and the terminal flags ``tty_mode`` takes. they used to be copied
here, under an XXX saying nothing held the two lists together, and a call
inserted in the middle of either would have renumbered every call after it.
``execv`` has no envp, because the environment is kept by the kernel for
the process and goes across an exec with it, and ``waitpid`` is
``wait`` with its flags, which every older caller passes as 0.
``struct stat`` and ``struct from`` keep the names a program expects and
are checked against the kernel's ``struct user_stat`` and ``struct
user_from`` at compile time, size and every field, so a layout that
drifts does not build. the signal trampoline spells its ``mov`` out of
``SYS_SIGRETURN`` for the same reason. a new call lands in the kernel
first and is usable here once a velvet release has it,
``kernel-release.mk`` names that release, and ``syscall.h`` has its
wrapper.

the libc
--------

every program here used to include ``syscall.h`` and write its own
everything. a compiler cannot be written that way, so ``userland/libc/`` is
``string``, ``stdlib``, ``stdio`` and a crt0, and the test of it is not a
feature list, it is whether **a program written for another unix has a
chance of building**.

``start.c``
   the smallest file and the one that decides that. it defines ``_start``,
   calls ``main``, and exits with what it returned. before it, every
   program here was entered at ``_start`` and had to call ``exit`` itself,
   which no program written anywhere else does.

``stdio.h`` / ``format.c``
   ``printf``, ``fprintf``, ``puts``, ``putchar``, ``getline_fd``. there
   are **no FILE streams**, deliberately rather than unfinished: a stream
   is a buffer plus a descriptor plus a position plus rules about when to
   flush, and every one of those is a decision a program can make better
   than a library can. the formatter is a second implementation of
   something the kernel already has. a real cost, and the honest price of
   a kernel and a userland that share no code.

``stdlib.c``
   ``malloc``, ``free``, ``calloc``, ``realloc``, ``strtol``, ``atoi``,
   ``abort``. the allocator gets memory from ``mmap`` and never gives any
   back; ``munmap`` exists, and using it would mean tracking which blocks
   came from which mapping and releasing one only when every block in it is
   free. a real allocator's job, and this is enough to build a compiler
   with. free blocks are kept on a single list **in address order**, which
   is the decision that earns its keep: coalescing needs neighbours
   adjacent in the list as well as in memory, and without it a program that
   allocates and frees in a loop ends up with thousands of blocks too small
   to use. that failure mode is invisible to a test that only allocates and
   frees, so most of ``tests/test_libc.c`` is about it.

``string.c``
   the usual set, with ``strdup`` and ``strcasecmp``. ``strncpy`` is here
   because ported code uses it, and the header says why nobody should reach
   for it first: it pads to ``n`` and does not always terminate.

the library is packed as an **archive**, and that is load-bearing rather
than tidiness: an object named on a link line is always included, a member
of an archive is pulled in only if it resolves something still undefined. so
a program that writes its own ``_start`` never drags ``start.o`` in and
never sees a duplicate, while one that writes ``main`` gets it, exactly
the rule that lets the old programs and the ported ones share a build. the
kernel's ``string.c`` and ``hash.c`` are compiled hosted into the archive
too, so there is one implementation rather than two.

``userland/wordcount.c`` is the demonstration: a ``main`` that returns an int,
``malloc``, ``printf``, ``strtol``, and not one line that names a syscall or
knows which kernel it is on.

arguments
---------

``args.c`` / ``args.h`` is one parser, and a program **declares what it
takes**:

.. code-block:: c

   static const struct opt cat_opts[] = {
       { 'v', "verbose", false, "name each file and its size before its contents" },
       { 'n', "number",  false, "number the lines" },
   };

that declaration is the only description of the program there is. the parser
reads it, and so does anything that has to explain the program to somebody,
which is what stops usage text from drifting away from what the code
actually does. ``help cat`` works by running ``cat --help``; the shell is
the kernel's, and velvet's ``docs/subsystems/shell.rst`` says how.

understood: ``-v``, ``--verbose``, ``-abc`` for three at once, a value as
the next word or stuck on or after an ``=``, and ``--`` to say everything
after it is a filename however much it looks like an option, which is the
only way to open a file whose name begins with a dash.

the programs
------------

``ls``, ``cat``, ``echo``, ``head``, ``wc``, ``grep``, ``sort``, ``cp``, ``mv``, ``rm``, ``mkdir``, ``rmdir``, ``touch``, ``chmod``, ``chown``, ``ln``, ``find``, ``less``, ``diff``, ``sha1sum``, ``whoami``, ``uptime``
   the toolbox. none of them are interesting alone and all of them are
   missed the first afternoon spent working *inside* the machine rather
   than on it.

``mem``, ``slabs``, ``ps``, ``vmm``, ``bt``, ``lspci``, ``disk``, ``parts``, ``cpus``, ``mount``
   the kernel's reports, which used to be its shell's commands. each
   prints one file of ``/proc``: ``meminfo``, ``slabinfo``, ``threads``,
   ``vmm``, ``stack``, ``pci``, ``disk``, ``partitions``, ``cpuinfo`` and
   ``mounts``. the kernel writes the text when it is read, with the same
   function its shell printed it with, so a program and that shell say the
   same thing. ``report.h`` reads a file in one go, because every read is
   a fresh writing of it, and a table read in two pieces could come out
   stitched from two moments. ``vmm`` and ``bt`` are root's, since they
   say where the kernel keeps things.

``dmesg``, ``date``
   two more of the kernel shell's commands. ``dmesg`` asks for the
   kernel's log with ``klogctl``, how big it is and then all of it, since
   the log is not a file. ``date`` turns the kernel's count of seconds
   into words with velvet's own date arithmetic, which this tree compiles
   into its library.

``margaret``
   the editor. the text lives in ``textbuf.c``, a separate file because it
   is the half that can be *tested*: an editor is a drawing loop wrapped
   around a data structure, and the structure is where a bug quietly eats
   somebody's file. **undo records operations, not copies**. keeping
   copies costs a whole file per keystroke, so each edit knows how to
   reverse itself, and forty mixed edits undone one at a time must arrive
   back at the original text byte for byte. the drawing is incremental,
   with dirty tracking as a *range* rather than a set, because every edit
   an editor makes is localised.

``gemini``
   fork, made visible; named for the twins. ``parent`` is the older, simpler
   demonstration that a program can start a program.

``tartarus``
   how deep can you go before the floor gives out. a stack-depth probe.

``hermes``
   the messenger: a program that speaks udp, over three syscalls.

``theodore``
   an attendant who waits: a server, which is what ``accept`` and
   ``listen`` are for.

``patience``
   a program that refuses to be hurried: the one ``alarm`` and an
   interruptible blocking call exist to make possible.

``hello``, ``counter``, ``fail``, ``reader``, ``ask``
   the early demonstrations. ``fail`` goes wrong on purpose so the exit
   code has somewhere to come from; ``counter`` links to the same addresses
   as ``hello``, which is what proved two programs could each have their
   own space.

``git``
   a git you can run in the machine. init, commit, log, diff, branch,
   checkout, over the object store in the kernel. see `the toolchain
   <toolchain.rst>`_.

the toolchain
-------------

the assembler, the linker, ``make``, ``ar`` and the image tools are not in
this directory. they are the programs the project builds for itself, and they
live in ``toolchain/``. their page is `the toolchain <toolchain.rst>`_.

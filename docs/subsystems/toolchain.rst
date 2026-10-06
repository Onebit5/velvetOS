the toolchain
=============

velvetOS builds its own kernel with its own tools. the assembler, the linker,
``make``, ``ar`` and ``mkext4`` live in ``toolchain/``, are built as host binaries
by ``make toolchain``, and are run by the kernel build. gcc is the only
program left in the chain that is not this project's, and 0.4.x is about
replacing that too.

the one thing that makes any of it checkable is comparing against the real
thing. an assembler cannot be eyeballed, a linker cannot be reasoned about
from its output, and a make that is subtly wrong looks exactly like a make
that is right until the file you changed does not rebuild. so every tool here
is checked against nasm, ``ld``, GNU make or GNU ``ar``, byte for byte, in
``make test``.

the assembler
-------------

the kernel has four files nasm builds and C cannot express: the isr stubs,
the context switch, the syscall entry, and the smp trampoline. a self-hosting
machine assembles those itself.

``tools/asmcheck.py`` assembles the same source both ways and compares. that
is stricter than it sounds, because nasm *chooses*: ``mov rax, 1`` assembles
to the five-byte ``mov eax, 1``, since writing a 32-bit register zeroes the
upper half anyway, so matching nasm means matching its choices between
valid encodings, not merely emitting something legal.

what it took: an encoder with memory operands, segment and control registers,
and prefixes; **branch relaxation**, because nasm picks the short jump when
the displacement fits and that choice changes the distance, so assembly runs
repeatedly until nothing lengthens; a **macro preprocessor**, since
``isr.asm`` is 256 interrupt stubs written once and expanded from 30 lines to
1058; and **ELF object output**, because a name the file does not define is
not an error but a relocation.

all four now produce objects indistinguishable from nasm's. every section
byte-identical, and the relocation counts matching exactly. the trampoline
took until 0.4.0 because it runs in **real mode**, and sixteen bits is a
different encoding rather than a smaller one: ModRM has its own table where
four registers appear in pairs, ``rm=110`` with ``mod=00`` is a bare address
rather than ``[bp]``, there is no SIB byte, a near call carries two bytes of
displacement instead of four, and the operand-size prefix means the opposite
of what it means everywhere else. the comparison found three things there
that are not addressing at all. ``mov eax, cr4`` read ``cr4`` as an
undefined symbol; ``lgdt`` and ``lidt`` were told apart by the wrong letter
of their own mnemonics, so every ``lgdt`` was an ``lidt``; and ``align``
padded with zeros where nasm pads with nops.

the comparison compares the *object* as well as the bytes now: relocations
grouped by the section they patch, and the exported symbols by name and
section. that is what caught ``global`` being accepted and ignored (every
exported symbol came out ``LOCAL``) and every relocation going into
``.rela.text`` whatever section it patched. sections are compared by *name*
rather than index. nasm numbers ``.rodata`` 2 and this numbers it 6, and
both are right.

the linker
----------

the assembler's answer to a name it did not know is a **relocation**: a note
saying "the four bytes here are a reference to this name, whoever provides
it". ``toolchain/linker.c`` is whoever provides it. three jobs:

- **placement**: every object arrives claiming its sections start at zero,
  and the linker makes that a real address.
- **resolution**: a name defined in one object and used in another; two
  definitions is one error, no definitions is a different one, and saying
  which is most of what makes a linker usable.
- **relocation**: ``R_X86_64_64`` writes ``S + A``; ``R_X86_64_PC32`` writes
  ``S + A − P``, a distance. the assembler sets ``A`` to −4 so the distance
  is measured from the end of the instruction, which is where the processor
  measures it from.

and it produces an ELF, the first time this machine has written one rather
than only read one. program headers, an entry point, and section headers so
the result can be disassembled. a linker whose output ``objdump`` cannot read
is one you can only debug by running it, which for kernel code means by not
booting.

``tools/linkcheck.py`` links the same objects both ways and compares what
actually gets *loaded*. what byte ends up at each address, and what each
address may be done to, not the file, because how many ``PT_LOAD`` segments
a linker uses is a decision and comparing decisions is comparing opinions. four
things that settled:

- the gaps between sections are not zeros in code: ``ld`` fills alignment
  padding in executable sections with multi-byte nops, because ``00 00``
  decodes as ``add [rax], al`` and a gap of zeros is a stretch of code that
  faults if anything runs off the end of a function.
- a **segment is not a section**: sections are what the linker thinks in,
  segments are what the loader thinks in, and the loader only cares about
  permissions.
- two sections sharing a page cannot be given different permissions, so the
  script's ``. = ALIGN(4096)`` between sections is load-bearing rather than
  tidy, and a script that forgets it gets one writable executable segment
  instead of the protection it thought it asked for.
- ``.bss`` is an address and a length and a promise, not bytes. a linker that
  writes it out anyway turns the kernel's zeroed globals into megabytes of
  file.

two fixture scripts, deliberately: ``simple.ld`` packs everything into one
page and ``paged.ld`` aligns each section onto its own. a linker that always
started a new segment per section matched ``ld`` on one of them and not the
other, which is the only reason the wrong rule was caught.

0.4.0 taught it the rest of a real script. ``PHDRS``, symbol assignments
like ``__text_start = .``, ``KEEP``, ``/DISCARD/``, which the kernel's own
script uses. the change underneath is that a script is read as a *sequence*
rather than a table: ``__text_start = .`` is a name whose value is wherever
the location counter had reached, and ``. = ALIGN(CONSTANT(MAXPAGESIZE))`` is
the counter pushed forward by an amount depending on what came before.

it reads **archives** too: an object named on the link line is always
included, a member of an archive only when it resolves something still
undefined. members must be taken *one at a time*, with the name table worked
out again in between, because taking every member that answers something in
one sweep takes the second definition of a name as well as the first and then
fails the link blaming the archive.

and it writes a symbol table, which an executable does not need and this
kernel does: ``gensyms`` reads the linked kernel's own symbol table to build
the table a panic symbolises its backtrace from. getting that to agree with
``ld`` needed one rule readable only from ``nm``. a name the script assigns
belongs to the *next* output section placed, or the last one if the script
ends first.

make
----

building by typing commands in order works exactly until the first time you
change one file. then you either retype all of them and wait, or retype some
of them and be wrong, and the second is worse, because being wrong does not
look like being wrong. it looks like a build that worked, with a binary in it
that was never rebuilt.

``toolchain/make.c`` is a graph, a timestamp comparison, and a small language. the
graph is walked depth-first and notices cycles. the language is variables
(both assignment kinds), pattern rules, the automatic variables, ``.PHONY``,
and fourteen of the text functions. the timestamp comparison is one line and
it is the whole idea.

it is checked against GNU make **edit by edit**: a fixture is a makefile and a
sequence of edits, replayed against both, comparing every command, message,
exit status and the directory left behind. the edits are the point. a make
that gets a cold build right and an incremental one wrong is the *normal*
kind of broken, and only the second run of a fixture ever shows it.
timestamps are set rather than taken from the clock, so there is no ``sleep``
anywhere and no fixture that passes on a fast machine and fails on a slow
one.

the rest of a real makefile's vocabulary arrives in 0.4.0: ``include`` and
``-include`` (the dependency files a compiler writes do not exist the first
time round), the four conditionals (a stack, with two flags per level,
whether this branch is being read, and whether any branch of this
conditional has been, without which an ``else`` after a branch that was taken
would be taken too), ``$(shell)`` (trailing newlines dropped, the rest turned
into spaces, so a command printing one path per line hands back a list of
words), and ``::``. the double-colon one had the bug the fixture caught
immediately: judging each rule as it ran let the first recipe's output make
the second look up to date, so a target with two rules and neither file
present built half of itself. all rules are judged against the state the
target was in *before any of them ran*.

ar
--

``toolchain/ar.c``, because ``libc.a`` is an archive and there was no program here
that could write one. the format is barely a format. ``!<arch>\n``, then
sixty bytes of header per member with every field in *text*, and two members
that are apparatus rather than files: ``/`` is the symbol index and ``//`` is
where long names live.

``tools/archeck.py`` compares byte for byte and then links the same program
four ways. ours and ``ld`` over each of the two archives, because an
archive is the easiest format here to get *nearly* right, and the danger is
not a crash but a file only this machine can read wearing a name everybody
else's tools recognise. either direction alone would pass on a private
format. byte-identical took two details only a comparison finds: the index
member rounds *its own size* up to an even number to keep the next member
aligned where every other member is followed by a pad byte outside its size,
and the name table leaves date, owner and mode blank rather than writing
zeros.

mkext4
------

``tools/mkext4.c`` is the sixth tool, and not a rewrite so much as a driver:
``mkfs.c`` already formats ext2 and ``ext4.c`` already writes files, so what
was missing was a way to point them at a host file and a host directory. it
replaced 460 lines of python that laid out ext2 a second time.

hashes and deflate
------------------

``kernel/lib/hash.c`` and ``lib/deflate.c`` are the kernel's, but they
are prerequisites for git rather than features of the kernel: a git
repository is addressed by the sha-1 of what is in it, and every object on
disk is a zlib stream. see `the library <lib.rst>`_ for what each sum is
for, and ``tools/hashcheck.py`` / ``tools/zlibcheck.py`` for how they are
checked (against python, zlib and git itself, including the case that cannot
fail. a single buffer hashed in one go, which is why every input is fed
again in chunks of 1, 7, 63, 64 and 65 bytes).

git
---

an **object's name is its hash**. the sha-1 of the content is the only name
the content has, and where the file goes is worked out from that name. two
identical files are one object; a file cannot be edited without becoming a
different object; there is no such thing as a corrupt object that still looks
valid, because checking *is* rehashing.

``userland/gitobj.c`` writes and reads all three shapes. blobs, trees and
commits, as loose objects, compressed with deflate and named with sha-1.
``tools/gitcheck.py`` has our code write a repository from scratch and hands
it to real git: ``fsck --strict`` accepts it, ``log`` walks it, ``status``
reports the tree clean, and the tree hash is compared with what ``git
write-tree`` computes.

two details in the format are unforgiving and caught only by that comparison:
a tree is **binary** (entries hold the twenty raw bytes of a name, not the
forty characters), and the **sort order is not ``strcmp``** (a directory
sorts as though its name ended in a slash, so ``lib.c`` comes before ``lib/``,
while ``strcmp`` puts ``lib`` first because it is a prefix. one entry out of
place changes the tree's hash and every hash above it).

``userland/git.c`` is the half that makes it usable: the same ``gitobj.c`` with
velvetOS's syscalls underneath instead of the host's stdio, behind
``gitio.h``. five functions with ``gitio_host.c`` and ``gitio_velvet.c``
under it. two libcs disagreeing is what the port cost: this machine's
formatter does not take ``%zu``, so the three things the store needed are
written out longhand. there is **no index**, and that is a decision rather
than a gap, without one, ``commit`` records the whole working tree, so
nothing to forget to stage, at the cost of no ``add`` and no partial commits.
``checkout`` is the same tree walk as ``diff`` with its printing turned off,
so "what changed" and "is there anything to lose" cannot disagree, which is
the only reason it is safe to refuse on the strength of it.

the self-hosted kernel
----------------------

``make selfhost`` builds the kernel with this project's make, assembler and
linker. ``tests/toolchain/kernel.mk`` is a real makefile rather than a
demonstration; it finds its sources with ``$(shell find)``, reads the
dependency files gcc writes with ``-include``, and runs the same two-pass
symbol-table dance the ordinary build does.

``tools/selfcheck.py`` compares the loaded image of both kernels byte by
byte, and the only differences are the six bytes of ``__TIME__`` that say
which second each was compiled in. the check allows no *budget* of
differences; it names what each one is, and a difference that is not a
clock is a failure.

it is a separate makefile rather than a switch in the GNUmakefile on purpose:
the two builds have to run side by side and be compared, and a build that can
only be had by turning the other one off cannot be. see `self-hosting
<../building.rst>`_ for the targets.

what is not ours in that image: philemon. a bootloader is sixteen-bit code
with ``a32`` prefixes and unreal mode underneath it, which is the
assembler's next mile rather than this one, so the boot sector, the ramdisk
and the source archive come from the ordinary build, and what is claimed is
the kernel.

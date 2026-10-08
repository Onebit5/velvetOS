# where this is going

this is the system's roadmap: the programs, the library they link, the
toolchain that builds the machine, and what goes on its disk. the kernel
under all of it is velvet, in `kernel/`, and the kernel's work is
in the kernel's roadmap.

until 0.4.0 the two were one tree with one roadmap, and the entries here
are the ones whose code is the system's, under the numbers they shipped
as or were planned as. the ones whose code is the kernel's went to
velvet along with it, so the numbers have gaps, and every gap is a
version the kernel's roadmap holds. an entry that was about both is in
whichever roadmap holds most of it, or split between the two where its
paragraphs split cleanly. some entries here wait on one there, and say
which.

what follows is a lot of small steps rather than one heroic one. each
line below should be a version on its own -- buildable, bootable, and
worth a paragraph in the changelog.

0.4.x is the one everything else is for: a machine that can rebuild
itself from its own disk. after it, 0.5.x is about who the machine
belongs to, 0.6.x about software it did not write, and 0.7.x about
putting it on disks that belong to somebody else.

from 0.4.0 the system is released on its own, and its numbers are its
own. the planned entries keep the numbers they were given while the plan
was one plan, so a release here may skip a number the kernel holds.

## 0.1.x: filling in what having processes implies

**0.1.4 a userspace toolbox.** ~~`cat`, `echo`, `uptime` as real programs
in `base/ramdisk/bin` rather than kernel commands. the kernel shell keeps
only what genuinely needs kernel access -- `vmm`, `bt`, `hexdump`, `ps`
-- and everything else moves out. the point is to find out which
commands were secretly using kernel internals.~~ **done in 0.1.4**, and
the answer was: `echo` needed only argv, `cat` and `uptime` needed
nothing that did not already exist, and `ls` needed one new syscall
because `open` can only answer about a name you already know.

## 0.2.x

**0.2.5 arguments worth parsing.** ~~every program parses argv by hand
and none of them agree. a small getopt in the user library: `-v`,
`--verbose`, clustering, `--` to stop parsing, and a usage string each
program declares once and never writes out twice.~~ **done in 0.2.5.**

**0.2.6 help that knows what it is describing.** ~~because 0.2.5 makes
each program declare its arguments, `help cat` can print them without
anybody writing that twice. `--help` on any program prints the same
thing, from the same place.~~ **done in 0.2.6**, and the shape of the
answer settles the question: `help cat` *runs* cat with `--help`, so
there is no second copy anywhere that could drift. the plain `help`
became names in columns, since a description beside every one of them
was a wall you had to read all of to find the line you wanted.

**0.2.9 an editor, and redirection.** ~~something nano-shaped, so a file
on the disk can be changed by the machine that stores it rather than by
rebuilding the image. the first program that has to think about a screen
rather than a stream.~~ **done in 0.2.9.** the editor is `margaret`,
after the one who keeps the compendium -- the only thing in the velvet
room that is written down.

## 0.3.x: the machine stops being alone, and becomes somewhere to work

the first six of these finish the wire. the rest are the answer to a
different question, and it is the question 0.4.0 asks: **what does a
machine need before it can build itself?**

that is not a rhetorical framing. every entry from 0.3.7 down is
something the compiler in 0.4.0 will not run without -- a libc to link
against, an assembler for the parts C cannot express, a linker to make
an image, a make to drive it, and a place to keep the source that
survives a reboot. none of them are the compiler, and all of them have
to exist first.

### a machine somebody could work on

**0.3.8 a libc worth linking against.** ~~every program in `userland/` includes
`syscall.h` and writes its own everything. a compiler cannot be written
that way. `malloc`, `stdio`, `string`, `printf` -- and the moment those
exist, a program ported from anywhere else has a chance of building,
which is the actual test of whether this is a unix or an impression of
one.~~ **done in 0.3.8**, and the smallest file in it is the one that
answers the question: `start.c` defines `_start`, calls `main`, and
exits with what it returned. every program here used to be entered at
`_start` and had to call `exit` itself, which no program written
anywhere else does.

the library is an *archive* and that turns out to be load-bearing rather
than tidy: an object on a link line is always included, an archive
member only when it resolves something undefined. so a program with its
own `_start` never drags start.o in, and one with a `main` gets it.
linking the objects directly broke every existing program at once.

the headers are on the include path, so a program writes `#include
<stdio.h>` and means this machine's. `userland/wordcount.c` is the proof:
a `main` returning an int, `malloc`, `printf`, and not one line naming a
syscall.

**0.3.10 margaret, grown up.** ~~files bigger than a screenful of memory,
undo, more than one buffer, and search that does not start again from
the top. the editor is the tool the whole of 0.4.0 is used through, and
an editor you fight is a compiler you never finish writing.~~ **done in
0.3.10.** the text is in `userland/textbuf.c`, separately from the drawing,
because that is the half that can be tested -- and undo has a property
nothing else here does: forty mixed edits, undone one at a time, must
arrive back at the original text byte for byte.

still missing: cut and paste, which the old margaret had. the textbuf
has no cut buffer, and the shortcut rows do not advertise keys that do
nothing.

**0.3.11 the small tools.** ~~`less`, `diff`, `find`, `wc -l` on a
thousand files. none of them are interesting alone and all of them are
missed the first afternoon spent working inside the machine rather than
on it.~~ **done in 0.3.11.** `diff` got a tested core, because the
property that matters is checkable exactly: its edits, applied to the
first file, must give the second.

### the toolchain, minus the compiler

**0.3.14 an assembler.** ~~the kernel has four files nasm builds and C
cannot express: the isr stubs, the context switch, the syscall entry, the
trampoline. a self-hosting machine assembles those itself. a subset of
x86-64 -- the instructions this kernel actually uses -- is a few hundred
opcodes and an enormous amount of table.~~ **done in 0.3.14 for three of
the four.** `switch.asm`, `syscall.asm` and `isr.asm` build to objects
indistinguishable from nasm's -- every section byte-identical, relocation
counts 0, 1 and 257 against nasm's 0, 1 and 257.

`trampoline.asm` assembles and diverges at byte 22, on **16-bit
addressing mode**: in real mode ModRM is a different table, `rm=110`
means disp16 and there is no SIB. that is the one thing left, and it is
a second encoding path rather than a missing instruction.

the version needed two things the entry did not mention. a **macro
preprocessor**, because `isr.asm` is 256 stubs written once and there is
no file to assemble without expanding it; and **ELF object output**,
because two of the four call into C -- a name the file does not define
is not an error but a relocation, which is the assembler's whole
contract with 0.3.15.

and the differential test against nasm is the version's real deliverable.
six bugs came out of it that reading the code never would have shown,
the worst being a local `dq label` resolved to its offset when it must
be a relocation: an offset within a section is not an address.

**0.3.15 a linker.** ~~relocations, symbol resolution, and a script that
says where sections go. the kernel already has a linker script, so the
shape of the answer is written down; what is missing is the program that
reads it. this is also the first time this machine will have to *produce*
an elf rather than only consume one.~~ **done in 0.3.15.** placement,
resolution, `R_X86_64_64`/`PC32`/`PLT32`/`32`/`32S`, a script reader that
takes the entry symbol, the base, the section order and `ALIGN`, and an
ELF with program headers, an entry point and section headers. checked
against `ld` by `tools/linkcheck.py`, which compares what gets *loaded* --
the byte at each address and the permissions each address ends up with --
rather than the file, since segment division is a decision rather than a
result.

the comparison earned its keep four times. executable padding is
multi-byte **nops** and not zeros, because `00 00` is `add [rax], al` and
a gap of zeros faults; a **segment is not a section**, and `ld` groups by
what the loader cares about, which is permissions; two sections **sharing
a page cannot have different permissions**, so the split needs both a
permission difference and a page boundary -- which is what makes the
script's `ALIGN` load-bearing; and **`.bss` is not bytes**. the fixture
set has a packed script and a page-aligned one for exactly this reason: a
segment per section matched `ld` on one and not the other.

what it does not read is the rest of a real script -- **`PHDRS`, symbol
assignments** like `__text_start = .`, **`KEEP`** and **`/DISCARD/`** --
and the kernel's own script uses all four, the symbol assignments
load-bearing since the vmm maps each section from them. that is what
stands between this and linking the kernel, and it is the next step
rather than a missing one.

**0.3.16 make.** ~~a dependency graph, timestamps, and a small language.
building anything by typing commands in order works exactly until the
first time you change one file, and the whole of 0.4.0 is a hundred and
thirty files.~~ **done in 0.3.16.** the graph with its cycle check, the
timestamp comparison, variables of both kinds, pattern rules, the
automatic variables, `.PHONY`, `-n`, `-s`, `-f`, and fourteen text
functions. checked against gnu make by `tools/makecheck.py`, which
replays a *sequence of edits* against both and compares every command,
message, exit status and the directory left behind -- the edits being
the point, since a make that gets a cold build right and an incremental
one wrong is the normal kind of broken.

eleven breaks, eleven caught, four of them only after the **fixtures**
were fixed: equal timestamps had no way of occurring, a file-backed
diamond absorbed a target built twice, no phony target had a file of its
own name beside it, and no pattern rule was asked for a `.o` with no
`.c`. and two things about gnu make i would not have guessed -- the
prerequisites of the rule carrying the recipe come first, and
`MAKELEVEL` in the environment makes it announce directories, so the
check passed by hand and failed inside `make test`.

`make toolchain` then builds an executable with **our make, our
assembler and our linker**, with no gcc, nasm, ld or gnu make in it, and
compares the result against nasm and ld's -- `.text` and `.rodata`
identical. that found three assembler bugs a `.text` comparison never
could, since none of them changes a byte of it: `global` accepted and
ignored so exported symbols came out local; every relocation filed under
`.rela.text` whatever section it patched, which a *count* of relocations
cannot see; and a `dq label` relocated against the name or the section
depending on whether the label was below or above the line. `asmcheck`
now compares the object as well as the bytes, and the three real kernel
assembly files are in `make test` rather than in a testing document.

what it does not have: `include`, `ifeq`, order-only prerequisites,
target-specific variables, command-line overrides, `$(shell)`, chained
implicit rules, and `-j`. the last of those is the interesting one and
is not a small addition -- a build that runs four commands at once needs
the graph to say which four, which is a different walk.

**0.3.19 a git-compatible object store.** ~~blobs, trees and commits, named
by their hashes, written where git would write them. the compatibility
is the point: a repository this machine writes should be one a real git
can read, which is the only way to know it is right rather than
self-consistent -- the same argument that made `tools/readext2.py` worth
having.~~ **done in 0.3.19.** `userland/gitobj.c` writes and reads all three
as loose objects, compressed with 0.3.18's deflate and named with
0.3.17's sha-1 -- both written before anything needed them.

`tools/gitcheck.py` builds a repository from nothing with our code and
hands it to real git: **`fsck --strict` accepts it, `log` walks it and
prints the message, `status` is clean against the commit**, and the tree
hash ours computes for a directory equals what `git write-tree` computes
for the same one.

the two unforgiving details are both invisible to reading: a tree holds
the **twenty raw bytes** of a name rather than the forty characters, and
the sort order is **not strcmp** -- a directory sorts as though its name
ended in a slash, so `lib.c` comes before `lib/`.

and the finding worth keeping: breaking that comparator in **one of its
two directions** was caught by the unit test and missed entirely by git,
because insertion sort only ever consulted the direction still working.
a half-broken comparator hides behind the access pattern of whatever
sorts with it.

what is not here is anything that *uses* the store -- no index, no
branches, no `log` of our own, and no ring 3 program yet. that is
0.3.20, and the store is the half that had to be right first.

**0.3.20 the rest of git.** ~~an index, branches, `log`, `diff`,
`checkout`. enough to work: make a change, see what changed, record it,
and go back to what was there before.~~ **done in 0.3.20**, and with the
part the entry did not ask for: **it runs in ring 3**. `init`, `hash-object`, `cat-file`, `write-tree`,
`commit`, `log`, `diff`, `branch` and `checkout`, from the same
`gitobj.c` the host checks drive, over a five-function seam
(`userland/gitio.h`) with velvetOS's syscalls under it. the entry's own test
-- make a change, see what changed, record it, go back to what was there
before -- passes end to end.

what the port cost was the two libcs disagreeing. `snprintf` with `%zu`
is a header this machine's formatter writes literally -- an object whose
name nothing else agrees with, silently -- so the store formats by hand
now. and `epoch.c` turned out to be pure arithmetic bolted to
`epoch_now()`, which wants the timer chip; the seam answers
`pit_uptime_ms` and says why. **splitting epoch.c properly is owed.**

**no index**, deliberately: it is a binary format with its own version
number and its own compatibility risk, and without one `commit` records
the whole working tree -- no staging, nothing to forget to stage. the
cost is no `add`, no partial commits, and a real git disagreeing about
`status` until it builds an index itself.

`diff` walks the commit's tree against the working tree and hands each
changed pair to `userland/difflib.c`, which has been sitting there since
0.3.11 waiting for a second caller. **`checkout` uses the same walk with
its printing turned off** -- so "what changed" and "is there anything to
lose" can never disagree about what a change is, which is the only
reason it is safe to refuse on the strength of it. and it does refuse:
it is the first thing in this project that can destroy something a
person typed, so it stops and says `git diff` will show you, rather than
overwriting and being sorry.

branches are a file with forty characters in it and HEAD is a line of
text naming one, which is why making a branch is instant however large
the history is, and why a detached head is not a special case but simply
HEAD holding a commit instead of a name.

## 0.4.x: the machine builds itself

**offline self-hosting.** edit the kernel, compile the kernel, and record
the change, without leaving velvetOS.

the test is not a feature list, and it is worth stating as the thing it
actually is: **if the machine this was developed on died tomorrow, and
all that survived was a velvetOS disk and a C compiler, the project could
continue from that disk.** everything in 0.3.x is chosen because that sentence is not
true without it.

what stands between here and there is what the sections below describe,
and it is no longer a compiler. this machine is built by gcc, the same
gcc that built the disk it was developed on, and the toolchain that is
ours is the one around it: the assembler, the linker, make and ar, each
checked against the GNU one it replaces. writing a C compiler would mean
the kernel could only use the C that compiler had learned, which is a
hostage rather than a tool, and nothing above needs one.

**0.4.0 the last of the toolchain.** ~~four things 0.3.x named as owed
and then left, none of them a compiler and every one of them in the way
of one. `trampoline.asm` still diverges from nasm at byte 22, because
real mode's ModRM is a different table and that is a second encoding
path. the linker does not read `PHDRS`, symbol assignments, `KEEP` or
`/DISCARD/`, and the kernel's own script uses all four. the linker does
not read an *archive* either, and there is no program here that writes
one -- 0.3.8 made "an object on the line is always included, a member
only when it resolves something" load-bearing, so a linker without that
rule links a different program than the one asked for. and make wants
`-include` for the `.d` files, `ifdef`, `$(shell)` and `::`, which is
what this project's own GNUmakefile asks for in its first forty lines.
the version ends when our make, our assembler and our linker build the
kernel out of gcc's objects and it boots -- everything replaced except
the compiler, which is the only honest way to find out that the rest of
the toolchain is finished.~~ **done in 0.4.0, and the kernel that came
out is the same bytes ld's is.**

the trampoline is byte-identical to nasm now, and sixteen bits turned
out to be five encoding differences rather than one: the ModRM table,
the accumulator's own short forms, a near branch carrying two bytes of
displacement instead of four, the operand-size prefix meaning the
opposite of what it means elsewhere, and `align` padding with nops in
every section rather than zeros. the comparison also found `lgdt` and
`lidt` being told apart by the wrong letter of their own mnemonics, so
every `lgdt` this assembler had ever emitted was an `lidt`.

the linker reads a script as a **sequence** now rather than as a table,
which is what `__text_start = .` needs and what everything else in that
list came out of: with a location counter in hand, `PHDRS` and
`/DISCARD/` are a few lines each. archives are read, `toolchain/ar.c` writes
them byte-identically to GNU ar's, and members are taken *one at a
time* with the name table worked out again in between -- taking every
member that answers something in one sweep takes the second definition
of a name as well as the first, and then the link fails blaming the
archive. and it writes a symbol table, because `gensyms` reads one: a
kernel linked without it boots, works, and cannot say where it died.

make gained `include`, `-include`, the four conditionals, `$(shell)` and
`::`, and one number: 512 rules was enough for a fixture and this
kernel's dependency files come to 878 of them. two of those four are
what the GNUmakefile actually asks for -- `$(shell)` seven times and one
`-include` -- and the entry above said all four were in its first forty
lines, which they are not. the conditionals and `::` are here because a
build with two of anything in it needs the first and a target with two
reasons to be rebuilt needs the second, and both are checked the way
everything in make is: a fixture replayed against gnu make, edit by
edit.

what is checked, and by whom: `make kernelcheck` links the real kernel
with ours and with ld and compares every byte at every address;
`make archeck` builds an archive both ways and links a program with the
two crossed over; `make selfhost` builds the kernel with our make, our
assembler and our linker, and holds it against the ordinary build --
identical, except for the six bytes of `__TIME__` that say which second
each was compiled in. `make selfboot` boots the image that comes out.

what is still not ours in that image: philemon, which is sixteen-bit
code with `a32` prefixes and unreal mode in it -- named here rather than
glossed, because the claim this version makes is about the kernel. and
the linker does no **string merging**: a `.rodata.str1.1` marked
SHF_MERGE is copied rather than deduplicated, which costs a user program
built at `-O1` about three bytes and costs this kernel nothing at all,
since it is compiled without optimisation and has no mergeable section
in it.

**0.4.1 the disk that continues the project.** what is left over is not
a compiler, and none of it was ever going to be. a **ustar writer**, named at 0.3.26 as the gap: this machine
reads two archives and can write neither, and `tar` lives on the
development machine. `srcstamp` in C, since 0.3.26 left that one in
python and a source stamp nobody can recompute is a claim rather than a
check. with those, and `mkboot`, `mkfat`, `mkext4`, `bin2c` and `gensyms`
already being C since 0.3.21, an installed velvetOS can write a whole
`velvetos.img` and put it on a second disk. then the version is the test
performed rather than described: a machine with nothing on it, a velvetOS
disk, one line of the kernel changed in margaret, `make`, install,
reboot into the change, and `git commit` with the git that has been here
since 0.3.20. no second machine at any point in that sentence.

**what will still be missing, and is worth naming now.** no optimiser of
our own, which is not a loss: gcc optimises the userland already, and the
kernel is built without it for reasons that would not change if we had
one. no debugger, so a fault is read the way faults have been read here
since the beginning: a backtrace, a symbol, and the source that is now
sitting on the same disk.

**0.4.2 installing.** 0.4.1 gives the machine the pieces to write an
image; this is the program that does it, onto a disk that is not the one it
booted from: a partition table, philemon in the first sector, the kernel,
the ramdisk and the source, written by velvetOS on the machine being
installed. the development host is not in this sentence, and neither are
`mkboot`, `mkfat` and `mkext4`. *ends when a blank disk goes in beside the
system disk and comes out booting on another machine.*

**0.4.4 the machine replaces its own kernel.** build, link, install and
offer a reboot, from inside velvetOS, in place: the source has been on the
disk since 0.3.26 and everything that reads it is already ours. *ends with
the test 0.4.1 describes performed for real -- one line of the kernel
changed on the machine itself, `make`, install, reboot into the change, and
the commit made by the git that lives here.*

## 0.5.x: a machine for more than one person

0.4.x is about what the machine can do. this is the first series about
who it belongs to.

**0.5.3 grant.** a setuid bit on the filesystem -- nothing in this tree
knows what one is -- and a program called `grant` that uses it. **it is
the first program here that is meant to be a hole in the wall**, which
means the version is not "does it work" but "is the hole exactly the
size it claims". a configuration in `/etc/grant.conf` saying who may
become whom, and a way to prove who is asking.

the environment is where this goes wrong for everybody who writes one,
and this machine handed programs an inherited environment block in
0.2.18 and made `$PATH` mean something in the same version. a `grant`
that passes those through unexamined is a hole the size of the wall. the
test is the one the entry is for: a user edits a system file without
becoming root for the rest of the session.

**0.5.5 an installer that asks.** 0.2.21 was a sequence of commands;
this is an interface, drawn with the escape sequences 0.3.9 taught the
console to understand. it asks for the hostname, the first user, their
password and whether they are an admin -- which is now a real question,
because 0.5.1 made a group something a person can be in. the
two-partition layout from 0.3.25 stops being two commands and becomes a
default.

what a conversation adds that a script did not have is somebody changing
their mind half way through, so this is the first thing here that has to
be *cancellable*: 0.2.21's ordering rule is still underneath it -- the
partition table cannot be written before the copy that would erase it,
and philemon's first stage ends at byte 365 to leave room for it -- and
every step before the one that commits has to leave the disk as it was.

## 0.6.x: the machine runs software it did not write

0.4.x made the machine able to build itself and 0.5.x made it belong to
somebody. this one is about the software that did not come from here.

every program in this image was written against this machine, by
somebody who could change the machine when it was easier than changing
the program -- which is the most comfortable position in software and
the one that hides the most. a program written somewhere else was
written against a unix that already exists, and it will not be polite
about the difference. that is the outside answer the whole series rests
on: the same argument as `readext2.py` and `makecheck.py`, at the scale
of somebody else's entire program.

**0.6.3 a shell in ring 3.** 0.2.18 ended its blocks with `end` rather
than `fi` and `done`, deliberately, on the grounds that a shell that
looks like sh and is not is worse than one that plainly is not. this is
the other side of that argument: a real `sh` as a program -- quoting,
`&&`, `||`, subshells, `$( )`, functions, and job control through
0.6.0's syscalls rather than through kernel internals. the kernel shell
stays exactly as it is and becomes the rescue shell, which is a better
outcome than replacing it: one shell for when the disk is broken, one
for work.

**0.6.4 lua.** the first program in this image that the author did not
write. it is chosen for three properties and not for what it does:
self-contained C, no dependencies worth the name, and a test suite that
came with it. every hole in libc that a hand-written program never
reached is on the other side of that suite.

**0.6.5 the libc the port asked for.** whatever 0.6.4 turned out to need
-- `qsort`, `setjmp`, buffered `FILE*`, the time functions -- driven by
a real demand rather than by imagining one. `strtod` and `%f` wait on
the kernel's half of this entry: floating point in ring 3 needs fpu
state in the context switch, and there is none yet.

**0.6.6 sqlite.** the amalgamation, and its own test suite pointed at
this machine. a database is the most demanding thing anybody points at a
filesystem, and its durability tests are an independent audit of what
0.3.23 and 0.3.24 claimed about crashes and the journal -- written by
people who have never heard of this machine, which is the only kind of
audit worth having.

**0.6.7 dynamic linking.** `.so` files, PLT and GOT, a loader, and a
`libc.so`. it needs position-independent code, which
0.5.8 already wanted for the program base, and it pays for itself where
this project is tightest: the ramdisk currently carries a copy of
`printf` per program, in an image whose size 0.4.6 had to do arithmetic
about.

**0.6.10 a remote shell.** 0.5.x said ssh needs the primitives and a pty
layer and that both were somewhere else; 0.5.6 and 0.6.8 are those. the
value is the same as every other port here: the test is somebody else's
client, speaking a protocol we did not design, against a server that has
to be right about every byte of it.

**0.6.11 a package, and somewhere to put it.** once software arrives
from elsewhere it has a name, a version, a signature and a list of what
else it needs. ustar with a manifest -- the archive format this machine
already reads twice over -- signed with 0.5.6's primitives, named with
0.3.17's sha-1, and a `/usr` for it to land in. the point is not
convenience: it is that software from outside should arrive the same way
every time, and be removable, or the disk becomes a place things are
poured.

**0.6.13 the machine fetches its own source.** git in this image can commit
and cannot reach anything, and 0.6.10 proved the primitives. this is dns, a
route off the one segment, the ssh that is already here or a tls that the
port brings with it, and packfiles at both ends: `clone`, `pull` and `push`
against a real remote. the pull request stays somebody else's problem,
which is the point -- a push is a ref, and a browser is not required to
open one. *ends when this machine clones velvetOS from where it is hosted,
builds it, pushes a branch back, and the pull request is opened from a
phone.*

**0.6.14 the terminal world.** ncurses, and the terminfo database under it,
which is one port standing in front of every full-screen program written
since 1980: editors, pagers, mail (`mutt`, and `msmtp` to send it), irc, and
`top`. *ends when an editor and a mail client, built from source that
arrived over the wire in 0.6.13, both open on this machine.*

## 0.7.x

**0.7.8 an installer that could destroy something.** every disk this
machine has ever formatted was made for it. on somebody's desk the disks
are theirs, and one of them has photographs on it. so: name each disk by
model and serial rather than by number, refuse to have a default, say in
words what is about to be lost and require it to be typed back, and have
a dry run that writes nothing. it is the only entry in this series that
is not about hardware -- it is about what hardware means.

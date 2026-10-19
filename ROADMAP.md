# where this is going

velvetOS's plan, from 0.4.0 on: the programs, the library they link, the
toolchain that builds the machine, the accounts and the people on it, and
what goes on its disk. the kernel under all of it is velvet, which this
tree builds from the release named in `kernel-release.mk`, and the
kernel's work is in velvet's roadmap. where an entry here needs the kernel
to do something new, the kernel's half is an entry there, and this one
says which.

until 0.4.0 the two were one tree with one roadmap, and what was done
before then is in CHANGELOG.md, under the numbers it shipped as. what
follows is a lot of small steps rather than one heroic one. each entry
below should be a version on its own: buildable, bootable, and worth a
paragraph in the changelog. the numbers are velvetOS's own and run without
gaps.

0.4.x is the one everything else is for: a machine that can rebuild
itself from its own disk. after it, 0.5.x is about who the machine
belongs to, 0.6.x about software it did not write, 0.7.x about putting it
on disks that belong to somebody else, and 0.8.x about a machine that can
say what went wrong.

## non-goals

- **being useful in any practical sense.** this is a system built to be
  understood, small enough to fit in one head.
- **a C compiler of our own.** the toolchain that is ours is the one
  around gcc: the assembler, the linker, make and ar. 0.4.x says why the
  compiler is not on that list.
- **a kernel in this tree.** the kernel is velvet, released on its own.
  this tree builds the release it names and nothing else, so a change to
  the kernel is velvet's, and reaches the system when the release it is
  in does.

## 0.4.x: the machine builds itself

**offline self-hosting.** edit the kernel, compile the kernel, and record
the change, without leaving velvetOS.

the test is not a feature list, and it is worth stating as the thing it
actually is: **if the machine this was developed on died tomorrow, and all
that survived was a velvetOS disk and a C compiler, the project could
continue from that disk.** everything in 0.3.x is chosen because that
sentence is not true without it.

what stands between here and there is what the sections below describe,
and it is no longer a compiler. this machine is built by gcc, the same
gcc that built the disk it was developed on, and the toolchain that is
ours is the one around it: the assembler, the linker, make and ar, each
checked against the GNU one it replaces. writing a C compiler would mean
the kernel could only use the C that compiler had learned, which is a
hostage rather than a tool, and nothing above needs one.

**0.4.2 the disk that continues the project.** what is left over is not a
compiler, and none of it was ever going to be. a **ustar writer**, named
at 0.3.26 as the gap: this machine reads two archives and can write
neither, and `tar` lives on the development machine. `srcstamp` in C,
since 0.3.26 left that one in python and a source stamp nobody can
recompute is a claim rather than a check; the tool is the kernel's, so
that half is a change to velvet. with those, and `mkboot`, `mkfat`,
`mkext4`, `bin2c` and `gensyms` already being C since 0.3.21, an installed
velvetOS can write a whole `velvetos.img` and put it on a second disk.
then the version is the test performed rather than described: a machine
with nothing on it, a velvetOS disk, one line of the kernel changed in
margaret, `make`, install, reboot into the change, and `git commit` with
the git that has been here since 0.3.20. no second machine at any point in
that sentence.

**what will still be missing, and is worth naming now.** no optimiser of
our own, which is not a loss: gcc optimises the userland already, and the
kernel is built without it for reasons that would not change if we had
one. no debugger, so a fault is read the way faults have been read here
since the beginning: a backtrace, a symbol, and the source that is now
sitting on the same disk.

**0.4.3 installing.** 0.4.2 gives the machine the pieces to write an
image; this is the program that does it, onto a disk that is not the one
it booted from: a partition table, philemon in the first sector, the
kernel, the ramdisk and the source, written by velvetOS on the machine
being installed. the development host is not in this sentence, and
neither are `mkboot`, `mkfat` and `mkext4`. *ends when a blank disk goes
in beside the system disk and comes out booting on another machine.*

**0.4.4 the machine replaces its own kernel.** build, link, install and
offer a reboot, from inside velvetOS, in place: the source has been on the
disk since 0.3.26 and everything that reads it is already ours. velvet
0.4.3 and 0.4.4 are what make it safe to try: a boot that can choose
between two kernels, and one that falls back to the old kernel when the
new one does not come up. *ends with the test 0.4.2 describes performed
for real: one line of the kernel changed on the machine itself, `make`,
install, reboot into the change, and the commit made by the git that lives
here.*

## 0.5.x: a machine for more than one person

0.4.x is about what the machine can do. this is the first series about
who it belongs to. there have been users since 0.1.5 and a session per
console since 0.2.16, and both entries were careful about what they did
not mean: `passwd` keeps its passwords in plain text and says why in a
comment of its own. that was true when it was written. what has changed
is that there is now something here worth reaching: a filesystem that
survives a reboot, the source the machine was built from, and after 0.4.x
the compiler that rebuilds it.

the kernel's half of this series is velvet's 0.5.x: credentials and
groups a process can carry, the setuid bit, randomness, the cryptographic
primitives, vaults, an address space that moves, limits, and a log
nobody can rewrite. what is here is what a person meets: the accounts,
the passwords, the login, and who is allowed to become whom.

**0.5.0 accounts, and groups to put them in.** `/etc/passwd` and
`/etc/group`, which moves the file into `/etc` on the way, since it has
been sitting at the root of the ramdisk since 0.1.5, and `login`, a
program since 0.4.1, moves with it. then `id`, and
`chmod`, `chown` and a `chgrp` that finally have a kernel that respects
them, since velvet 0.5.1 gives every process a gid and makes the vfs
consult the group bits.

the passwd file has been making the argument for this since 0.1.5
without meaning to: `elizabeth` is uid 0 because there was no other way
to say she is staff rather than a visitor. with a group there is one.

**0.5.1 the shadow.** `passwd` is readable by everyone, which is what
makes it useful and is why the hashes cannot live in it. a `/etc/shadow`
reachable only by the superuser, and a password hasher, argon2id, in the
libc, salted from velvet 0.5.0's randomness. **0.1.5's passwords were a
variable; these are a secret.** the comment in the file says storing them
properly needs somewhere to write, which is what a real disk is for; the
disk arrived in 0.2.21 and this is that comment coming due.

two things follow that the entry does not look like it contains. the
hasher is *deliberately slow and deliberately hungry*: argon2 asks for
tens of megabytes because that is what makes it worth having, so this is
the first program on the machine whose whole point is to allocate more
than it needs, and 0.3.13's reserve has to hold while it does. and slow
is fine where `login` is now: a program of its own since 0.4.1, on a
console of its own, so a check that takes half a second keeps nobody
else from typing.

**0.5.2 who else is here.** there have been four consoles since 0.2.16
and, once 0.5.0 exists, they can be four different people. `who` and
`w`; a console that belongs to whoever logged into it, so that 0.2.16's
rule (output belongs to its writer, input belongs to the screen) gains the
clause it has not needed until now, which is that neither belongs to
anybody else.

**0.5.3 grant.** a setuid bit on the filesystem, which velvet 0.5.1
teaches the kernel to honour, and a program called `grant` that uses it.
**it is the first program here that is meant to be a hole in the wall**,
which means the version is not "does it work" but "is the hole exactly
the size it claims". a configuration in `/etc/grant.conf` saying who may
become whom, and a way to prove who is asking.

the environment is where this goes wrong for everybody who writes one,
and this machine handed programs an inherited environment block in 0.2.18
and made `$PATH` mean something in the same version. a `grant` that passes
those through unexamined is a hole the size of the wall. the test is the
one the entry is for: a user edits a system file without becoming root
for the rest of the session.

**0.5.4 tls.** 0.3.6 could fetch a file and could not talk to most of the
web, and said so: there is no tls here, and a redirect to https is
reported rather than followed. this is that limit lifted: tls 1.3, one
cipher suite, no legacy anything, a library in the libc on top of velvet
0.5.2's arithmetic and velvet 0.5.0's randomness. **the most
arithmetic-heavy version since the compiler.**

the handshake is not the hard half. the hard half is that a certificate is
a claim about *dates*, and this machine reads the cmos clock once at boot
and believes it (0.3.12): a clock that is wrong by a year rejects the
entire internet, and one set forward accepts a certificate that expired.
and the trust store has to come from somewhere: shipping somebody else's
list of certificate authorities inside the image is a decision to make out
loud rather than a file to copy in, because it is the one thing on this
machine that is trusted without being checkable here.

**0.5.5 auditing.** a log in `/var/log/auth` recording every login, every
failed password and every use of `grant`. **security is not only about
stopping people; it is about knowing what happened when you were not
looking.** the machine already gets one part of this right by accident: a
failed login says "that is not a name and a word i know" for both halves,
because saying which was wrong hands over half of it. what it does not do
is *remember* that somebody asked.

two rules come with the file. a log the logged-in can edit is not a log,
so it is append-only to everybody but the kernel, which velvet 0.5.6 makes
something the kernel holds to. and a log with no limit is a disk that
fills, which velvet 0.5.5's limits do not cover from this direction, so it
rotates, and the rotation is part of the version rather than a thing to
add after the first full disk.

**0.5.6 an installer that asks.** 0.2.21 was a sequence of commands; this
is an interface, drawn with the escape sequences 0.3.9 taught the console
to understand. it asks for the hostname, the first user, their password
and whether they are an admin, which is now a real question, because 0.5.0
made a group something a person can be in. the two-partition layout from
0.3.25 stops being two commands and becomes a default.

what a conversation adds that a script did not have is somebody changing
their mind half way through, so this is the first thing here that has to
be *cancellable*: 0.2.21's ordering rule is still underneath it (the
partition table cannot be written before the copy that would erase it,
and philemon's first stage ends at byte 365 to leave room for it), and
every step before the one that commits has to leave the disk as it was.

**what this series deliberately does not reach.** no remote login: a
machine with users on a wire wants ssh, and ssh needs velvet 0.5.2's
primitives *and* a pseudo-terminal layer, which is velvet 0.6.7. that is
0.6.6 here, once both exist.

## 0.6.x: the machine runs software it did not write

0.4.x made the machine able to build itself and 0.5.x made it belong to
somebody. this one is about the software that did not come from here.

every program in this image was written against this machine, by somebody
who could change the machine when it was easier than changing the
program, which is the most comfortable position in software and the one
that hides the most. a program written somewhere else was written against
a unix that already exists, and it will not be polite about the
difference. that is the outside answer the whole series rests on: the
same argument as `readext2.py` and `makecheck.py`, at the scale of
somebody else's entire program.

velvet's 0.6.x makes the kernel answer the way linux does, so "written
somewhere else" stops meaning "written for another kernel". the programs
here follow it without a port: they include the kernel's
`include/uapi/velvet/` rather than copying it, and moving to a velvet that
speaks linux's numbers is a rebuild.

**0.6.0 a shell that is sh.** 0.2.18 ended its blocks with `end` rather
than `fi` and `done`, deliberately, on the grounds that a shell that looks
like sh and is not is worse than one that plainly is not, and when 0.4.1
made the shell a program, with quoting and its jobs run through the
kernel's calls, it kept that language so the scripts written for it kept
running. this is the other side of that argument: `fi` and `done`, `&&`,
`||`, subshells, `$( )` and functions, in the same program. the kernel's
own shell became a monitor in velvet 0.4.2, for when the disk is broken,
which is the better outcome than replacing it: one shell for that, and
one for work.

**0.6.1 a shell that remembers.** up-arrow history that survives a
reboot, and completion that knows about users and groups as well as
files. it stores history in `~/.history`, which means home directories
have to exist and be writable, and nothing in this tree names `/home`
today. the shell stops being a command runner and starts being a
workspace.

the detail that will bite: history has been per-console since 0.2.16,
deliberately, because four shells sharing one of anything is one shell
with four windows onto it. one person logged into two consoles is now two
sessions and one file, so it appends rather than rewrites. a shell that
writes its whole history at logout is a shell that loses an afternoon of
it whenever somebody logs out of the other console second.

**0.6.2 lua.** the first program in this image that the author did not
write. it is chosen for three properties and not for what it does:
self-contained C, no dependencies worth the name, and a test suite that
came with it. every hole in libc that a hand-written program never
reached is on the other side of that suite.

**0.6.3 the libc the port asked for.** whatever 0.6.2 turned out to need
(`qsort`, `setjmp`, buffered `FILE*`, the time functions), driven by a
real demand rather than by imagining one. `strtod` and `%f` wait on velvet
0.6.3: floating point in ring 3 needs fpu state in the context switch, and
there is none yet.

**0.6.4 sqlite.** the amalgamation, and its own test suite pointed at this
machine. a database is the most demanding thing anybody points at a
filesystem, and its durability tests are an independent audit of what
0.3.23 and 0.3.24 claimed about crashes and the journal, written by people
who have never heard of this machine, which is the only kind of audit
worth having.

**0.6.5 dynamic linking.** `.so` files, PLT and GOT, a loader, and a
`libc.so`. it needs position-independent code, which velvet 0.5.4 already
wanted for the program base, and it pays for itself where this project is
tightest: the ramdisk currently carries a copy of `printf` per program.
the kernel's half, mapping the loader a program names, is velvet 0.6.9.

**0.6.6 a remote shell.** ssh needs the primitives and a pty layer, and
both were somewhere else; velvet 0.5.2 and 0.6.7 are those. the value is
the same as every other port here: the test is somebody else's client,
speaking a protocol we did not design, against a server that has to be
right about every byte of it.

**0.6.7 a package, and somewhere to put it.** once software arrives from
elsewhere it has a name, a version, a signature and a list of what else it
needs. ustar with a manifest (the archive format this machine already
reads twice over), signed with velvet 0.5.2's primitives, named with
0.3.17's sha-1, and a `/usr` for it to land in. the point is not
convenience: it is that software from outside should arrive the same way
every time, and be removable, or the disk becomes a place things are
poured.

**0.6.8 the machine fetches its own source.** git in this image can commit
and cannot reach anything, and 0.6.6 proved the primitives. this is dns, a
route off the one segment, the ssh that is already here or the tls from
0.5.4, and packfiles at both ends: `clone`, `pull` and `push` against a
real remote. the pull request stays somebody else's problem, which is the
point: a push is a ref, and a browser is not required to open one. *ends
when this machine clones velvetOS from where it is hosted, builds it,
pushes a branch back, and the pull request is opened from a phone.*

**0.6.9 the terminal world.** ncurses, and the terminfo database under it,
which is one port standing in front of every full-screen program written
since 1980: editors, pagers, mail (`mutt`, and `msmtp` to send it), irc,
and `top`. *ends when an editor and a mail client, built from source that
arrived over the wire in 0.6.8, both open on this machine.*

## 0.7.x: the machine leaves the emulator

velvet's 0.7.x takes the kernel onto hardware nobody emulated: uefi, usb,
nvme, real screens, real power buttons. what that means for the system is
smaller and more dangerous: a disk the firmware will boot, the right
time, and an installer let loose on disks that were never made for it.

**0.7.0 a table the firmware will look at.** uefi wants gpt and a FAT32
EFI system partition, and this machine has read gpt since 0.2.15, read and
written fat32 since 0.1.10, and been able to format one since 0.3.21. so
this is a small version: the installer writes a gpt with its protective
mbr, lays down an ESP beside the two partitions 0.3.25 already arranges,
and puts velvet 0.7.0's loader where firmware looks rather than where
philemon put it.

**0.7.1 the time, from somewhere that knows it.** ntp. velvet 0.7.5 makes
the clock right and lets ring 3 set it; this is the program that knows
what to set it to. it matters more than it looks, because 0.5.4 turned
certificates into claims about dates: a laptop whose coin cell died
rejects the entire internet until somebody tells it what year it is, and
this is the somebody. *ends when a machine booted with its clock a year
out is right to the second within a minute of reaching the network.*

**0.7.2 an installer that could destroy something.** every disk this
machine has ever formatted was made for it. on somebody's desk the disks
are theirs, and one of them has photographs on it. so: name each disk by
model and serial rather than by number, refuse to have a default, say in
words what is about to be lost and require it to be typed back, and have
a dry run that writes nothing. the hardware itself is velvet's; this is
about what hardware means.

## 0.8.x: the machine explains itself

velvet's 0.8.x gives the kernel the means to say why something went
wrong: lines in its backtraces, core dumps, one process reaching into
another, tracing and a profiler. what the system adds is the program a
person uses to ask.

**0.8.0 a debugger.** breakpoints, stepping, registers, memory read and
write, and the symbols and lines velvet 0.8.0 made readable, through
velvet 0.8.2's way of reaching into another process. it is the first
program here whose whole job is to look inside another one, which is why
it waits on a kernel that can say who may. it is checked the way the
kernel's core dumps are: the same program debugged here and with gdb on
the development machine, agreeing on every frame. *ends when a program
that crashes is caught here, its stack named line by line, and stepped
past the fault.*

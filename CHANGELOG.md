# changelog

every version of velvetOS, newest first.

this lived at the bottom of README.md until 0.3.0, by which point it was
fifty-four entries and rather more than half the file -- a reader who
wanted to know what the machine *is* had to scroll past everything it had
ever been to find out. so it moved here, and the README is about the
current machine again.

the entries themselves are byte for byte what they were, which turned
out to need no effort at all: **not one of them ever named the project.**
they are written about what changed and why, so the rename in 0.3.0 had
nothing to edit here.

that was luck rather than discipline, and it is worth noticing. a
changelog full of "tinyOS now does X" would have left a choice between
rewriting fifty-four entries -- losing the only record of what the thing
was called when -- and leaving them inconsistent with everything else.

the versions are the ones in [ROADMAP.md](ROADMAP.md), and each of them
was a machine that booted -- that is the rule the roadmap sets itself
and the reason there are fifty-four of these rather than five.

---

- **0.4.0**: the last of the toolchain, and a kernel built by it. four
  things 0.3.x named as owed and left, none of them a compiler and every one
  of them in the way of one. **`trampoline.asm` diverged from nasm at byte
  22**, and sixteen bits turned out to be five encoding differences rather
  than the one the note said: real mode's ModRM is its own table where four
  registers appear in pairs and `rm=110` with `mod=00` is a bare address
  rather than `[bp]`; the accumulator has short forms with no ModRM byte at
  all; a near branch carries *two* bytes of displacement instead of four;
  the operand-size prefix means the opposite of what it means in long mode;
  and `align` pads with nops rather than zeros, in every section and not
  only in code. the byte comparison also found three things that are not
  addressing at all -- `mov eax, cr4` read `cr4` as an undefined symbol and
  assembled `mov eax, 0` with a relocation attached, `mov word [x], 5` had
  no encoding here at all, and **`lgdt` and `lidt` were told apart by the
  wrong letter of their own mnemonics**, so every `lgdt` this assembler ever
  emitted was an `lidt`. **the linker reads a script as a sequence now
  rather than as a table**, which is the change the rest of it came out of:
  `__text_start = .` is a name whose value is wherever the location counter
  had reached, and `. = ALIGN(CONSTANT(MAXPAGESIZE))` is that counter being
  pushed forward by an amount depending on what came before -- neither is a
  fact about a list of sections, and once placement walks the script with a
  counter in hand, `PHDRS`, `KEEP` and `/DISCARD/` are a few lines each. it
  reads **archives** too, and writes a symbol table it never used to:
  `gensyms` reads the linked kernel's own symbol table to build the table a
  panic symbolises its backtrace from, so a kernel linked without one boots,
  works, and cannot say where it died. **the archive rule is the whole
  reason an archive exists** -- an object named on the link line is always
  linked, a member only when it resolves something -- and it has to take
  members *one at a time*, working the name table out again in between,
  because taking every member that answers something in one sweep takes the
  second definition of a name as well as the first and then fails the link
  blaming the archive. `user/ar.c` is the other half of it, byte-identical
  to GNU ar's output down to two details only a comparison finds: the symbol
  index rounds *its own size* up to keep the next member aligned where every
  other member is followed by a pad byte outside its size, and the long-name
  table leaves its date, owner and mode **blank** rather than writing zeros
  in them. make gained `include`, `-include`, the four conditionals,
  `$(shell)` -- whose newline rule is the whole of why it is useful, since a
  command printing one path per line has to come back as a list of words --
  and `::`, where the fixture immediately caught the bug that matters:
  judging each rule as it ran let the first recipe's own output make the
  second one look up to date, so a target with two rules and neither of its
  files present built half of itself. and one number rather than a feature:
  512 rules was plenty for a fixture, and this kernel's dependency files
  come to **878** of them, because `-MP` writes an empty rule per header so
  that deleting one does not break the build. **the version ends where it
  said it would**: `make selfhost` builds this kernel with our make, our
  assembler and our linker, gcc being the only program left in the chain
  that is not this project's -- and `tools/selfcheck.py` holds the result
  against the kernel gnu make, nasm and ld built and finds **431,203 bytes
  identical and six that differ, which are the `__TIME__` string saying
  which second each was compiled in**. that check allows no budget of
  differences: it names what each one is, and a difference that is not a
  clock is a failure. what is still not ours in the image it writes:
  philemon, which is sixteen-bit code with `a32` prefixes and unreal mode
  under it, and is the assembler's next mile rather than this one. and the
  linker does no string merging, which costs a user program built at `-O1`
  about three bytes and this kernel nothing, since it is compiled without
  optimisation and has no mergeable section in it.

- **0.3.26**: the source, on the disk, and the one place it must not be.
  the last version before 0.4.0 and on paper the dullest: ship the project's
  own source tree in the image so that a machine which boots is also a
  machine you could rebuild from. **the obvious place for it is the ramdisk,
  and the obvious place is wrong.** the ramdisk is already a tar and already
  read -- and it is loaded whole into memory at boot and never freed,
  because programs are run out of it, so putting three and a half megabytes
  of source in there spends that much of *every* boot, forever, on files
  that are read on the day the machine is installed and then not again. the
  source has to be **reachable, not resident**, and those are very different
  prices. so it is written into the boot image past the ramdisk, at sectors
  philemon's table names, and left there: philemon does not load it, the
  kernel does not load it, and it is read a sector at a time by whoever
  opens a file in it. **the table only ever grows at the end**, which is
  what lets three new fields mean "no source tree" when they are zero -- an
  image from 0.3.25 reads as an image with nothing after the ramdisk, and
  the installer that copies "everything up to the end of the system" needed
  one line to carry the new region and no thought at all about the old ones.
  **it appears at `/boot/src`, which is where it actually is.** `/boot` is
  what the boot medium handed over and the source came on the same medium,
  so it is a mount of its own standing in the ramdisk's directory the way
  `/work` stands in the root's -- a third kind of thing under one namespace,
  being neither a disk nor memory but a tar on raw sectors. that cost one
  branch in `vfs_open`, one in `vfs_readdir` and one case in `vfs_read`,
  because 0.2.15 and 0.3.25 had already made "which mount does this name
  belong to" a question with somewhere to be answered; `ls`, `cat`, `grep`
  and `diff` work on it with nothing added to any of them, and the
  prefix-shifting that makes `/boot/bin` a real directory in a flat archive
  is now one function instead of two copies. **the image did not get
  bigger.** it has been padded to a cylinder past eight megabytes since
  philemon existed, because firmware that computes geometry the 1981 way
  reads a smaller disk as no disk at all -- so there were four megabytes of
  zeroes in there already, and the source fits in them. `velvetos.img` is
  the same 8,773,632 bytes it was last version. **and a stamp, because
  "there is a source tree here" is the weaker claim.** an archive and a
  kernel sitting in one image are two files that happen to be adjacent, and
  anybody can put two files next to each other. "this is the source *this*
  kernel was built from" can only be established at the one moment both
  halves are in the same room, which is the build -- so `tools/srcstamp.py`
  hashes the archive and writes the number into a C file linked into the
  kernel, and `src verify` reads every sector back off the medium and holds
  the two against each other. the price is that editing anything at all
  relinks the kernel, which is not overhead so much as the claim being true:
  a kernel whose stamp survived an edit to its source would be a kernel
  telling a lie. **the installer writes it twice, deliberately.** the
  archive is inside the system area, so the verbatim copy carries it without
  being asked; then the new disk gets it *expanded* into `/src` as ordinary
  files, through the same `install_copy_tree` that has been copying the
  ramdisk since 0.2.15. one copy is what this kernel was built from and
  cannot be written, the other is yours -- and `diff` between them is then a
  sentence about what you have changed, which is what 0.4.0 will want on the
  morning somebody first edits this kernel from inside it. **it also found a
  race that had been sitting in the installer.** reading the source means
  selecting a drive that is usually not the mounted one, and `disk_raw_read`
  did that *without* the lock every filesystem operation holds -- so a
  thread switching to drive 0 while another sat between selecting drive 1
  and reading from it would have handed the second thread the wrong disk's
  sectors and no reason to doubt them. survivable while the only raw reads
  were one install command somebody was watching; not survivable once `ls`
  could do one. **what checks it is deliberately not the code that wrote
  it.** `tests/test_source.c` runs the reader against a medium made of bytes
  -- a half-written archive, a table claiming more than was set aside for
  it, a name split across ustar's prefix field, and every awkward offset a
  read can start at, including the check that listing every file and opening
  each one costs about one read per file rather than a walk per file.
  `tools/srccheck.py` starts from the *image* instead: it finds the archive
  through the boot table, reads it with python's tarfile -- somebody else's
  tar reader, which is the entire point -- compares every file byte for byte
  against the working tree, requires that everything git tracks is in there,
  and requires the archive's sha-1 to appear inside the kernel shipped
  beside it. and the boot test does it the slow way on a real machine, off a
  drive that is not the one it is running from. **what is missing is a way
  to write one.** the machine can read both archives and write neither --
  `tar` builds them and `tar` is not on this machine -- so an installed
  velvetOS can rebuild the kernel's contents and not yet the image that
  carries them. `mkboot` is already C and already knows about the new
  region, and a ustar writer is a hundred lines; it is named here rather
  than glossed, because 0.4.0's whole sentence is about what a surviving
  disk can do.

- **0.3.25**: a disk that can grow, and a filesystem that is not *the*
  filesystem. two halves that turned out to be the same sentence twice. **a
  filesystem describes exactly the disk it was made for**, which is fine
  until the disk is not the one it was made for: an image written onto a
  bigger drive is a 64 MiB filesystem with a terabyte of nothing behind it,
  and the usual answer -- make a new one and copy -- needs somewhere to copy
  to. a machine holding the only copy of its own source does not have
  somewhere to copy to. **growing is almost entirely additive, and that is
  the whole of why it is safe.** a new block group is a block bitmap, an
  inode bitmap and an inode table written *past the end of the filesystem as
  the superblock currently describes it* -- nothing can reach any of it,
  nothing looks at it, and a machine that stops half way through writing it
  comes back to exactly the filesystem it had. then one transaction moves
  one number and the whole of it becomes real, in that order: the
  descriptors before the count that makes them real, because a machine
  stopping between the two has a table describing groups the superblock says
  nothing about, which nothing ever reads, while the other order would have
  a filesystem claiming blocks it has no descriptor for.
  `tests/test_resize.c` stops the machine at every one of the 217 writes a
  grow takes and demands a filesystem at every one -- the same claim
  `test_crash.c` makes about the journal, and it holds for the same reason.
  **shrinking moves nothing either, and that is a limit rather than a
  design.** giving a tail back means moving whatever is out there first, and
  moving a block means finding everything that points at it -- an inode, an
  extent tree, a directory entry -- and correcting every one. that is not a
  smaller version of growing, it is a different program. so a shrink whose
  tail is *already empty* is arithmetic, and one whose tail is not says how
  many blocks and inodes are in the way and stops. the third outcome, a
  shrink that succeeds and loses a file, is the one that must never happen,
  and the suite checks the refusal as carefully as it checks the success --
  including that the refused filesystem is exactly the size it was, since a
  refusal that changed something would be the worst of both. **the ceiling
  is the descriptor table, and the fix for it belonged in the formatter.**
  the table describes every group and sits at the front of *every* group,
  with the bitmaps and the inode table directly behind it -- so a table that
  needed one more block would push every bitmap and every inode table on the
  disk along by one, in every group at once. nobody does that; it is not a
  resize, it is a rewrite. so `mkfs.c` reserves the room up front, which is
  what `s_reserved_gdt_blocks` in the format is for: a handful of blocks
  after the table that belong to it and are marked used from the day the
  filesystem is made, sized to describe a filesystem sixty-four times the
  one being formatted. growing the table is then filling in a block that was
  already there and nothing behind it moves. `fsck.c` had to be told about
  them in the same breath, or it would have called them leaks and handed
  back the space a resize is going to need -- a checker breaking the thing
  it was run to protect, one version after the last time that nearly
  happened. **and the other half: a filesystem is not the filesystem.**
  0.2.15's sentence was "a disk is not a filesystem"; this is the same
  sentence one level up. one was mounted at a time and `mount <n>` *moved*
  it, which is fine for looking at a second disk and no use at all for what
  this version is about -- keeping the system and the work on separate
  filesystems so that filling one does not stop the other. a partition you
  must unmount the system to reach is not that. so there are two, `/` and
  `/work`, and **every call that touches a file now says which**: the
  alternative was a second copy of each with `work_` in front of it, and two
  copies of anything drift. the routing lives in the vfs rather than in
  `disk.c`, because deciding that a name belongs to a mount at all is the
  namespace's job and being a filesystem afterwards is the disk's. both
  mounts are on the same drive and that is the block cache's doing rather
  than a simplification -- it holds absolute sector numbers and is bound to
  whichever drive is selected, so two mounts on two drives would be two
  caches or one that writes a block back to the wrong disk. **the case worth
  building the test around is a name that exists on both.** `/notes.txt` and
  `/work/notes.txt` are two different files, and a router that forwards the
  path but loses the mount reads one when asked for the other -- which looks
  exactly like a corrupt disk and is not. so `struct vfs_file` remembers
  which filesystem it came from, and a rename from one to the other is
  **refused** rather than quietly turned into a copy and a delete wearing a
  rename's name. **the installer lays down two partitions**, and the split
  is not half and half because the two halves are not the same kind of
  thing: what the system needs is bounded and known -- it is the thing being
  copied, plus its own source once 0.3.26 puts it there -- and what the work
  needs is whatever is left, because nobody knows. so the system gets a
  floor or an eighth of the disk, whichever is larger, and a disk too small
  for both to be worth having gets one partition and is told why, since two
  filesystems each too small to hold anything is worse than one that is
  merely small. `make run` and the boot test both build a disk whose second
  filesystem is **smaller than its own partition** -- 8 MiB of ext4 inside
  24 MiB -- because that is precisely the situation `resize` exists for, and
  a feature demonstrated only by its own unit test is a feature nobody has
  watched work.

- **0.3.24**: the journal, and three ways of making a crash worse before
  making it better. 0.3.23 ordered the writes so that a crash could only
  leave a *leak*, and built an fsck that gives leaks back; what that costs
  is a walk of the whole filesystem after every unclean stop, which is the
  reason unix machines used to take minutes to boot after a power cut. **a
  journal replaces the deduction with a record**: before a change touches
  the filesystem it is written down somewhere else, in full, with a mark at
  the end saying all of it arrived -- so recovery is not detective work but
  "read the log, apply what is marked complete, ignore what is not".
  `fs/jbd2.c` is jbd2, the format ext4 already names, so this is a log a
  linux could read rather than one of my own invention: inode 8, a ring of
  blocks, and a transaction in it is a **descriptor** saying where the
  blocks after it belong, the blocks themselves, and a **commit block**
  saying they all arrived. every field big endian on a machine that is not
  -- which is not the format's age, jbd2 is younger than that, it is that a
  journal is the one structure that may have to be read by a machine other
  than the one that wrote it. **the whole of the guarantee is three waits.**
  the descriptor and the blocks are on the disk before the commit block
  claims they are; the commit block is on the disk before anything goes to
  its home; and the homes are on the disk before the log says it no longer
  holds them. the first two are the ones every description of a journal
  mentions and they protect the log from the disk. the third is the one that
  is easy to leave out and it protects the disk *from the log* -- moving the
  tail past blocks a write-back cache then loses is a change nothing will
  ever replay again, sitting in a log that has been told it is empty. this
  is why `struct ext4` grew a `sync` and why a journalled disk mounts
  **read-only** until somebody calls `ext4_start_journal`: the promise about
  ordering is not the filesystem's to make, it belongs to whoever knows what
  is between it and the platters. **and the roadmap's warning about doing it
  badly was a description of the first working version rather than a
  hypothetical.** the crash suite went from 0 stopping points leaving
  unanswerable damage to 4, and each of the three bugs behind that is a
  different way of being wrong. a data block was zeroed *through* the log
  and then filled straight to the disk -- ordered mode, correctly -- so the
  commit dutifully copied the log's zeroes over the top of the contents: the
  block was allocated, named, counted and empty, and a symlink whose target
  was too long for its inode lost the target. a freshly formatted log says
  `s_start = 0` and the code read that as "clean, nothing to replay", which
  meant the first transaction ever written to a new filesystem sat entirely
  outside recovery's reach; an empty log is replayed too now, which costs
  one read of a block with no magic in it and closes the window. and a
  transaction that outgrew the log was **abandoned** -- its already-staged
  blocks dropped and the rest of the operation written straight to the disk,
  which applies the *end* of an operation without its beginning and is
  damage invented by the thing installed to prevent it. it restarts instead:
  what is staged is committed as a transaction of its own and the rest
  continues in a fresh one, so the worst an operation bigger than the log
  can leave is exactly what 0.3.23 left. **`ext4_write` is a transaction now
  too**, which it was not -- the busiest allocator on the disk was the one
  thing outside the log, and it is also the only operation with no bound on
  its size, which is what made the restart necessary rather than merely
  tidy. **the crash suite asks the harder question.** it used to stop the
  machine at every write there is and demand the wreckage be *mendable*; it
  stops at all 145 of them, boots the machine again so that recovery runs --
  the power coming back on before fsck, which is the only order there is --
  and demands that there be **nothing to mend**. and because a journal
  checked only by the code that wrote it is two hundred lines agreeing with
  themselves, which is the arrangement mkfat.py was wrong inside of for a
  whole version, `test_mkfs.c` hand-writes a transaction from the format
  itself -- twelve bytes of header, a tag, the block, a commit block behind
  it -- and requires the driver to replay it, and requires the same log
  *without* its commit block to be replayed by nothing. **a disk now says
  whether it needs recovering, and means it.** the driver has always
  respected `needs_recovery` on somebody else's filesystem and never set it
  on its own, which is the quiet failure: a linux mounting this disk after a
  crash would have seen a filesystem claiming to be fine, skipped the
  replay, and read the version of every block the log was in the middle of
  correcting. it is set the moment the log goes live and cleared only by a
  clean unmount, which is a thing that now exists -- `init` closes the
  journal on the way down, after the last sync and before the machine stops,
  and `mkext4` closes the one in every image it builds so that what ships is
  a filesystem that was unmounted rather than one that merely stopped being
  written to. **what is not here is revoke**, and the two halves of that are
  different. none are written, which is a design: revoke records exist
  because linux checkpoints lazily, so a block can be journalled as
  metadata, freed, and handed out again as somebody's file contents while
  the old transaction is still in the log -- and replaying it then writes a
  directory over a file. this checkpoints inside the commit and waits for
  it, so no transaction outlives the operation that made it and the hazard
  has nowhere to happen. none are *understood*, which is an omission: every
  mke2fs sets the feature, so a log written by linux is refused and its disk
  stays read-only. that costs less than it sounds, since such a disk has
  `metadata_csum` on it and has been read-only for that reason since 0.3.22
  -- but it does mean the roadmap's "lift the read-only rule on any disk
  with a journal already on it" is half done, and the half that is missing
  is named rather than glossed.

- **0.3.23**: a filesystem that survives the plug, and the discovery that
  three obvious orderings were all backwards. 0.2.13 was honest that a
  write-back cache turns "you might lose anything" into "you might lose the
  last few seconds", and this is the other half of that sentence: not *how
  much* a crash costs but **what kind of damage it leaves**. there are two
  kinds and the difference is everything. a **leak** -- a block or an inode
  marked used that nothing points at -- costs some space and has exactly one
  right answer, computable from what is on the disk. a **dangling
  reference** -- a name pointing at an inode that was never written, an
  inode pointing at blocks nobody allocated -- has *no* right answer, and an
  fsck that picks one destroys the thing it was run to save. which of the
  two you get is decided entirely by the order the writes went out in. **so
  first there is an fsck of its own.** `fs/fsck.c`, in C, on the machine --
  `tools/readext4.py` has been the second opinion since 0.2.14 and it runs
  on the machine this was *developed* on, which is no help to a disk that
  gets damaged after 0.4.0. it reads the layout by hand rather than through
  the driver, for exactly the reason readext4.py imports nothing: a checker
  that shares the driver's idea of what the disk says is unable to notice
  the driver being wrong. and it mends -- but only what has one right answer
  *and* can be computed rather than chosen: free counts, link counts, a
  block marked used that nothing claims, an inode allocated whose name never
  landed. a block two files both claim is reported and **left alone**,
  because one of them is wrong and nothing on the disk says which. the first
  thing it was ever pointed at, it got wrong in an instructive way: it
  called group 1's superblock and descriptor *copies* leaked and would have
  freed them, which is a checker mending a filesystem into one that hands
  out its own superblock. **then the orderings, which are the actual
  answer.** three were backwards, and all three were the obvious way round.
  a link count written *before* the name it counts -- so a crash in between
  leaves an inode insisting somebody points at it when nobody does, which is
  lost+found's problem everywhere else and unanswerable here. a directory
  linked into its parent *before* it had its own `.` and `..` -- so a crash
  leaves something reachable that is not a directory and cannot be
  reconstructed. and a name removed *before* the count came down, leaving
  the same orphan as the first. reversed, each one's failure window holds a
  leak instead: an allocated inode nobody names, an allocated block nobody
  points at, a link count too low. all mendable, all computable. **what
  makes that a claim rather than a hope is `tests/test_crash.c`**, which
  stops the machine at every write there is -- seventy-nine of them -- and
  demands that what is left is mendable and that mending it produces a
  filesystem the checker then calls clean. not usually. every one. it
  started at 27 of 75 stopping points leaving unanswerable damage. and
  `tools/fsckcheck.py` is the other side: nine deliberate breaks, one per
  check, each of which must be found -- and for the mendable ones the
  repaired image is handed to `readext4.py`, which shares no line with the
  checker, because an fsck saying it fixed something is an fsck's opinion of
  itself. what is **not** here is the journal. ordered writes mean a crash
  costs a run of fsck; a journal is what would make the fsck unnecessary,
  and it would also lift the read-only rule on any disk that already has one
  -- which today is every ext4 anybody else made. it is 0.3.24 and its own
  version, because every metadata write becomes part of a transaction and
  the block cache has to stop being free to write a block home whenever it
  likes. doing that badly makes data loss *more* likely, which is the one
  outcome worse than not having it yet.

- **0.3.22**: ext4, and the sentence ext2 could not say. this machine's
  filesystem was from 1993 and it showed in one place above all: ext2
  answers *where is block N of this file* with a list of every block the
  file owns. a gigabyte in 1 KiB blocks is a million numbers, four megabytes
  of map, and reading the file means reading the map. an **extent** says the
  other thing -- *the next N blocks are at P* -- so a file written front to
  back is twelve bytes of map whatever its size. that is the difference ext4
  is actually about, and everything else it added is trimming. **it is the
  same driver grown up, not a second one beside it.** `fs/ext2.c` is
  `fs/ext4.c`; there is no ext2 driver any more and there does not need to
  be, because ext4 *is* ext2 with features bolted on and an ext4 disk holds
  files of both kinds at once -- a file made before the feature was turned
  on keeps the map it was born with, since rewriting it would mean rewriting
  every file on the disk to change one flag. so which map a file uses is a
  bit in its **inode**, not a property of the filesystem, and `map_block` --
  the one function that ever answers where a block is -- asks that bit and
  takes one of two paths. two drivers would have been the same code twice
  with the bug fixed in one of them, which is the mistake this project
  already made once with two libcs and once with two ext2 layouts. **the
  tree grows a level exactly once per file**, and that is the only
  interesting moment in the whole feature: the inode's sixty bytes hold a
  header and four records, so a file in four pieces or fewer costs nothing
  to describe, and the fifth piece is the one that has to move those four
  into a leaf block and leave an index behind. it is easy to write code that
  never reaches that -- a file written in one go is *one* extent, because
  the allocator hands back the block after the last one -- so the suite
  forces it, growing two files a block at a time so that neither ever gets a
  contiguous run and the fifth append has nowhere to go. **what is read and
  deliberately not written is everything with a checksum in it.** a modern
  `mke2fs` turns on `metadata_csum`, and then every write owes a crc32c that
  this driver does not compute; an image whose checksums disagree with its
  contents is one fsck calls corrupt, so a disk claiming that feature
  **mounts read-only** rather than being refused or, worse, written to. the
  same for a disk with a journal, since writing beside a journal is how the
  older version of a block wins -- and the driver loses its write function
  outright rather than being trusted to remember, so there is one rule in
  one place instead of fifteen scattered checks. the formatter claims
  neither feature, which is the same decision seen from the other side. and
  **the reader was taught the new map before anything was trusted to write
  it**: `tools/readext4.py` shares no line with the formatter or the driver,
  so a file whose extents are wrong is caught by something that has no stake
  in them being right -- which is how the extent code was checked at all,
  since there is no `e2fsck` on this machine to ask. two things fell out of
  the version that were not about ext4. `tools/toolcheck.py` was leaving its
  working directory behind on every run, and 0.3.21 had put two 64 MiB fat
  images in it -- a hundred and seven of those had accumulated, a tenth of a
  gigabyte apiece, and what fails when /tmp fills up is never the thing that
  filled it. and the rename found a **string literal the compiler cannot
  check**: the ext2 suite opened `bin/tests/ext2.img` by name, so after the
  rename it silently read a stale image a previous run had already written
  to, and failed in seven places that all looked like driver bugs. an hour
  went into that. the lesson is the old one -- a test that finds its input
  by a name nobody rebuilds is a test that can pass or fail for reasons that
  have nothing to do with the code.

- **0.3.21**: the build tools, in C, and the filesystem learning to write a
  name it could already read. six of this project's build tools were python
  -- `gensyms`, `mkboot`, `bin2c`, `checkfmt`, `mkext2` and `mkfat` -- and
  **a self-hosting machine has no python and is not going to get one**, so
  every one of them is a C program now. this is the entry that is easy to
  miss and blocks everything after it: without it 0.4.0 is a machine that
  cannot rebuild itself without the machine it was built on. **the whole
  risk of a version like this is that nothing looks different when it goes
  wrong** -- a tool that changes its output by accident produces an image
  that builds, boots, and is quietly not what the last one was. so each C
  tool is built *beside* the python it replaces rather than instead of it,
  and `tools/toolcheck.py` runs both and compares: byte for byte for the
  four with one right answer, and **not** byte for byte for the two
  formatters, because two formatters that allocate in a different order
  produce two perfectly good filesystems and demanding identical bytes would
  be demanding identical decisions. those go through a reader instead. **two
  of the six stopped reaching for things the machine will not have**:
  `gensyms` asked `nm` for the kernel's symbols and now reads the symbol
  table out of the ELF itself, the same structures the linker in 0.3.15
  already walks; `mkext2` turned out to be four hundred and sixty lines of
  python laying out ext2 a *second* time, next to `mkfs.c` and `ext2.c`
  which have done it since 0.2.14 -- so it became a two-hundred-line driver
  over code that already worked, and the duplication `mkfs.c` says in its
  own comment it exists to avoid was there all along in a different
  language, where nobody saw it. **and mkfat is where the version stopped
  being a rewrite.** it populates its image through `fs/fat32.c`, the same
  driver the kernel uses, for that same reason -- but the driver could read
  a long name and **refused to write one**, so going through it would have
  renamed half of `diskroot/` and put `velvet-room.txt` on the disk as
  `VELVET~1.TXT`. the fix belonged in the driver rather than in the tool,
  and every program on the machine got it: `echo x >
  /a-name-far-too-long.txt` works now. a long name is several records that
  have to go in or not at all, and the two details that are not obvious are
  both about *other* readers -- the short name behind it is minted with
  `~1`, `~2` until nothing in that directory shares those eleven bytes,
  since an older tool has nothing else to tell two files apart by; and the
  group is kept inside one cluster, so when it will not fit in what is left
  at the end of one, those slots are **struck out** rather than skipped.
  that second one is the bug this version would have shipped: a zero byte
  left in the middle of a directory is the end of that directory as far as
  every reader is concerned, and every file written afterwards would have
  been invisible -- to everyone except the code that wrote them, which is
  the code that would have been reading them back in the test. **so there is
  a second opinion now.** `tools/readfat.py` shares no line with the
  formatter or the driver, walks the image the way the specification says
  to, and insists on the things a fat filesystem may assume about itself:
  every long name complete, in order and checksummed to the entry behind it;
  nothing hidden behind the byte that ends a directory; no two entries
  sharing a name, long or short; every chain ending, in range, and owned by
  exactly one file. it runs over what the formatter wrote *and* over what
  the test suite wrote, which are different claims, and it found the first
  thing it looked at -- mkfat left the free-cluster count in fsinfo saying
  the whole disk was empty on a disk that was not. **and the two libcs are
  one.** `kernel/src/lib/string.c` and `user/libc/string.c` have been the
  same code twice since 0.3.8; there is one file now, compiled twice --
  freestanding for the kernel, hosted for ring 3 -- which is a build-system
  change and this was the version touching the build system. the two printf
  formatters stay apart, and that is not the same call: the kernel's streams
  into a console, a serial line and a ring buffer under a lock, so that a
  panic half way through still prints what it got.

- **0.3.20**: git, inside the machine, and what two libcs disagreeing
  costs. the store went in last version and nothing could reach it: a host
  program the checks drove, linked into no ring 3 program at all. this is
  the half that makes it usable -- the **same `gitobj.c`**, with velvetOS's
  syscalls underneath instead of the host's stdio. that worked by **naming
  the seam rather than reaching through it**: `user/gitio.h` is five
  functions -- read a file, write one, make a directory, does it exist, the
  nth name in a directory -- with a host implementation and a velvetOS one
  under it. the same pattern as `libc_get_pages` in 0.3.8 and for the same
  reason: the alternative is the same file twice, drifting, with the bug
  fixed in one of them. **the port's real cost was the two libcs
  disagreeing.** the store used `snprintf` everywhere and this machine's
  formatter does not take `%zu` -- an object header reading `blob %zu`
  rather than `blob 20` is an object whose name nothing else in the world
  agrees with, and it would have failed *silently*, since it still
  compresses and still reads back as itself. so the three things it needed
  are longhand now: a decimal number, an octal one, and joining a path, all
  three shorter than the bug would have been to find. `strtoul` went the
  same way. and one honest ugliness, written down where it is: `epoch.c` is
  pure date arithmetic *and* `epoch_now()`, which asks the timer chip, and
  an archive pulls in whole members -- so wanting the first drags in the
  second and its reference to `pit_uptime_ms`. the seam answers it and says
  why; splitting epoch.c the way signal.c and tty.c are split is the real
  fix and belongs to whatever version next has a reason to touch that file.
  the commit's date comes out of a file's timestamp instead, through the
  conversion 0.3.12 checked against the host's timegm. **there is no
  index**, and that is a decision: git's is a binary format with its own
  version number and its own compatibility risk, and without one `commit`
  records the whole working tree -- no staging, so nothing to forget to
  stage. what it costs is no `add`, no partial commits, and a real git
  disagreeing about `status` until it has built an index of its own. `diff`
  walks the commit's tree against the working tree and hands each changed
  pair to `user/difflib.c`, which had been sitting there since 0.3.11
  waiting for a second caller, and **`checkout` is that same walk with its
  printing turned off** -- so "what changed" and "is there anything to lose"
  cannot disagree about what counts as a change, which is the only reason it
  is safe to refuse on the strength of it. and it refuses: it is the first
  thing in this project that can destroy something a person typed, so it
  stops and points at `git diff` rather than overwriting and being sorry. a
  **branch** is one file with forty characters in it and `HEAD` is a line of
  text naming one, which is why making a branch is instant however large the
  history is and why a detached head is not a special case but HEAD holding
  a commit instead of a name. `log` walks parents, which is all history is:
  a commit names the one before it, nothing holds a list, and making a
  commit updated exactly one file -- the branch. **and two bugs that only
  came out of using it.** the shell had *no quote handling at all*, so `git
  commit "a message"` handed the program `"a` and `message"` -- and nothing
  could tell: the commit was made, the log printed `"first`, and it looked
  like git losing the message rather than like the line never having been
  one string. quoted words are one word now, single or double, with the
  quotes removed rather than passed on, and an unterminated one is the rest
  of the line rather than a walk off the end of it. and `ls` with no
  argument listed **`/`**, hardcoded, so it had been ignoring every `cd`
  ever typed since it was written -- which looked like the shell losing
  track of where it was, and was one character in `user/ls.c`.

- **0.3.19**: a git object store, and the half-broken comparator that hid
  behind a sort. **an object's name is its hash** -- not a name with a hash
  attached and not a hash kept in an index: the sha-1 of the content is the
  only name the content has, and where the file goes is worked out from that
  name. two identical files are one object, a file cannot be edited without
  becoming a different object, and there is no such thing as a corrupt
  object that still looks valid, because checking *is* rehashing. everything
  else falls out of it: history is not a list of diffs but a chain of
  complete snapshots that share what they have in common, and they share it
  automatically because equal content has equal names. blobs, trees and
  commits, written as loose objects -- compressed with 0.3.18's deflate,
  named with 0.3.17's sha-1, both of which were written before anything
  needed them and this is the thing that needed them. **the compatibility is
  the point, and the roadmap said so before the code existed**: a store only
  this machine can read is a store that agrees with itself. so
  `tools/gitcheck.py` has our code write a repository from nothing -- every
  blob, every tree, a commit, the branch ref -- and hands it to real git,
  which `fsck --strict`s it, `log`s it, prints the message back, and reports
  `status` clean against the commit. and the tree hash ours computes for a
  directory is compared against what `git write-tree` computes for the same
  one: forty characters covering every blob, every name, every mode and the
  order they are in. two details in the format are unforgiving and neither
  is visible by reading. **a tree is binary** -- entries carry the twenty
  *raw* bytes of a name rather than the forty characters, and writing hex
  gives a tree that looks perfectly reasonable and is twice the size. and
  **the sort order is not strcmp**: a directory sorts as though its name
  ended in a slash, so `lib.c` comes before `lib/` since `.` is 0x2e and `/`
  is 0x2f, while strcmp puts `lib` first because it is a prefix -- one entry
  out of place changes that tree's hash and every hash above it, and git
  says only "not properly sorted". **the best result of the version came out
  of breaking that comparator in one direction only.** the unit test caught
  it; git missed it completely, because insertion sort only ever consulted
  the direction that was still right. a half-broken comparator hides behind
  the access pattern of whatever sorts with it, which is a thing i would not
  have guessed and will not forget. broken in both directions, git caught it
  at once. also caught: a mode written `040000` rather than `40000`, the
  zero byte left out of the `<type> <length>\0` header, an object written
  uncompressed, and a commit missing the blank line between its headers and
  its message -- which git read as a commit with no message at all.

- **0.3.18**: deflate and inflate, and the third of the format that only
  somebody else's compressor reaches. every object in a git repository is
  compressed -- not optionally, not as a setting: the file on disk *is* a
  zlib stream -- so reading a repository means implementing this whether or
  not anything else in the machine wants compression. it arrived as a
  prerequisite rather than a feature and it is worth saying so. deflate is
  two ideas stacked and independent of each other: **lz77**, where anything
  seen before is written as "go back this far, copy this many", which is why
  text compresses and random bytes do not; and **huffman**, where common
  symbols get short codes and the codes are *canonical* -- the table is
  never transmitted, only the length of each code is, and both ends derive
  the same codes from the lengths by the same rule. that is the trick that
  makes the header small and it is most of what the decoder is about. **the
  two halves need different checks and that is the whole design of
  `tools/zlibcheck.py`.** inflate has to read what *other people* wrote, and
  deflate is three formats in a trenchcoat -- stored, fixed huffman, dynamic
  huffman -- with the compressor's *level* deciding which comes out: 0
  stores, the low levels emit fixed codes, the high ones build a table per
  block. so a decompressor tested only against its own compressor has been
  tested against one third of the format, and the third it wrote itself.
  every input goes through zlib at all ten levels. deflate, meanwhile, only
  has to be *understood*: "it is small" is not a claim worth checking and
  "zlib reads it back and gets the original" is the entire contract, because
  it is the one that decides whether a repository this machine writes can be
  cloned by git. **that design paid immediately**: two bugs -- the
  code-length alphabet's peculiar transmission order, and the
  repeat-the-last-length code appearing at the very start of a table --
  passed the suite completely and were caught only by the higher zlib
  levels, since our own compressor never emits a dynamic block and never
  would have gone near either path. and the check the version exists for: a
  temporary repository made by real git, its loose objects inflated by ours
  and compared against zlib's answer and against the `<type> <length>\0`
  header that 0.3.19 has to parse. 203 streams agree. the compressor is
  fixed huffman with a greedy match search, which is the middle of the three
  answers deliberately -- storing everything is four lines and gives a valid
  repository that is merely large, and a dynamic table means counting
  frequencies, building an optimal code and then transmitting it, which is a
  second huffman *encoder* for a saving that matters to a network and not to
  this machine. zlib beats it about threefold and both files are equally
  valid. two things in here are load-bearing and look like nothing: an lz77
  copy goes a byte at a time and **not** through memcpy, because a match may
  reach back fewer bytes than it is long -- "go back one, copy two hundred"
  is how a run of one byte is written -- so source and destination overlap
  and memcpy is entitled to do that in any order it likes; and the bit
  writer reverses every huffman code on the way out, because deflate packs
  bits from the bottom of a byte upwards while a huffman code is read from
  the top down. the suite covers what the differential check cannot: a wrong
  adler32, a header that is not one, a stream that stops in the middle, and
  nine thousand bytes refusing to be unpacked into a hundred -- a
  decompressor that accepts nonsense is the one genuinely dangerous thing
  here, since the input is a file somebody else wrote and the output goes in
  a buffer of ours.

- **0.3.17**: hashes, and the vector that proved vectors are not enough.
  three sums that get lumped together under one word and answer different
  questions: **crc32** answers *did this arrive intact*, arithmetic in a
  polynomial field chosen so the errors a wire or a disk actually makes are
  certain to change it rather than merely likely to -- it is not a hash in
  the security sense and gpt uses it because a disk does not lie
  deliberately. **adler32** answers the same question worse and is here
  because a zlib stream ends with one, so reading a compressed git object
  will mean checking one. **sha-1** answers *is this the same thing*: a git
  repository is a store addressed by the hash of what is in it, so it is not
  a check bolted onto the format, it is the format's idea of identity. each
  has a streaming form, because the input is not always in memory and the
  whole point of a sum over a stream is to compute it while the stream goes
  past. **checked against python, zlib and git.** a published vector proves
  the algorithm was copied right for the lengths whoever wrote the standard
  thought of, and every one of them is a single buffer hashed in one go --
  the case that *cannot* fail, since it never exercises the part where the
  input arrives in pieces and a half-finished block has to be kept until the
  rest turns up. so three hundred lengths go through both, and then through
  again in chunks of 1, 7, 63, 64 and 65 bytes: 64 is the block size, so the
  ones either side are where the buffering is off by one or missing. 1708
  answers agree. and the one that matters for what comes next is **`git
  hash-object`** -- a git object's name is the sha-1 of its type, its
  length, a zero byte and *then* the content, which no sha-1 vector can tell
  you and which is why an empty file still has a name. ours agrees with
  git's on every file it was given, which is the contract 0.3.19 rests on,
  tested before there is anything to rest on it. **and the result that
  justifies all of it**: taking adler32's modulus at 65536 rather than 65521
  passed every published vector, because all three are short enough that the
  sums never reach either -- and passed the whole-against-parts consistency
  check too, because a wrong constant is wrong consistently. only somebody
  else's answer caught it. there is a long vector in the suite now, and the
  number in it came from zlib, which is the point. crc32 used to live
  privately in the gpt driver, which had *also* hand-inlined the table a
  second time to checksum an entry array one sector at a time, since the
  one-shot form had nowhere to keep its place; both are gone, because a
  checksum with one caller is a checksum that gets written twice the moment
  there are two. and there is a **`sha1sum`** to run, printing what
  everybody else's prints so the answer can be pasted into a terminal on
  another machine, with `-c` for crc32, `-a` for adler32 and `-g` for the
  name git would give the file. it lives in `libc.a` rather than in every
  program, since an archive member is pulled in only when something needs
  it.

- **0.3.16**: make, and the three tools finally used together. building
  anything by typing commands in order works exactly until the first time
  you change one file: then you either retype all of them and wait, or
  retype some of them and be wrong, and the second is worse because being
  wrong does not look like being wrong -- it looks like a build that worked
  with a binary in it that was never rebuilt. so: **a graph**, walked depth
  first with a cycle check, because a graph written by a person eventually
  has one; **timestamps**, which is the only question make ever asks and is
  one line of comparison; and **a small language** -- variables of both
  kinds, pattern rules, the automatic variables, `.PHONY`, and fourteen text
  functions. **checked against gnu make, edit by edit.** a fixture is a
  makefile and a sequence of edits -- write this, touch that, run, run again
  -- replayed against both, comparing every command, every message, every
  exit status and the directory left behind. the edits are the whole point:
  a make that gets a cold build right and an incremental one wrong is the
  *normal* kind of broken, and only the second run of a fixture ever shows
  it. timestamps are set rather than clocked, the files the build writes
  included, so there is no `sleep` anywhere and no fixture that passes on a
  fast machine and fails on a slow one. eleven deliberate breaks and eleven
  caught -- but **four were MISSED first**, and the fix was to the fixtures
  rather than the code: equal timestamps had no way of occurring, a
  file-backed diamond quietly absorbed a target built twice, no phony target
  had a file of the same name next to it, and no pattern rule was ever asked
  for a `.o` with no `.c` anywhere near it. two things gnu make does that
  only the comparison would have told me: when a target's prerequisites are
  spread over several rules, **the prerequisites of the rule carrying the
  recipe come first** rather than file order; and gnu make leaves
  `MAKELEVEL` in the environment, so running the check from inside `make
  test` had it decide it was a sub-make and announce the directory it was
  entering -- a check that passed by hand and failed in the suite, which is
  the worst way round. and then **`make toolchain`**, which builds an
  executable with our make, our assembler and our linker and none of gcc,
  nasm, ld or gnu make in it, and compares the result against nasm and ld's:
  `.text` and `.rodata` identical. wiring them together found **three
  assembler bugs that comparing .text could never find**, because not one of
  them changes a byte of it. `global` was accepted and ignored, so every
  exported symbol came out `LOCAL` -- a name being defined here and a name
  being visible elsewhere are two different facts, and the linker's first
  words on the subject were "nothing defines helper". every relocation went
  into `.rela.text` whatever section it patched, which the manual check
  could not see because it counted relocations and got the same *total* --
  two and two is four either way -- while what it cost was .rodata's
  relocations applied at those offsets in .text, which is code overwritten
  with an address. and a `dq label` came out as a relocation against the
  *name* when the label was below it and against the *section* when it was
  above, for the same line of assembly, because "not defined yet" and "not
  defined anywhere" are not the same question and the middle of a pass
  cannot answer the second. so `asmcheck` now compares the object as well as
  the bytes -- relocations grouped by the section they patch, exported
  symbols by name and by section name rather than index, since nasm calls
  .rodata 2 and this calls it 6 and both are right -- and the three real
  kernel assembly files moved out of a procedure in a testing document and
  into `make test`, where they had needed to be since the assembler was
  written.

- **0.3.15**: a linker. the assembler's answer to a name it did not know
  was a *relocation* -- a note saying "the four bytes here are a reference
  to this name, whoever provides it" -- and this is whoever provides it. it
  does the three things: **placement**, since every object arrives claiming
  its sections start at zero and something has to make that a real address;
  **resolution**, where two definitions of a name is one error and none is a
  different one, and saying which is most of what makes a linker usable; and
  **relocation**, which is two lines of arithmetic that are easy to get
  backwards -- `R_X86_64_64` writes `S + A` and `R_X86_64_PC32` writes `S +
  A - P`, an address and a distance. it reads a script for the entry symbol,
  the base address, which output sections exist in what order, and any
  `ALIGN` between them. **and it writes an ELF, which is the first time this
  machine has produced one rather than only consumed one** -- program
  headers, an entry point, and section headers so the result can be
  disassembled, because a linker whose output `objdump` cannot read is one
  that can only be debugged by running it, which for kernel code means by
  not booting. **checked against `ld` the same way the assembler is checked
  against nasm**: `tools/linkcheck.py` links the same objects both ways and
  compares what actually gets *loaded* -- which byte ends up at each
  address, and what each address may be done to -- rather than the file,
  because how many `PT_LOAD`s a linker uses and which sections it groups
  into each are *decisions*, and comparing those would be comparing opinions
  rather than results. four things it settled that reading the code never
  would. **the gaps between sections are not zeros**: `ld` pads executable
  sections with multi-byte nops, and for good reason, since `00 00` decodes
  as `add [rax], al` and a gap of zeros is a stretch of code that faults if
  anything ever runs off the end of a function -- and the table of which nop
  for which size is not something i worked out from a manual, i generated
  every gap from 1 to 127 bytes, linked each with `ld`, and read back what
  it put there. **a segment is not a section**: sections are what a linker
  thinks in and segments are what the *loader* thinks in, and the loader
  only cares about permissions, so `ld` puts `.text` and `.rodata` in one
  and a segment per section would cost a page of file and a page of memory
  each. **two sections sharing a page cannot be given different
  permissions** however much they differ, so the split happens only when
  permissions differ *and* a page boundary separates them -- which is what
  makes the script's `. = ALIGN(4096)` load-bearing rather than tidy, and
  why a script that forgets it gets one writable executable segment and a
  warning instead of the protection it thought it asked for. and **`.bss` is
  an address and a length and a promise** rather than bytes, so a linker
  that writes it out turns the kernel's zeroed globals into megabytes of
  file. the fixture set has two scripts on purpose: one that packs
  everything into a single page and one that aligns each section onto its
  own. a linker that always started a new segment per section matched `ld`
  on the second and not the first, and that is the only reason the wrong
  rule was caught. what it does not read yet is the rest of a real script --
  `PHDRS`, `__text_start = .`, `KEEP`, `/DISCARD/` -- all four of which the
  kernel's own script uses, so linking the kernel with this is the next step
  rather than a finished one.

- **0.3.14**: an assembler, for three of the four files it exists for. the
  kernel has four that nasm builds and C cannot express -- the isr stubs,
  the context switch, the syscall entry, the trampoline -- and `switch.asm`,
  `syscall.asm` and `isr.asm` now build to objects **indistinguishable from
  nasm's**: every section byte for byte, and relocation counts of 0, 1 and
  257 against nasm's 0, 1 and 257. `trampoline.asm` assembles and diverges
  at byte 22, on 16-bit addressing mode, where ModRM is a different table
  entirely -- `rm=110` means disp16 and there is no SIB. **the only thing
  that makes any of this checkable is the comparison against nasm.** an
  assembler cannot be eyeballed: a diff prints something a person can read
  and a parser produces a structure that can be examined, but this produces
  *bytes*, and wrong bytes look exactly like right bytes until something
  executes them -- at which point the failure is a triple fault with no
  explanation attached. so `tools/asmcheck.py` assembles the same source
  both ways and compares, and it runs with `make test`. it is stricter than
  it sounds, because nasm *chooses*: `mov rax, 1` becomes the five-byte `mov
  eax, 1`, since writing a 32-bit register zeroes the upper half anyway, so
  matching nasm means matching its choices between valid encodings rather
  than merely emitting something legal. the version needed two things the
  roadmap entry did not mention. a **macro preprocessor**, because `isr.asm`
  is 256 stubs written once and there is no file to assemble at all without
  expanding it -- 30 lines become 1058. and **ELF object output**, because
  two of the four call into C: a name the file does not define is not an
  error but a *relocation*, which is the assembler's whole contract with a
  linker and is what makes 0.3.15 possible rather than necessary. also
  **branch relaxation**, since nasm picks the two-byte jump whenever the
  displacement fits and that choice *changes the distance* -- shortening one
  jump moves every label after it -- so the assembly runs repeatedly, every
  branch starting short and lengthening those that do not reach, until a
  pass changes nothing. **six bugs the comparison found that reading never
  would.** `swapgs` is three bytes and i had written two. `fgets` leaves the
  newline on, so the first mnemonic was `"ret\n"` and matched nothing in any
  table. case folding ran *after* names were read, so `TRAMP_BASE equ`
  stored `tramp_base` while `TRAMP_BASE+0x0f00` looked up `TRAMP_BASE` and
  the two never met. case folding also touched quoted literals, turning `mov
  al, 'A'` into `'a'` -- a wrong byte in a program that runs. ignoring
  `bits` inverted every operand-size prefix through the sixteen-bit half of
  a file, because in real mode the default width is 16 and in long mode it
  is 32. and the worst: a local `dq label` was resolved to the label's
  offset, when it must be a **relocation** -- an offset within a section is
  not an address, only the linker knows where the section lands, and a table
  of 256 of them would have jumped into whatever happened to sit at 0x9.
  nasm writes zeros there and 256 relocations, and nothing but the byte
  comparison would ever have shown it.

- **0.3.13**: memory under pressure. compiling is the first thing this
  machine will do that can genuinely exhaust memory, and the answer used to
  be "the allocator returns null and something gives up" -- survivable, and
  not *diagnosable*: the something that gave up might be the program, or the
  filesystem writing its output, or the console trying to say so, and which
  one it was decides whether you lose a compile or the machine. **a reserve
  is held back that only the kernel may draw on**, and the check goes in
  `populate` -- the one place a program's memory actually arrives, since
  every page a process ever gets comes through the fault path. so a program
  asking for more than there is fails *itself*, and the kernel still has
  enough to write the file, print the reason and reap it; without that, the
  order of failure is decided by whoever happens to ask next, which is how a
  machine that should have lost one compile loses the ability to say so. the
  rule is a **policy rather than a data structure**, so it lives in
  `kernel/src/mm/pressure.c` apart from the allocator and is checked at
  every edge -- exactly at the reserve, one page inside it, a request bigger
  than all of memory -- with no machine or page table near it. four breaks
  are caught, including the one easiest to write by accident: **the reserve
  is what would be *left*, not what is there now**, and checking the free
  count before the allocation still lets a program take the machine below
  the line. `mem` reports what a program may actually ask for rather than
  the free count, because printing only the free figure tells somebody they
  have memory they cannot have. and `ps` grew a memory column -- counted as
  pages arrive rather than worked out by walking four levels of page table
  per process, which is a great deal of work to answer a number that could
  simply have been kept.

- **0.3.12**: time that means something. `make` is the reason: it decides
  what to rebuild by comparing two numbers, and a clock that resets to zero
  at boot makes every one of those comparisons a lie -- every file looks
  like it came from the future, so everything looks up to date, so nothing
  is ever rebuilt. **the cmos chip is read once, at boot, and never again**;
  everything after adds the timer's uptime to that seed. the filesystem had
  been reading the chip *per file stamped*, which is wrong twice over -- a
  device access each time, and the chip's second hand ticks on its own
  schedule, so two reads close together can give the same second twice or
  skip one. a clock that sometimes runs backwards is worse than none at all
  for anything comparing two times. when the time is not known it answers
  **0** rather than something plausible: a wrong number that cannot be told
  from a right one is worse than no number, and the whole value of a
  timestamp is that it can be trusted. **the conversion has a tested core**
  in `kernel/src/lib/epoch.c`, checked against the host's own `timegm` --
  somebody else's implementation, and the only opinion worth having about a
  date. seven breaks are caught, and the property that matters is the round
  trip: every day of sixty years turned into a number and back must be the
  same day, about twenty-two thousand of them, which is establishable by
  loop rather than by argument. the two everybody gets wrong are in the
  fixtures -- **1900 was not a leap year and 2000 was** -- and an invalid
  date is refused rather than corrected, because the 30th of february
  silently becoming the 2nd of march is how a file ends up stamped with a
  date nobody chose. **`alarm` is a deadline rather than a countdown.** a
  countdown has to be decremented by somebody on a schedule, and every tick
  touching every process is a cost the machine pays whether or not anyone
  set one; a deadline is compared instead, and only where something is
  already looking -- on the way out of a syscall, where signals are
  delivered anyway. it is measured in uptime rather than wall-clock, so
  "wake me in five seconds" survives somebody setting the date, and it
  returns what was left on the previous alarm, which is what a program
  restoring one it displaced needs to know. four breaks are caught there,
  including the one that matters most: an alarm fires **once**, and a
  repeating one is a different thing nobody asked for. what is still resting
  on its suite rather than on a running program: nothing in the ramdisk
  calls `alarm` yet.

- **0.3.11**: the small tools, and the kernel bug the first of them found.
  `less`, `diff` and `find`: none of them interesting alone, and all of them
  missed the first afternoon spent working *inside* the machine rather than
  on it. **`diff` has a tested core** in `user/difflib.c`, because the
  property that matters is checkable exactly -- the edits it produces,
  applied to the first file, must give the second. that is not "does it spot
  a change", since anything spots a change, and it is what separates a diff
  from something printing plausible-looking output; every fixture in the
  suite replays the answer and compares. the reason it is not a line-by-line
  walk is the case that makes diff worth writing at all: insert one line at
  the top and a comparison in step reports every line changed, which is true
  and useless. so it is longest-common-subsequence, and honestly the
  *textbook* O(n·m) version rather than Myers' -- it is here because it is
  correct and obvious, and a wrong diff is worse than a slow one. over four
  thousand lines it refuses and says so rather than allocating a table that
  would take the machine down. six breaks are caught, two worth naming:
  comparing lines only to the shorter length, which is the classic way to
  miss a truncation and makes "ab" and "abc" read as identical; and the
  tie-break that prints a change as a removal followed by an insertion,
  which is what every diff has done since the seventies and what anybody
  reading one expects. `find` walks with a depth bound rather than trusting
  the filesystem, since a directory loop would otherwise walk until the
  stack ran out, and takes a substring rather than an expression language --
  `find . -name '*.c' -type f -print` is a miniature programming language,
  and when substring matching stops being enough the answer is a real parser
  rather than three more flags. `less` pages with the raw mode and
  incremental drawing of the last two versions and reuses `textbuf`, which
  now serves the editor and the pager both. **and then `diff` found
  something older than any of it.** it is the first program in this
  project's history to hand *malloc'd memory to a syscall*, and every read
  was refused. memory from `mmap` is **reserved rather than mapped** -- the
  page arrives when the program first touches it, which is how a program
  asks for a megabyte and pays for what it uses -- so `user_range_ok`
  checked the page tables, found no entry, and refused a pointer that was
  perfectly legitimate and simply had not been faulted in yet. nothing had
  ever reached it because nothing had ever done it: `wordcount`, my own
  demonstration of the libc in 0.3.8, reads a byte at a time into a *stack*
  variable and copies it into its heap buffer, so the heap pointer never
  crosses the boundary. a missing page is faulted in now, the same way
  touching it would, and then re-checked; an address in no region at all
  still fails, so a genuinely bad pointer is still caught. the diagnostic
  did exactly what it was written for -- the comment above it says a silent
  refusal is how the 0.1.0 version of this bug stayed hidden, and because it
  printed the pid and the address the fault was identifiable from two lines
  of a boot log.

- **0.3.10**: margaret, grown up, and a console that stopped repainting
  itself. the old editor held a file in `char text[600][240]`: 140 kilobytes
  of static array, six hundred lines at most, and anything past a line's
  240th character silently thrown away -- which was not carelessness, since
  there was no allocator in ring 3 until 0.3.8 and a program's memory was
  whatever it declared at compile time. the text lives in `user/textbuf.c`
  now, **separately from the drawing, because that is the half that can be
  tested**: an editor is a drawing loop around a data structure, the drawing
  needs a terminal and the structure needs nothing, and the structure is
  where a bug quietly eats somebody's file. **undo records operations rather
  than copies** -- keeping copies is what a first attempt always does and
  costs a whole file per keystroke, which is precisely the case this version
  exists to support -- so each edit knows how to reverse itself, and that
  gives undo a property nothing else here has: it can be checked *exactly*.
  forty mixed edits, undone one at a time, must arrive back at the original
  text byte for byte, and no amount of trying it by hand finds a wrong
  inverse. ten breaks are caught. three first passed for the wrong reason: a
  delete recorded *after* removing its text reads whatever followed it and
  the round-trip survived by luck because the lengths matched; the "last
  line cannot be deleted" rule was checked in two places and the public one
  masked the other, the same duplicate-guard pattern as the dns and http
  dead checks; and the test's own baseline was an edit still on the undo
  stack, which is not a test bug but a **missing operation** --
  `textbuf_forget_history`, without which opening a file leaves the reading
  of it on the stack and enough presses of undo empties a file nobody
  touched. **the version's real finding was in the kernel.** rewriting the
  editor showed that wiring the escape parser in for 0.3.9 had made *every
  cursor move repaint the whole screen* -- filling the framebuffer and
  blitting all two thousand glyphs -- so an editor that emits a cursor move
  before each line it draws turned a twenty-line update into twenty full
  repaints: forty thousand glyphs to change twenty rows. the incremental
  drawing was working perfectly and paying for a full redraw twenty times
  over. and `sys_write_console` was `kprintf("%c", ...)` in a loop, parsing
  a format string and taking a lock **per character** a program printed.
  both had been slowing down every program, not only this one. removing the
  repaint then took the *cursor* with it, because the block cursor is a
  drawn thing that covers the character in its cell and the repaint was what
  redrew it -- moving it is an erase and a draw, two cells rather than two
  thousand. one more 0.3.9 bug surfaced: `tty_intercept` never read the
  terminal mode, so a program could ask for raw keys, be told yes, and still
  have ctrl+z taken away before it arrived -- an editor binding it to undo
  was suspended instead. the shortcut rows at the bottom follow nano's
  arrangement, which is convention rather than invention: somebody who has
  used nano can use this without being told. nano's *code* is somebody
  else's and stays that way; what is worth taking is the layout its users
  already know. still missing, and written into the roadmap rather than left
  implied: cut and paste, which the old margaret had and the textbuf has no
  buffer for.

- **0.3.9**: a terminal that behaves. the console understood **no** escape
  sequences at all: a program that cleared the screen printed `[2J`, one
  that moved the cursor printed `[10;5H`, and that is exactly why `margaret`
  works here and nothing written for a real terminal would -- the editor was
  written against what this console does rather than against what a terminal
  is. there is a parser in front of every byte now: clear, cursor movement,
  erase to end of line, save and restore, hide and show. **it is a state
  machine for the same reason the http reader is**, and the shape keeps
  recurring -- a sequence arrives as bytes and nothing guarantees they
  arrive together, so `\033[2J` is four bytes a program may write with four
  separate calls, and the suite feeds every fixture *one byte at a time*.
  seven deliberate breaks are caught, and the subtle one is that **an absent
  parameter is not a zero**: `ESC [ A` means up one line, and treating the
  missing parameter as 0 moves the cursor nowhere. an unrecognised sequence
  is swallowed rather than printed, because garbage on the screen is worse
  than a thing that did not happen. two of my own fixtures were wrong: the
  runaway-sequence test used letters, and a letter *ends* a sequence
  immediately, so it passed with the length bound removed -- only digits and
  semicolons can actually run away; and an escape arriving mid-sequence was
  being taken as that sequence's final byte, so the `[` after it printed as
  a stray character. **raw and cooked are a mode, not two syscalls**, which
  is what the roadmap asked for and is the part worth getting right: two
  calls would mean every program choosing between them at every read, and
  one that chose wrong once would hang. three flags rather than one mode
  word, because they are independent -- a program can want keys as they
  arrive and still want to be killable, and a password prompt wants cooked
  input with echo off. the decisions live in `drivers/termios.c`, which
  knows nothing about buffers or screens, so the rule that ctrl+c is a
  signal in one mode and the byte 0x03 in another is in one place and is
  checked with no machine underneath it. the mode belongs to the **console
  rather than the process**: a program that dies in raw mode has to leave
  something the shell can put right, and a per-process mode would vanish
  with the process and leave nothing to repair. one call sets and gets, so a
  program can restore what it was given without a second syscall and a race
  between them. `winsize()` finally answers how big the screen is --
  `console_size` had existed in the kernel for versions with nothing
  exposing it. **a bug the tty suite caught**: echoing the character before
  dispatching on the action printed the newline twice, once as the character
  and once as the line ending -- and fixing that left the original echo
  still in place further down, so every character appeared twice instead.
  what is still owed: sgr colour is parsed and then ignored, so the sequence
  no longer lands on the screen as text but the colour does not change
  either; and nothing in the ramdisk asks for raw mode or the window size
  yet, which makes 0.3.10 the first real exercise of either.

- **0.3.8**: a libc worth linking against. every program in `user/`
  included `syscall.h` and wrote its own everything, and a compiler cannot
  be written that way. so: `string`, `stdlib`, `stdio`, a formatter, and a
  crt0 -- and the test of it is not a feature list but whether **a program
  written for another unix has a chance of building here**. **the smallest
  file is the one that answers that**: `start.c` defines `_start`, calls
  `main`, and exits with what it returned. every program here used to be
  entered at `_start` and had to call `exit` itself, which no program
  written anywhere else does, so each one needed editing before it would
  build; that edit is what this removes. **the library is an archive, and
  that is load-bearing rather than tidy** -- an object named on a link line
  is always included, but an archive member is pulled in only if it resolves
  something still undefined, so a program that writes its own `_start` never
  drags start.o in and never sees a duplicate, while one that writes `main`
  gets it. i linked the objects directly first and broke every existing
  program at once. the headers are on the include path, so a program says
  `#include <stdio.h>` and means this machine's; `user/wordcount.c` is the
  demonstration, with a `main` that returns an int, `malloc`, `printf`,
  `strtol`, and not one line that names a syscall or knows which kernel it
  is on. **the allocator** takes memory from `mmap` and never gives any back
  -- `munmap` exists, and using it would mean tracking which blocks came
  from which mapping and releasing one only when every block in it is free,
  which is a real allocator's job and this is enough to build a compiler
  with. free blocks are kept on a single list **in address order**, which is
  the one decision that earns its keep: coalescing needs neighbours adjacent
  in the list as well as in memory, and without it a program that allocates
  and frees in a loop -- which is every program -- ends up with thousands of
  blocks too small to use and a heap that only grows. that failure mode is
  invisible to any test that merely allocates and frees, so most of
  `tests/test_libc.c` is about it: eight blocks freed newest-first,
  oldest-first and shuffled must all end as one, and a megabyte allocated
  and freed two hundred times through a four-megabyte arena must never run
  out. **two real bugs the tests found.** the formatter read a `long` for
  `%d`, and `printf("%d", n)` passes an `int` -- which occupies the low half
  of its vararg slot with the top half undefined, so it works until it does
  not; length modifiers are read now rather than skipped. and my own
  coalescing test asserted the wrong block count, because the tail of the
  chunk the allocator asked the kernel for is itself adjacent to the last
  allocation. the allocator wants exactly two things from the world, so it
  gets them through one named seam -- `libc_get_pages` -- rather than
  including `syscall.h`, every declaration of which collides with a host
  libc's and would have made the file untestable; what the suite exercises
  is the real allocator with only the source of memory replaced. the
  formatter is a second implementation of something the kernel already has,
  which is a real cost and the honest price of a kernel and a userland that
  share no code.

- **0.3.7**: signals. there was one flag called `interrupted` and it was as
  close to a signal as this kernel got: ctrl+c set it, and a program found
  it the next time it asked the kernel for anything -- enough for `cat`, and
  not enough for anything that runs for a minute. **a signal is a bit and
  not a message**: no queue, no payload, and raising one already raised
  changes nothing, so two ctrl+c presses in the same instant are one
  interrupt. that is correct and it is the first thing people expect to be
  wrong. **SIGKILL and SIGSTOP cannot be caught, blocked or ignored**, which
  is not an arbitrary rule but the entire reason a machine can still be
  saved by somebody who can type -- and installing a handler for one is
  *refused* rather than accepted and quietly dropped, because a program told
  yes and then killed anyway has been lied to. **delivery happens on the way
  out of a syscall**, because a handler runs in ring 3 on the program's own
  stack and that is the only place the kernel has those to hand and is about
  to give them back: the interrupted registers go on the program's own stack
  below the red zone, and the handler is entered as though *called*, with a
  return address pointing at a stub in the program itself -- which has to
  come from the program because there is nowhere else to put it, the stack
  not being executable and there being no page of kernel code mapped into
  every program. **and the roadmap was right about which half was hard.**
  delivery is thirty lines; the rest is that every blocking call here was
  written assuming it ends for one of two reasons -- what it waited for
  happened, or the thing it waited on went away -- and a signal is a third.
  so a blocked thread carrying a deliverable signal is now *woken*, or a
  signal sent to a program parked on a read arrives whenever that read
  happens to finish, which for one waiting on the keyboard is never. eleven
  deliberate breaks are caught by the rules suite: a catchable SIGKILL, a
  blockable SIGKILL, a handler in the table escaping it, a handler failing
  to block its own signal (ctrl+c held down would re-enter it until the
  stack met its guard page), a mask cleared instead of restored, SIGCHLD
  defaulting to death (which would have every shell in history exit the
  moment a command finished), pending signals inherited across fork, exec
  forgetting that a signal was ignored, and delivery order reversed. **the
  mistake worth recording**: i made `process_take_interrupt` stop consuming
  its flag, reasoning that the signal is consumed elsewhere -- and several
  callers loop until it clears, so the syscall suite hung on a `write` that
  had nothing to do with signals. the flag and the signal are two different
  facts and both have to be consumed, in different places. one divergence
  from unix is deliberate and written down rather than hidden: an *ignored*
  signal still cuts a blocking call short, because `interrupted` predates
  signals here and answers "stop waiting" rather than "what becomes of this
  process". `user/patience.c` is the program the version exists for -- it
  counts slowly, acknowledges the first interrupt and agrees to the second
  -- and `signal <pid> <name>` sends one by hand, named rather than numbered
  because `signal 7 9` is a line nobody can read back.

- **0.3.6**: fetching something. `fetch <url> [file]`, and the point is not
  the web: a machine that can pull a file down is a machine that can be
  **given** things, which is how everything after this gets easier. it is
  also the first command that uses the whole stack at once -- a name
  resolved, a route out, a connection opened, a stream read in whatever
  pieces it arrives in, and a file written. **the response reader is a state
  machine rather than a parser**, and that is the whole of what makes it
  interesting. every other format in this kernel arrives whole, so parsing
  one is a function from bytes to meaning; a http response is not handed
  over at all, and the same response is legally the same whether it comes as
  one buffer of nine hundred bytes or nine hundred buffers of one. so the
  central test is not "does this parse" but **does the split point matter**
  -- every fixture is driven through split at every single byte offset and
  required to give an identical answer, which is a property that cannot be
  established by ordinary testing and has to be searched for. both body
  framings work: `Content-Length`, and `chunked`, which is not optional to
  support since asking a modern server for anything very often gets it --
  and a body with neither ends when the connection does, which is http/1.0's
  original answer and still legal. **fourteen breaks are caught, and four of
  them first passed for the wrong reason.** the absurd-length fixture used a
  number that wraps to something *large*, which the close catches anyway; it
  uses 2^64+5 now, which wraps to **five** and would read five bytes and
  call the response complete. the over-long-header fixture truncated to
  `X-Long: aaa...`, which means nothing to anybody either way; it truncates
  to a *content-length that is wrong* now, which is the actual reason a
  half-received header must not be acted on. and one more piece of dead
  code, the third this month: an explicit full-output-buffer test that could
  not fail on its own, because a full buffer already stops the loop by way
  of `take` being zero. **and booting it found a real bug in 0.3.3's tcp.**
  four failed fetches filled the connection table with entries that could
  never finish: nobody read the responses, a full receive buffer advertises
  a window of zero, a peer with a zero window cannot send its fin, and a
  connection waiting for a fin that cannot be sent holds its slot until the
  machine is rebooted. so data arriving after the program has closed is
  acknowledged and *discarded* rather than buffered -- the sequence number
  advances, the window stays open, the fin behind it can be sent -- and
  closing with data still unread sends a **reset** instead of a fin, because
  those bytes were acknowledged and then thrown away and a fin would claim
  the conversation ended properly. the oldest TIME_WAIT entry is also
  reusable under pressure, since four slots and a ten-second wait means four
  fetches in ten seconds is a machine that cannot open a connection. two
  smaller ones in `fetch` itself: the file was created *before* the request,
  so a fetch that 404ed truncated a good file on its way to failing and left
  it empty -- it is created only when there is a body byte to put in it --
  and "cannot write to that" now says the directory has to exist, which is
  what it actually meant. **no https**, and that is a real limit rather than
  a deferral: there is no tls here, most of the internet answers with a
  redirect to a scheme this cannot speak, and a redirect is reported with
  its Location rather than followed, because a fetch that quietly went
  elsewhere hands you something other than what you asked for.

- **0.3.5**: names, and a way off this wire. dns, so `host example.com` and
  `ping google.com` mean something. this is the first protocol here whose
  other end is software written by somebody else years ago, and `dns.c`
  believes as little of the answer as it can: the **id and the question are
  both checked** against what was asked, which is not politeness but the
  whole of what stops a forged reply being taken, since anybody who can
  guess the id and get a packet in first wins otherwise. **name compression
  is the entire danger** -- a name may be cut short by a pointer elsewhere
  in the message, and nothing in the format forbids that pointer going
  forwards, backwards or at itself, so a decoder that simply follows them
  can be hung by a fourteen-byte packet. every jump is counted and bounded,
  and a name that has not ended by then is refused rather than truncated: a
  truncated name resolves to *something*, and something is what gets
  connected to. **two checks turned out to be dead code**, found by breaking
  them rather than by reading -- a bound on the pointer target duplicated
  the test at the top of the decoding loop, and an explicit reject of the
  reserved 0x40/0x80 patterns was subsumed by the label-length rule since
  both are numbers above 63. neither could be made to fail on its own, so
  both are gone and the reasoning stayed in a comment. **six of eleven
  breaks initially passed for the wrong reason**, which is the real finding
  of this version: two length checks were catching *each other*, since one
  fixture with a huge rdlength is refused by either alone; a buffer-overflow
  fixture overflowed on a middle label where the dot-separator check caught
  it a moment later, and has to be the *final* label to mean anything; and
  several bounds change no answer when removed, only reading past the
  message -- invisible in a 600-byte fixture array even under a sanitizer,
  so those parse out of exact-sized allocations where asan says so. one test
  was passing against my own stub: the shell suite stubs `net_resolve`, and
  my stub had copied the real short-circuit for addresses, so "an address is
  not a question" would have passed with that check deleted. **the version's
  own bug was the waiting.** the first resolver parked the asking thread on
  a waitq whose deadline was only checked *on waking*, and nothing woke it
  when no reply came -- so one `host` against a silent server hung the shell
  for good, and left `waiting` set so every later name was told somebody
  else was already asking. a timeout that is only checked when something
  else happens is not a timeout. the poll thread wakes the queue every run
  now, a jammed query can be taken over rather than owning the resolver
  until reboot, and `ifconfig` shows the name server, since "was one offered
  at all" is the first question and there was nowhere to read the answer.
  **and 0.3.6 needed a route this version did not plan for**: everything off
  this wire had been refused since 0.2.x and the gateway dhcp offers had
  gone unused since 0.3.2, so `ip_next_hop` decides where a datagram is
  handed next -- on this wire, arp asks for the destination; anywhere else,
  arp asks for the *gateway* while the ip header still names where it is
  really going. those two addresses answering different questions is the
  whole of routing at this scale. the decision is pure and lives in `ip.c`,
  so it is tested with no machine underneath it. one number in a boot log
  was worth chasing: a successful lookup reported *two* questions asked in
  fifty milliseconds, which cannot both be true -- the first send returns
  false while arp resolves, and the loop then waited a whole second for a
  reply to a datagram that never left. fixing it introduced the same hang a
  second time, in a different function, until the retry got a bound of its
  own.

- **0.3.4**: sockets that wait. `recvfrom` used to say "not yet" so the
  program could ask again, which is a poll wearing the clothes of an api --
  honest only because there was no way to park a thread on a socket. now
  there is: **a waitq per socket**, so a program blocked on one port is not
  woken by traffic for another, living in `netif.c` rather than in
  `socket.c` because the pure half stays pure and a `struct waitq` in that
  struct would drag the scheduler into a file whose whole value is not
  needing one. **`select` waits on several at once**, which is what every
  server is -- and the roadmap's "a waitq per socket" turns out to be right
  for a blocking read and wrong for this, because a thread can only be
  parked on *one* queue: the wait list is threaded through the thread
  itself. so select waits on a shared queue and looks again when anything
  arrives, which is more wakeups than it needs and nothing worth measuring
  at eight sockets. tcp got its ring-3 interface in the same version --
  `connect`, `listen`, `accept`, `send`, `recv`, `shutdown` -- because a
  stream read that could not block would have been the same mistake twice,
  and **a read of 0 means the stream ended** rather than failing, which is
  the one distinction every program using this depends on. a blocking tcp
  read also has to wake on more than arriving data: the other end closing,
  or the connection being reset, both have to end it or it never returns.
  every call takes a timeout, where 0 is exactly the old look-and-return
  behaviour, kept because a program that wants to glance and carry on should
  not have to spawn a thread for it. **two shapes are honest about table
  sizes rather than general**, and both are written down where somebody will
  hit them: tcp handles are offset by 64 so one number space serves both
  kinds, which is blunter than a per-process handle table and right for
  eight sockets and four connections -- the alternative is a second table to
  keep in step, whose failure mode is a handle naming the wrong thing; and
  `accept` hands back the *same* handle, because a listening entry becomes
  the connection when a syn arrives and there is no backlog. **a trap built
  and removed**: `recvwait` first packed its length and timeout into one
  register on the belief that the call wanted six arguments, when a fifth
  argument register was already in use elsewhere -- silent truncation for no
  reason. `socket_deliver` changed from a yes/no to *which socket took it*,
  since there is now somebody to wake, and that is its own trap: socket
  nought answers `0`, which every existing `CHECK(socket_deliver(...))` read
  as a failure -- the return value changed meaning without changing type in
  a way the compiler could see. `user/theodore.c` is the program the version
  exists for, holding a tcp port and a udp port at once and sleeping until
  either has something; `make run` forwards host ports 5555 and 5556 in, so
  the machine can be connected *to* rather than only connecting out -- and
  that forward is the only way in, because qemu's user-mode networking is a
  userspace nat and the guest's own address is not one the host can route to
  at all.

- **0.3.3**: tcp: the first thing here that has to remember what it said
  and be prepared to say it again. a **sequence number counts bytes rather
  than packets**, which is what lets a retransmission carry the same number
  as the original and lets the same stream arrive as one segment or forty
  and mean the same thing; syn and fin each take a number too, though they
  carry no data, which is what makes them acknowledgeable by the same
  arithmetic and leaves one retransmission mechanism rather than three. and
  it **wraps** -- nothing may say `a < b` about two sequence numbers, since
  0xffffffff comes *before* 0x00000001, and this is the easiest thing in the
  protocol to get wrong because it is right for the first four billion
  bytes. eleven states, **seven of them about stopping**, which is the
  honest shape of it: opening is three messages and closing is a
  negotiation, because either end may stop while the other carries on.
  `tcp.c` is pure, told the time and handed segments, and returns what to
  send -- the payoff is far bigger than dhcp's, because the retransmission
  timer, the TIME_WAIT expiry and a four-billion-byte wrap are three things
  a real clock will never show you. **eighteen deliberate breaks are caught,
  and the close sequence was where every real bug was** -- all three
  invisible to anyone watching a connection work. TIME_WAIT answered nothing
  at all: `tcp_tick` returned early for the whole state, so it sat there for
  ten seconds being all of the cost and none of the point while the other
  end retransmitted its fin until it gave up. a *retransmitted* fin was
  ignored, because it carries the sequence number it always did, which is
  now one behind what this end expects, so it failed the in-sequence test --
  anything already seen is acknowledged again now. and a bare ack set
  `ack_due`, so every ack received produced an ack back, forever: the only
  bug in the file that gets *worse* the better the network is. **four of my
  own tests passed for the wrong reason** and are fixed: the data-offset
  fixture was refused for a bad checksum rather than the bounds check, and
  an attacker gets to compute a correct checksum so the fixture has to as
  well; the TIME_WAIT test checked the *report* and not the state
  transition; the fin-ordering test was enforced by statement order rather
  than by the guard; and the mss limit had nothing writing more than 1460
  bytes. **and the version found something bigger than itself.** breaking a
  function in `tcp.h` on purpose and watching the suite pass anyway revealed
  that test binaries had *no header prerequisites at all* -- the kernel's
  objects rebuild from -MMD dependency files and the tests are compiled
  straight to a binary in one step, so for the entire life of this project,
  editing a header and running `make test` ran the previous binaries and
  reported them passing, which is indistinguishable from the change being
  harmless. every test depends on every kernel header now. out-of-order
  segments are dropped rather than held, which is legal and removes an
  entire reassembly structure; there is no ring-3 interface yet, because a
  read that does not block would turn every program into a poll loop and
  that is 0.3.4's problem.

- **0.3.2**: dhcp, and the first protocol here that is a *conversation*.
  everything else in `net/` is a packet with fields: you are handed bytes,
  you work out what they mean, and you are done -- nothing has a yesterday.
  this one has four messages, five states, retransmission and a lease. the
  request is broadcast rather than sent to the server that offered it, which
  looks wrong until you see why: any other server that also offered has to
  hear it was not chosen, or it sits holding an address reserved for a
  machine that is never coming back. **the chicken and the egg is the shape
  of the whole thing.** a machine doing this has no address, which every
  layer underneath assumes it has -- `net_send_ip` refuses when the
  interface is down, resolves through arp, and drops anything not on this
  wire, and all three are correct and all three are fatal here, because
  there is nothing to send *from*, arp cannot ask on behalf of a machine
  with no address, and 255.255.255.255 is on no wire in particular. so dhcp
  goes round it with a broadcast path of its own, and a renewal goes the
  ordinary way because by then there is an address. `dhcp.c` is **pure**:
  told the time and handed packets, it *returns* what should be sent and
  never sends anything or asks the clock. that is not tidiness --
  retransmission is the half of dhcp that is easy to get wrong and
  impossible to test against a real clock, and with the clock passed in a
  full backoff and a twelve-hour lease run in microseconds. fourteen
  deliberate breaks are caught and one is honestly not: the guard on reading
  an option's *length byte* changes no answer, since the next check catches
  the same message, and its only effect is a one-byte overread -- out of a
  512-byte fixture even a sanitizer cannot see it, so the malformed fixtures
  are parsed out of allocations that end where the message ends, and then
  `-fsanitize=address` says so. two of my own fixtures were wrong: the
  oversized-option test passed *for the wrong reason*, because the bad
  option was the message type and skipping it left no type at all, so it
  never reached the bounds check. **three bugs the tests found in my own
  code**, two of them in what the machine *says* rather than what it does.
  the backoff doubled a step early, turning a seven-second give-up into
  fourteen, which reads as a slow server rather than a bug. a renewal
  reported itself as a new address, which would have reconfigured the
  interface and reset the arp cache every half-lease. and on the first boot
  the lease message printed about a second in -- which is exactly when the
  login prompt is waiting for a name, so it landed *in the field being typed
  into* and the first login failed for a reason nothing on screen explained;
  there is a `klog_printf` now that reaches the log and the serial line and
  never the screen, because `kprintf_to_console` is global and a service
  thread lowering it would take the screen from whatever a shell was
  printing. the last one is the version in miniature: `dhcp` printed a lease
  that had been given up exactly like a live one, down to "renewed at half"
  for a client that will never renew again.

- **0.3.1**: an interrupt instead of a poll. the card was watched by a
  thread twenty times a second: late by up to fifty milliseconds, and a
  wakeup burned whether or not anything had arrived. it can raise an
  interrupt instead, and the whole design is **what a handler is allowed to
  do**: this one reads the cause register and wakes the poll thread, and
  does not parse an ip header, answer an arp or take the interface's lock. a
  handler doing that much with the timer held off is why 0.3.0 polled at
  all; the answer was not to do it in the handler but to have the handler
  wake somebody who may. that is only legal because of something the
  scheduler already did: all seventeen acquisitions of `sched_lock` take it
  with interrupts off, so this interrupt cannot land on a core holding the
  lock the wake needs: which is a rule worth *checking* before relying on
  it rather than after. three things had to be true at once and each is a
  card that answers every register read and never interrupts: `INTX_DISABLE`
  cleared in the pci command register (nothing here had ever needed to), the
  legacy line unmasked, and the card's own `IMS` admitting the causes.
  **`make checkarch` failed the moment the handler existed**, and it was
  right: taking an interrupt meant naming two x86 headers. the fix came from
  noticing that not one of the five handlers in this kernel had ever *read*
  the register frame it was handed: so they take nothing now,
  `irq_register` + `pic_unmask` became a single portable `irq_install`, and
  the "irq 12 is on the second pic" cascade rule moved out of `mouse.c` to
  where the next driver on a high line gets it for free. the e1000 uses no
  port i/o, so it now takes an interrupt without naming the architecture at
  all. **the part that cost an evening was the diagnostic.** the poll loop
  stays as a fallback on a half-second timer, so a machine whose interrupt
  never arrives still works: and looks exactly like one where it does. so
  `ifconfig` reports which, and the first cut of that reported *three*
  states as two: a card the firmware gave no line, and a card wired up
  correctly that has simply not been sent anything, are not the same answer.
  under qemu's user-mode networking nothing arrives unasked, so the second
  is the normal state of a freshly booted machine, and I went looking for a
  fault in a working one. it also printed below the statistics, where a
  machine with no address returns before reaching it: `ifconfig` with no
  argument being the first thing anybody types. two smaller things fixed on
  the way past: an out-of-range irq left the line number set, so the
  diagnostic would have claimed the card was armed on irq 20; and `stats`
  was cleared *after* the card was armed, which loses the first interrupt:   on a quiet wire that may be the only one for a while.

- **0.3.0**: **local networking, and the project is velvetOS now.** the
  name first: it was tinyOS. the rename touched forty-two files and none of
  the changelog entries below, because not one of them had ever named the
  project -- they are written about what changed rather than about what it
  is called, which meant there was nothing here to rewrite. this changelog
  also *moved here* in 0.3.0; it lived at the bottom of README.md until it
  was fifty-four entries and more than half the file. now the wire. a
  network card found the way the disk was -- walk pci, match a vendor, take
  a bar -- and then arp, ipv4, icmp and udp, all of which turn out to be
  **arithmetic over a byte buffer**: no card, no kernel, and therefore
  testable against packets laid out by hand from the rfcs. that is how
  `net/` is shaped and why not one file in it names x86. thirteen deliberate
  breaks are caught, and three gaps my own tests missed are now closed --
  the checksum's carry fold needs a *loop* (one pass is right for every
  small packet and wrong near an mtu), the pseudo-header has to be summed
  with the datagram (a stack that skips it accepts datagrams delivered to
  the wrong host, which is the one thing it exists to prevent), and a
  computed checksum of zero goes out as all ones. I also nearly told a lie
  in a comment: the fixtures were described as "captured packets" and they
  are not -- I have no capture, and the bytes I wrote from memory carried a
  wrong checksum, which the test failed on. they are laid out from the rfcs
  now and say so. **four things only a real machine found.** the card
  answered every register read, reported its hardware address, and moved not
  one frame in either direction: **pci bus mastering** was never enabled,
  because `pci.h` had no config *write* at all -- ahci never needed one
  since firmware enables it on whatever it boots from, and a network card
  gets no such favour. a program faulted on an instruction fetch inside its
  own argv: `hermes` was written with a `main` and there is no crt here, so
  the linker could not resolve `ENTRY(_start)`, defaulted the entry to the
  start of `.text`, and ran `write_num` with argc in the register it wanted
  a number in -- and `ld` had warned exactly that, in a line I filtered out
  of the build output. sending to this machine's own address could never
  work because there was no **loopback**: arp asks who has an address nobody
  will claim, since a card does not hand back what it just sent. and `ping`
  from the shell tripped the lock rank rule, because stamping a file reads
  the cmos clock while the disk lock is held. `ifconfig`, `ping` and `arp`
  are shell commands; `hermes` is the first ring-3 program to use the wire,
  over three syscalls -- `socket`, `sendto`, `recvfrom` -- and no `connect`
  or `accept`, because udp has neither. a socket is a port, an owner and a
  fixed queue, and a datagram arriving at a full one is dropped and counted,
  which is what udp promises anyway.
- **0.2.21**: a live mode, and an installer. it was meant to be a small
  step and two things were missing, neither small: this kernel could
  **read** a partition table and not write one, and **mount** a filesystem
  and not make one: every filesystem it had ever seen was formatted by a
  python script on a development machine. so most of the version is
  `mkfs_ext2`, mke2fs in the kernel, making an *empty* filesystem and
  nothing else; copying files in goes through the ordinary
  `vfs_create`/`vfs_write` from 0.2.14, because a formatter that also laid
  out files would be a second implementation of the half that works. the
  verification is the part worth keeping: a formatter and a driver by one
  author can agree on something wrong and both be happy: precisely what
  happened to `mkfat.py` in 0.1.10: so the result is checked by the
  arithmetic, by `fs/ext2.c` which is a separate implementation written a
  version earlier, and by `tools/readext2.py`, written from the on-disk
  format and sharing code with neither, now run over a kernel-formatted
  image as a `mkfsck` step. the installer itself is three steps that each
  already had a test plus one property none of them can see: **the order**.
  an mbr table lives at offset 446 of the first sector, which is also
  philemon's sector and the first thing the copy overwrites: write the
  table first and the copy erases it. philemon's first stage ends at byte
  365, which is not luck but the constraint every bootloader sharing a disk
  with a table has worked under since 1983. `part_write_mbr` therefore reads
  the sector, replaces its last 66 bytes and puts it back, and refuses a
  disk carrying a gpt, since a protective mbr exists so that a tool which
  cannot read gpt declines. and live-or-installed finally means something: a
  drive carrying philemon's table at sector 32 is a boot medium, the machine
  is installed when that is also where the root filesystem came from, and
  the boot log says which: it is the difference between a machine whose
  changes survive a reboot and one whose changes do not.
- **0.2.20a**: the boundary, checked two ways instead of one.
  `tools/checkarch.py` greps, so it cannot see a portable file that
  *depends* on x86 without saying so: `make portable-check` compiles the
  portable kernel against `arch/none/`, where every function is empty and
  every constant is a plausible lie, and 35 files build with no machine
  underneath them. it found a leak on its first run, in code from the day
  before (`thread.c` calling `smp_tlb_shootdown()`, invisible to the grep
  because the *include* was portable), then linked what it compiled and
  collected the undefined symbols: which is the porting checklist derived
  mechanically rather than remembered. an **aarch64 port was written,
  compiled, linked, never ran, and was removed** (see ROADMAP.md: I cannot
  test arm on this hardware, and a port only one of us can build rots
  silently while looking finished). three things it found stayed: `thread.c`
  was **building an x86 stack frame by hand** in portable code: six zeroes
  named `rbp, rbx, r12…` and a return address for `ret` to pop, where
  aarch64 has ten callee-saved registers and returns through a *register*;
  no checker could catch that, since it is integers written into memory, and
  it is `context_make_stack()` now. `kmain` was a machine description with a
  kernel wrapped round it: three hooks now, and `main.c` came off the allow
  list. and `drivers/serial.c` was a **terminal with a chip stuck to it**:
  an 8250 is not an x86 chip, *reaching* it through a port space is, so the
  escape-sequence machine stayed portable and the `outb`s went to
  `arch/x86_64/uart.c`. that is the drivers question 0.2.20 deferred,
  answered for one of them. 77 portable files and 8 still naming x86, from
  75 and 10.
- **0.2.20**: the x86 parts, in one place. an `arch/` boundary drawn **from
  the outside in**: the headers are named for what the kernel wants: may
  interrupts happen, stop until something occurs, this mapping is stale,
  land the kernel here when this thread traps: rather than for what x86
  provides. drawn the other way round it produces a `write_cr3()`, which is
  an x86 instruction wearing a portable name and worse than the inline asm
  it replaced. the biggest leak was not the assembly but `cpu/interrupts.h`:
  **nineteen files** included an x86 header to get four lines of interrupt
  masking, and got a struct listing rax through r15 and the whole 8259 with
  them. the version's real deliverable is `tools/checkarch.py`, since a
  boundary is worth exactly what checks it and this one would decay silently
 : it fails the build on inline asm outside `arch/`, on any portable file
  naming x86 without a stated reason, and on a list entry that has stopped
  being needed. it caught three wrong entries in the list I had just written
  by hand, plus two `__asm__ volatile` sites my own grep had missed.
  **"nothing changes" was checked rather than asserted**: the previous
  kernel is one `git archive` away, and 1057 of the 1073 functions in both
  builds have byte-identical instruction sequences: every one of the
  sixteen accounted for, and two of them not differences at all but two
  files each having a static function called `mine`. the thing that nearly
  got through: this kernel builds at `-O0`, where a plain `static inline` is
  a call: merely slower for `cpu_relax`, but *wrong* for
  `cpu_frame_pointer`, since a called function reads its own frame and the
  backtrace would have started one line low, politely reporting itself.
- **0.2.19**: an init worth the name. `kmain` started the four shells and
  the flusher itself because there was nothing else that could, which works
  once and answers nothing that comes after: what order things start in,
  what happens when one dies, who owns a process whose parent has gone, what
  shutting down means. the clause that pays for the rest is **restarting
  what dies**: `logout` can end a session now rather than calling `login`
  from inside it, so the next person does not inherit the last one's
  directory, history, jobs and variables. the rule with teeth is the one
  every init has had since sysvinit: something that dies instantly and
  restarts instantly is a machine that does nothing else ever again, so five
  deaths inside ten seconds and init leaves it down and says so on that
  console: and the **window** matters as much as the count, since five
  deaths across an afternoon is five people logging out and a machine that
  gave up on a console for having been *used* would be worse than no rule at
  all. shutdown is where owning things stops being theoretical: `reboot`
  used to sync and reset from whichever console typed it while three other
  sessions carried on writing, so what reached the drive was whatever was
  dirty at that instant; init stops respawning, asks the programs to stop
  before insisting, takes the services down in the reverse of the order they
  came up (sessions first, since they can write; flusher last, since it
  protects the disk) and syncs only then. pid 1 matters because reparenting
  needs a number known before the process it names exists. that replaced a
  sweep at the top of every spawn which collected *every* finished process:   a background job's exit code was swept away by the next command typed, so
  `jobs` reported whatever the status had been last time anyone looked; it
  survives only as a last resort for a table that is genuinely full. the
  orphan rule had a bug the test caught at once: "is my parent still in the
  table" says no for parent **0** as well, and 0 is the kernel shell, so
  every shell's child looked like an orphan. also fixed in passing: a bare
  `mount` walked into `argv[1]` on a one-word line, which the compiler had
  been pointing at the whole time by way of an unused `argc`.
- **0.2.18a**: the keyboard handler was reading bytes that belonged to the
  mouse. they share one controller, one data port and one output buffer, and
  bit 5 of the status byte says whose a waiting byte is: the mouse handler
  asked, the keyboard handler did not. a wheel mouse answers its device-id
  query with a **3**, `0x03` is the scancode for the 2 key, and so every
  boot came up with a `2` already typed in the username box. the wheel
  handshake also runs with reporting off now, since movement packets
  otherwise interleave with the acknowledgements and put the conversation
  out of step, and whatever is left is drained before interrupts come on.
- **0.2.18**: an environment, `$PATH`, and scripts. the environment is one
  block of NUL-separated `NAME=value` strings on the process rather than a
  table of pointers, because that shape makes inheriting it a single memcpy
  and inheriting is most of what an environment is *for*; it belongs to the
  process, which is why `export` has been a shell builtin everywhere since
  1977: a command could only ever change its own. the shell's lives in its
  session since it is a kernel thread with no process, so the block
  operations moved to `lib/env.c` and both callers share them. doing `$PATH`
  turned up something quietly wrong: **completion had been walking a fixed
  list** that happened to be the same one, and stopped being the same the
  moment PATH could be set: completion that offers a program which cannot
  be run is worse than none. scripts get `if`, `else`, `while` and `end`:   `end` rather than `fi`/`done` deliberately, since borrowing sh's spellings
  would claim a compatibility that does not exist. a condition is a command
  and true means it exited zero, which is sh's rule and the right one; the
  subtle part is that a condition inside a *skipped* block must not be
  evaluated at all, because `if grep x file` would still read the file:   checking only that the body is skipped misses it entirely, and there is a
  test that does not. `/boot/etc/profile` runs before every prompt on every
  console, which is the whole point: a PATH that must be typed every time is
  a PATH that means nothing.
- **0.2.17**: a ps/2 mouse, which is the first input here that is not a
  stream of characters. a queue is right for typing because typing *is* a
  sequence and the order is the meaning; a mouse reports a change since last
  time and the interesting thing is where the pointer ended up, so the
  driver keeps a position and the events are edges. what it turned out to be
  good for is what a pointer has been good for on a text console since gpm
  in 1993: drag over a path, middle-click at a prompt, and it is typed for
  you, with the characters going into the input queue as though keys had
  been pressed so that nothing above knows a mouse exists. the wheel scrolls
  back. the one genuinely hard part is that the 8042 has no framing: a
  packet is three bytes and nothing marks where one starts but a bit that is
  always set, so a single dropped byte puts every packet after it out of
  step and the pointer flies off in a straight line: which looks like a
  hardware fault and is not, which is why `mouse` prints the count of
  discarded bytes. a truncated packet cannot be fully recovered and costs
  exactly one wrong packet before it is back in step, and there is a test
  that says so.
- **0.2.16**: four virtual consoles, because the terminal layer was one
  machine pretending to be one seat. the console had kept a shadow of every
  cell since 0.1.x, added so a block cursor could put back the character
  underneath it: and a console nobody is looking at turns out to be exactly
  that shadow with nothing painting it, so most of the driver is the code it
  always was with one question in front of the parts that touch pixels.
  scrollback is the same array made taller. the whole thing turns on one
  rule stated twice: **output belongs to its writer** (a shell on console 2
  must not scribble over console 1, so every thread carries its console and
  the driver asks on each write) and **input belongs to the screen** (a
  process at the front of console 3 is at the front of console 3 and is
  still not being typed at, so a read needs both conditions; ctrl+c arrives
  from the keyboard and is aimed at the console being looked at, not at
  whichever the interrupted thread was on). the shell was the harder half:
  its state was file-static, correct while there was one of it and a bug the
  moment there were four, so it is a session per console reached through the
  calling thread. that move broke something quietly: `next_job_number = 1`
  was a static initialiser and a zeroed session does not get one, so every
  session's first job was job zero, which means "no job" everywhere else in
  the file.
- **0.2.15**: partitions, because a disk is not a filesystem: it is a table
  saying where several of them are, and the kernel had been asking each
  drive whether sector zero looked like a superblock. both tables: mbr's
  four sixteen-byte entries from 1983, and gpt's checksummed header and
  array, with the protective mbr a gpt disk carries so old tools see a full
  disk rather than an empty one. the crc is the whole difference between
  them: a corrupt mbr is simply followed because nothing in the format could
  notice, while a gpt that does not add up is *known* to be corrupt and is
  refused: so the parser is judged by what it will not do, and the tests
  spend more on the refusals than the happy path. the filesystem is handed a
  *view* of one partition rather than the drive, bounded at both ends, so it
  never finds out it is not alone; the cache stays underneath on absolute
  addresses so it remains correct when the mount moves. every partition is
  tried at boot rather than only the first, since an efi partition in slot
  one and the real filesystem in slot two is the ordinary arrangement.
  `parts` lists what each drive says it holds and what was actually found on
  it: a different question: and `mount <n>` moves the mount, bluntly and
  with a warning, because there is no reference counting here that could do
  better. `tools/readext2.py` had to learn about tables too: it read from
  sector zero, and the moment the disk was partitioned it would have quietly
  stopped being the only external check this project has.
- **0.2.14**: ext2, read and written: superblock, block groups, bitmaps,
  inodes, and the twelve-direct-then-indirect block map every unix
  filesystem of the era used. the whole difference from fat is one sentence
 : in fat a file *is* its directory entry, and here a name and a file are
  different objects: and everything else follows: permissions belong to the
  file rather than the name, a rename moves nothing, and `chmod` finally has
  somewhere to write an answer. there is no e2fsck on this machine, so the
  formatter and the driver would have been two programs by one author
  agreeing with each other; `tools/readext2.py` is written from the on-disk
  layout and run by the test target *after* the suite has finished writing,
  so what it validates is what the driver wrote. it found two real bugs
  before the driver even existed: every subdirectory was being built as
  though it were the root, and a fast symlink: one stored *in* its own
  block pointers: was having its target walked as block numbers, which is
  the sharpest edge in the format. `disk.c` mounts ext2 or fat32 and almost
  nothing above it changed, which is what having a vfs since 0.1.11 was for.
  and the new capability exposed an old bug: `vfs_may_read` only ever
  checked the *other* bits, which was invisible while nothing had an owner
  and made a 0600 file unreadable by its owner the moment anything did.
- **0.2.13**: a block cache, slotted exactly where fat32's two function
  pointers already were, which is the whole reason the filesystem needed no
  changes at all: it was handed a way to move sectors and it still is. the
  win is in *which* sectors: walking a cluster chain asks for the same
  table sectors over and over, and `disk` now prints the hit rate. writes
  are write-back, which is a promise broken on purpose: until a sync, the
  disk does not hold what the machine believes. so `reboot` and `poweroff`
  sync first (before this, a write reached the drive as it was made and a
  reboot lost nothing by definition), and a flusher thread syncs every three
  seconds: which does not replace `sync` but turns "you might lose
  anything" into "you might lose the last few seconds". a whole-block write
  skips reading the block first, since fetching bytes about to be thrown
  away doubles the cost of writing a big file; a partial write must not, and
  that one fails quietly: overwrite one sector of eight, evict, and the
  other seven are gone. no lock of its own, because everything reaches it
  through the disk's, and a second lock at the same rank inside the first is
  what the rank check exists to refuse. writing the tests found something
  worse than a bug: the shell suite's `reboot` stub called `exit(0)`, so
  every assertion after it had been vacuous: nothing had tested reboot
  before, so nothing had noticed the suite was quietly running half of
  itself.
- **0.2.12**: demand paging, which is one idea and a lot of consequences: a
  **record** of which addresses are legitimately empty. without it there is
  no telling a stack that wants to grow from a program dereferencing
  nonsense, and a kernel that guesses either kills good programs or conjures
  memory for bad ones. so every space keeps a table of ranges it agreed to
  and every not-present fault is answered out of it or not at all. the stack
  is a megabyte of range with two pages in it: the arguments have to go
  somewhere before the program starts: and the address below it is nothing,
  which is the guard page for free: it costs no memory because there is
  nothing there to cost anything. `mmap` hands out ranges the same way, so
  asking for four megabytes and using three pages costs three pages; there
  is a cap on a single ask for exactly that reason, since without one a
  program could reserve more than the machine has and find out halfway
  through using it. `elf_describe` returns the segments instead of copying
  them, so a program starts with none of itself in memory: but only for an
  image that will still be there when it runs, which is the ramdisk and
  therefore everything in `/boot/bin`; a disk copy would have to outlive
  every fork of the program, and that is a lifetime scheme this does not
  need to be worth having. writing the tests turned up a real bug the hard
  way: making the fake allocator hand back *poisoned* frames instead of
  conveniently blank ones showed that `addrspace_create` had never zeroed
  its region table, so a fresh space started with whatever the slab had in
  it last: and a garbage region is a record saying a wild pointer is
  legitimate.
- **0.2.11**: `fork`, with copy on write, and the page tables were the easy
  half. two things were not. **the parent's pages have to lose their write
  bit as well as the child's**: protect only the child and the parent
  quietly writes through the child's memory, which looks like it works;
  there is a test that fails on precisely that. and **a forked child has to
  return from a syscall it never made**, holding everything its parent held
 : including `rbx`, `rbp` and `r12`, `r15`, which the abi says are the
  callee's problem and which are therefore in the cpu at the moment of the
  call and buried under a C prologue a moment later. so the entry stub now
  writes the whole of ring 3 down on every call and hands the frame to the
  dispatcher; the child leaves through `fork_return`, which stands on a copy
  of that frame, zeroes `rax` and `sysret`s. `PTE_COW` is a software bit the
  cpu ignores, saying the read-only above it is a lie told on purpose:   without it a shared page and a genuinely read-only one are
  indistinguishable at fault time, and the difference between them is
  copying a page and killing a program. frames gained a one-byte share
  count; a frame down to its last holder is not copied at all, which is what
  makes fork-then-exit free. a child inherits descriptors, cwd, uid and its
  parent's **process group**, that last one because a child outside it is a
  process no ctrl+c can reach. `gemini` exists to make it visible, since
  copy on write is invisible when it works.
- **0.2.10**: job control, and the interesting part was getting stopping
  wrong first. a `THREAD_STOPPED` state next to `READY` and `BLOCKED` looks
  right and is not: a suspended thread may *also* be blocked on a pipe, and
  those answer different questions: what is it waiting for, against may it
  run at all. one enum for both means a stopped thread forgets it was
  stopped the moment anything wakes it, so `cat | wc` under ctrl+z would
  resume itself as soon as you typed. it is an orthogonal flag that
  `pick_next` skips, which also makes continuing a job one bit rather than a
  recovery, since it kept whatever it was halfway through. the terminal's
  front is a process group now rather than a pid: `cat x | grep y | wc -l`
  is one thing to whoever typed it, and interrupting only the last of three
  leaves the other two writing into a pipe nobody reads. with no signals, a
  suspended job announces itself by leaving a note the shell picks up in the
  poll it was already sitting in: waiting simply gained a second way to
  finish, and `fg` and `bg` differ by one argument. the job table lives in
  the shell because the kernel has pids and the shell has the line that was
  typed; jobs are recorded only when they survive the command (backgrounded
  or suspended), and anything that finished quietly is reported at the next
  prompt rather than over the top of whatever is being typed.
- **0.2.9**: an editor, and the descriptor work it dragged in behind it.
  `margaret` is nano-shaped and paints its whole screen on every keystroke
  rather than tracking which cells changed; the clever version is a second
  model of the screen, and a second model of anything is a second thing that
  can be wrong. it needed four syscalls nothing had wanted before: one key,
  the screen size, the cursor, and clear: all refused to anyone who is not
  the foreground, since a background program repainting over whoever is
  being typed at is the one thing none of it may allow. `getkey` is also how
  raw mode arrives: there is no flag saying a terminal is raw, because
  asking for a line and asking for a key are different questions. `write` is
  deleted in favour of `echo hi > file`, which meant finally doing what
  0.2.8 said it was deferring: **0, 1 and 2 became real descriptors**,
  tagged by what they point at, with redirection being nothing more than
  putting something else in a slot before the program starts. `>`, `>>` and
  `<` belong to a command rather than a line, are stripped from the
  arguments before the program sees them, and ask the same permission
  questions `open` and `create` do. `>` truncates by unlinking first, since
  `create` opens rather than empties. home, end, page up and page down had
  to be decoded on both the keyboard and the serial line: terminals send
  them in two different shapes and knowing one is not enough.
- **0.2.8**: pipes, and the buffer turned out to be the easy half. the two
  rules that matter both hang off the reference counts: a read returns 0
  once the last writer has gone (which is the only reason a pipeline ever
  finishes) and a write fails once the last reader has (which is the only
  reason `cat huge | head` stops rather than blocking forever on a buffer
  that can never drain). unix raises SIGPIPE there and the default
  disposition is to die; with no signals, the kernel does the dying part
  directly. releasing a process's ends had to be idempotent, because a
  program that *chooses* to leave runs `thread_exit` and one that is killed
  never does: three separate paths call `pipe_release_for` and whichever
  arrives first wins, since a stage that dies still holding an end leaves
  its neighbour asleep on a pipe that will never say end of file. 0, 1 and 2
  were never table entries here, so a pipeline is one pointer each on the
  process rather than a rebuilt descriptor table; that is why `>` still does
  nothing and says so. stderr deliberately stays on the console. builtins
  are refused with a reason: the shell is a kernel thread printing straight
  at the screen and has no stdout to give away. `head`, `wc`, `grep` and
  `sort`, plus a `cat` that reads standard input: all of them indifferent
  to whether a file or a bar is on the other end, which is the whole point.
  tab completion learned that the word after a bar is a command and not a
  filename.
- **0.2.7**: `rm`, `cp`, `mv` and `touch`, and the difference between two
  of them is the point: `mv` moves a name and not a file, so it costs the
  same on a hundred megabytes as on nothing, while `cp` really does carry
  every byte through a buffer. `fat32_rename` writes the new directory entry
  *before* striking out the old one on purpose: a machine that dies between
  the two leaves a file with two names, which is recoverable, where the
  other order leaves it with none. removing a file gives its clusters back
  and erases the whole run of long-name entries in front of it, since
  leaving those behind is how a directory ends up with a name pointing at
  nothing. fat has had date fields since 1980 and I had been writing zeroes
  into all of them; the filesystem now takes a clock as a function, so the
  kernel hands it the cmos and the tests hand it one stuck at a fixed
  instant: which is what makes "the right bytes were written" checkable
  rather than plausible. `stat`, and `ls -l`. no `rm -r`: that is a
  different operation and it can have its own name when something can be
  trusted to stop in the right place.
- **0.2.6**: `help cat` runs cat with `--help` and lets it answer. what a
  program takes is declared inside the program, so any copy the shell kept
  would be a second description free to drift; there is no copy. `cat
  --help` typed directly gives identical text by construction rather than by
  discipline, and both are built from the declaration 0.2.5 introduced.
  builtins come out of the shell's table, which is where *they* are
  declared, and `help cd` says why it is a builtin at all. the plain `help`
  stopped printing a description beside every name -- that was a wall you
  had to read all of to find one line -- and prints names in columns sized
  to the terminal instead, with `help <name>` for the one you actually
  wanted.
- **0.2.5**: one argument parser, and a program declares what it takes
  rather than reading `argv` by hand. that declaration is the only
  description of the program there is: the parser reads it, and so will
  whatever has to explain it, which is what keeps usage text from drifting
  away from the code. short and long forms, clustering, values as the next
  word or stuck on or after an `=`, and `--` to stop parsing -- the last
  being the only way to name a file that begins with a dash. `--help` is
  noticed and deliberately not acted on, because deciding what to print is
  0.2.6's job and a parser should not write things. `cat -v` and `cat -n`,
  `ls -1`, `echo -n`, `write -t`.
- **0.2.4**: a real search path. typing a program by name had worked since
  0.1.4, but by sticking `bin/` on the front and asking the ramdisk
  directly, around the vfs -- so a program on the disk could never be a
  command and `./thing` meant nothing. now `/bin` then `/boot/bin`, through
  the vfs, with a name earlier on the path hiding one later; the working
  directory is deliberately *not* on it, because a name typed alone should
  mean the same thing wherever you stand and a program left lying about
  should not become a verb. anything with a slash is a path, read from where
  you are. `help` is one list with a dot in the margin for the ring 3 ones,
  and completion offers both. one asymmetry had to go for any of it to work:
  the ramdisk is a flat archive, so `/boot/bin/hello` could be opened while
  `/boot/bin` could not be listed.
- **0.2.3**: a working directory per process, and every path resolved
  against it at the syscall boundary so no filesystem below ever sees a name
  that means two things. `.`, `..` and repeated slashes are flattened, a
  path that will not fit is refused rather than truncated, and `..` from the
  root stays at the root -- that last one having its own tests, since a path
  that can climb above `/` can name anything. `cd` and `pwd` are builtins
  because a `cd` that was a program would change where it was standing and
  then exit. `mkdir` and `rmdir` down to fat32: a directory is born with the
  two entries every directory has, and only an empty one can be unmade. two
  gaps surfaced on the way: the mount points could not describe themselves
  -- `/` is where mounts hang from and `/boot` *is* the ramdisk, so neither
  is on any filesystem -- and the ramdisk fallback had to widen to absolute
  paths, or a machine with no disk could suddenly reach nothing at all.
- **0.2.2**: every core runs threads now. one shared ring rather than a
  queue each, which is a deliberate departure from the roadmap: at four
  cores the lock is not the bottleneck and an idle core taking whatever is
  ready is already load balancing, without the migration machinery per-core
  queues then need. each core gets its own descriptor tables, its own timer
  and its own idle thread, and learns its own name from the task register --
  every core loads a different tss selector, so the register the cpu already
  holds is its identity, which costs two cycles and needs nothing in memory
  to have been reached first. tlb shootdown by inter-processor interrupt
  with a bounded wait for every core to answer. the syscall path turned out
  to be per-machine where it had to be per core -- the entry stub swapped
  stacks through two globals, and `star`/`lstar`/`sfmask` were set only on
  the boot core, so a program scheduled anywhere else executed `syscall` and
  jumped to address zero in ring 0; it uses per-core words reached through
  `gs` now -- and deliberately *without* `swapgs`, because the parity of
  those swaps is per thread while the bases are per core, so a thread
  preempted inside a syscall and resumed elsewhere leaves a core's bases
  reversed. three bugs that are not about scheduling had to be fixed first:
  all four cores were counting the same clock, so an hour would have passed
  in fifteen minutes; the reaper freed the stacks of threads that another
  core might still be standing on; and a core's idle thread was visible in
  the ring before that core had claimed it. `ps` gained a core column,
  `cpus` says what each is running.
- **0.2.1**: locks, at last. thirty-nine `irq_save` pairs across twelve
  files were correct mutual exclusion for one core and were quietly
  reclassified as wrong by 0.2.0; every one is now a real lock. the
  primitive is deliberately the same shape as what it replaces, so the audit
  reads as one change repeated thirty-nine times and anything that is not
  stands out. lock order is checked against a rank read off the call graph
  rather than assumed -- and it warns rather than panics, because with one
  core a wrong rank cannot deadlock and should not cost a working machine.
  `test_locks` is the first suite in this project that can see a race: eight
  threads through the real allocators, with a deliberately unguarded counter
  as a control, asserted to come out *wrong* so that the guarded one means
  something. `locks` shows the ranks and the contention. a lock declared
  where it is defined never called `spin_init`, so none of the twelve were
  on the list the shell shows -- they register themselves on first use now.
  and the recursion check paid for the whole version on its first boot,
  catching four bugs of one shape: a lock is a property of the machine and
  must never be held across a context switch, where `cli` was a property of
  the thread and rode through one harmlessly. a new thread began holding the
  scheduler's lock with its release on a stack it would never return to; the
  blocking reader slept holding the keyboard's lock, so the interrupt meant
  to wake it spun on that lock forever; the timer tick walked the run queue
  holding nothing; and waking a thread took the scheduler's lock twice. all
  four were correct on one core.
- **0.2.0**: the other processors wake up. a core that has never run holds
  itself in reset until another core's local apic tells it otherwise, and
  when it starts it starts in real mode at a page below a megabyte, so there
  is a trampoline sitting there to walk it back into long mode -- on page
  tables that are a copy of the kernel's with low memory identity-mapped,
  because the kernel maps none of where that code lives. each woken core
  reports its own apic id, which is the one thing it cannot fake, and then
  halts. `cpus` lists them. they are given nothing to do on purpose: there
  are 39 `irq_save` pairs in this kernel that call turning interrupts off
  mutual exclusion, and every one is false on a second core -- so 0.2.1 is
  an audit before 0.2.2 is a scheduler. also fixed: `all: $(BOOTIMG)` was
  written above the line defining `BOOTIMG`, so a bare `make` had been
  quietly building nothing at all.
- **0.1.12**: philemon, my own bootloader, and now the only one. the
  bootloader before it is gone, along with the iso, the uefi path and the
  protocol that came with it: writing a bootloader and then booting with
  somebody else's is not much of a bootloader. one file, whose first 512
  bytes are the only part the bios will read and which do nothing but pull
  in the rest of the same file; a20 and unreal mode so the kernel can be
  read in above a megabyte; page tables and long mode; and a 64-bit half in
  C that parses the elf and builds the memory map. the kernel is handed one
  struct in rdi and knows nothing about anybody's boot protocol including
  mine: the old boot protocol header is deleted and there is not one request
  structure left in it. the C half is host-tested, and writing those tests
  found two real bugs: a carve loop walking unsorted regions that handed the
  ramdisk's memory away as free, and boot-table offsets read as though the
  struct had 16-byte fields. a third was found by reading: the video mode
  code loaded `fs` in real mode, which quietly undid unreal mode and left
  the page tables being written somewhere else entirely.
- **0.1.11**: one namespace instead of two filesystems side by side. the
  disk is the root; the ramdisk moved to `/boot`. a bare name is looked for
  on the disk first and the ramdisk second, so a disk may supply its own
  copy of anything while a machine without one falls through to what it
  booted with. `mount` shows the table, and `cat welcome.txt` versus `cat
  /boot/welcome.txt` demonstrates the order in one line. the syscall layer
  lost its prefix tests and its two branches -- `open`, `read`, `write` and
  `readdir` all go through one resolver now, and programs and `passwd` come
  through it too, which is what lets either of them live on either
  filesystem. the ramdisk stays on purpose: it is a module handed over
  before any driver exists, so a kernel that needed a sata controller to
  find its own programs would be one a missing cable bricks. the vfs suite
  runs every check twice, once with the disk switched off, to keep that
  true.
- **0.1.10**: a disk, and a filesystem on it that remembers. an ahci driver
  reaches the sata controller pci enumeration found: commands are built in
  ram -- a header, a table holding the frame the drive receives, a scatter
  list of physical addresses -- and one bit says go, after which the
  controller moves every byte itself. polled rather than interrupt-driven,
  with every wait bounded. on top of that, fat32: cluster chains,
  subdirectories, and long filenames assembled from the records hidden in
  front of the short ones (checked for completeness, since a half-assembled
  name is worse than none). writes go to existing files, past their end, and
  to files that did not exist yet. `ls`, `cat welcome.txt`, and `write
  /notes.txt something` -- then reboot and it is still there. the filesystem
  takes its disk as two functions, so the suite runs the real parser against
  a real image; but the formatter and the parser share an author, so the
  check that counts is mounting the image on linux.
- **0.1.9**: the allocators, rebuilt. the pmm is a buddy allocator: free
  lists per block size, and blocks that put themselves back together when
  both halves come home, found by flipping one bit of a frame number.
  fixed-size structs (threads, address spaces) get slab caches, where each
  page carries a header naming its cache so a bare pointer can be traced
  back to where it came from -- and so a page whose objects have all
  returned goes back to the pmm rather than being held forever. `kmalloc`
  sits on top of those and no longer searches for anything: size classes
  below 1 KiB, whole pages above. `mem` gained the shape of free memory
  rather than just the amount of it, and `slabs` shows what each cache is
  holding. the cost, stated plainly, is that the buddy rounds up -- five
  pages costs eight -- which `mem` now counts honestly instead of hiding.
- **0.1.8**: the pci bus, enumerated at boot and shown by `lspci`: vendor
  and device ids, the class in words, the base address registers saying
  where each device listens, and its interrupt line. the scan takes config
  space as a function rather than reaching for the ports, so the test builds
  its own machine -- a bridge with a device behind it, a multifunction part,
  empty slots between -- and checks the walk finds exactly what is there.
  the two things worth getting right are that a multifunction part only
  admits to its other functions in one bit of function zero, and that a
  bridge hides an entire bus that has to be walked through; the walk is
  depth-limited, because firmware that disagrees about bridges forming a
  tree should not be able to make the kernel recurse forever.
- **0.1.7**: off the 8259 and onto the apics. acpi tables walked from the
  rsdp the loader hands over, the io apic routing external interrupts, and
  the lapic's own timer in place of the pit -- calibrated against the pit
  first, because nobody documents what speed it runs at. the routing asks
  acpi where an irq really arrives rather than assuming the numbers everyone
  knows, since irq 0 is wired to line 2 on most real machines and a kernel
  that assumes gets no timer at all. both apics live above ram, so each
  needs a page mapped with caching off. the calibration polls pit channel 2
  rather than counting its interrupts, because interrupts are off that early
  and the chip is about to be masked -- and the new timer is proved to
  deliver before the old one is given up. the io apic half is *not*
  automatic: a route that reads back correctly can still deliver nothing,
  and the symptom is a machine with no keyboard, so it lives behind an
  `ioapic` command you run from a shell that already works. and because the
  timer and the external interrupts now live on different chips, which one
  acknowledges an interrupt is two flags rather than one -- getting that
  wrong meant the 8259 never heard back, and stopped after a single
  keypress. lateral on its own and the readme says so; what it buys is a
  second cpu being possible. if the firmware describes no apics the 8259
  keeps the job and nothing above notices. the parser is tested on malformed
  tables, because firmware bytes are the least trustworthy in the machine
  and the ones acted on earliest.
- **0.1.6**: the kernel measures itself. the timer tick charges itself to
  whoever was running, so `ps` grows a cpu column and the scheduler stops
  being theoretical -- a sampling measure rather than real accounting, and
  the README says so. the pmm remembers its peak, every syscall is counted
  as it is dispatched, and `top` redraws the lot twice a second until you
  press a key. also quieter: a program typed by name no longer narrates its
  pid and its departure, because you wanted the output rather than a
  commentary on it. `run` still does, since that is a demonstration, and a
  kill is always reported.
- **0.1.5**: users. a login prompt reading accounts from `passwd` in the
  ramdisk, with the password not echoed; a uid on every process, inherited
  by children and unaskable-for by programs; and a check with real
  consequences -- `open` weighs the mode tar recorded against the caller's
  uid, so `velvet-room.txt` at 0600 is readable by `igor` and refused to
  `guest`. `whoami` exists twice on purpose: the builtin reads a variable
  the shell keeps, the program asks the kernel what uid it was given and
  cannot lie about the answer. the passwords are plaintext and the README
  says why that is the honest shape of this rather than a corner cut. the
  parser is tested mostly on malformed input, since a passwd file letting
  somebody in on a line it half understood is the worst thing it could do.
- **0.1.4**: a toolbox outside the kernel. `cat`, `echo`, `uptime` and `ls`
  are programs in `ramdisk/bin` now, and typing one looks no different
  because the shell falls back to looking for a program of that name. what
  made it possible was arguments: the loader builds argv on the program's
  own stack -- strings, then pointers to them -- and hands argc and argv
  over in registers, with every store going through the direct map while
  every pointer written is the address the program will see. the exercise
  was meant to reveal which commands were secretly using kernel internals,
  and it did: `echo` needed only argv, `cat` and `uptime` needed nothing
  new, and `ls` needed `readdir`, because `open` can only answer about a
  name you already know. what stayed behind reads kernel state or acts on
  the machine, and could not have left.
- **0.1.3**: the keyboard belongs to somebody. a foreground process owns
  the terminal while it runs, and the shell stops peeking at keys on its
  behalf -- which is what made a program reading the keyboard impossible
  until now. ctrl+c aimed at a program is delivered to it rather than acted
  on for it: it never becomes a character, the process finds it on its next
  syscall, and a read or a sleep comes back -1 so a sleeping program hears
  about it at once. pressing it twice stops asking. only the foreground
  process may read stdin, so a background one cannot take keys meant for
  somebody else. the tty also owns the line discipline -- echo, backspace,
  and ignoring arrows -- because a program never sees the keys go past and
  cannot echo them itself; without it you type into a void. `bin/ask` reads
  a line and greets you, which is a thing that could not have worked a
  version ago.
- **0.1.2**: the syscall table doubles: `open`/`close` and a `read` that
  takes a descriptor, so a program can read a file instead of only being
  loaded from one; `getpid`; and `spawn`/`wait`, which let a program start
  another and hear how it went. descriptors live on the process, so they
  close when it does, and a bookmark into a read-only archive costs nothing
  to allocate or free. a process may only wait for its own children.
  `read`/`write` gained an fd argument, a breaking change to the user abi
  and the right shape. also fixes a regression 0.1.0 shipped: user pointers
  were validated against the kernel's page tables, which since per-process
  address spaces map none of a program's memory -- so every syscall taking a
  pointer silently returned -1 and programs printed nothing at all. refusals
  are logged now, a test asserts which page tables get consulted, and the
  boot test fails if any pointer is ever refused. two new programs:
  `bin/reader` opens a file and reads it in bites, `bin/parent` spawns
  `bin/fail` and passes on its 42.
- **0.1.1**: processes. a program now has a pid, a parent and an exit code,
  kept in a table that outlives the thread that ran it -- which is the only
  way an exit code can survive, since the thread and its whole address space
  are gone the moment it dies. `run` reports how a program went; `ps` shows
  threads and processes as the different things they are. and the caveat
  that has been in this file since m6 is retired: threads carry a pointer
  back to the waitq they are parked on, so `kill` can take one off that
  queue before the reaper frees it, instead of refusing. `bin/fail` exists
  to exit 42 and prove the number gets home.
- **0.1.0**: **programs are isolated.** each gets its own pml4, sharing
  only the kernel half, and by reference so the kernel stays reachable
  whichever tables are loaded -- it must, since the stack I switch on lives
  there. two copies of the same program now run at once at identical
  addresses without meeting. teardown walks the lower half and hands back
  the image, the stack and the page tables together, which retires the leak
  0.0.17 shipped with. `run prog &` for background, `ps` showing which
  threads are ring 3 and how much memory each holds, and `bin/counter` as a
  second program that exists to be run twice. plus [ROADMAP.md](ROADMAP.md),
  which lays out the eleven steps of 0.1.x -- from exit codes and a wider
  syscall table up to a filesystem on a real disk and a bootloader of my
  own.
- **0.0.17**: **it runs programs.** ring 3 via `iretq` into a fabricated
  frame, `syscall`/`sysret` with STAR/LSTAR/SFMASK, a static elf64 loader,
  per-thread kernel stacks tracked in the tss and for `syscall`, and
  `user/hello.c` -- a real program with no libc that prints and sleeps and
  exits, all through six syscalls. every pointer ring 3 hands the kernel is
  checked against the page tables before it is touched, mapped *and* user,
  so a program cannot make the kernel fault by lying -- and the refusals are
  tested harder than the successes, since they are the actual boundary.
  found a genuine bug on the way: intermediate page table entries never set
  `PTE_USER`, and since the cpu ANDs that bit down the whole chain, every
  user mapping would have been unreachable while looking perfectly correct
  in a dump. boot is quiet now -- the driver chatter goes to serial and
  `dmesg`, and the screen gets the banner and `welcome.txt`. tab completes
  filenames after any command that takes one (`cat`, `run`), fills in the
  longest shared prefix, and does nothing on an empty word *in the command
  position* -- listing every command is what `help` is for, but after `cat `
  there is no such list to consult, so an empty word there is worth
  answering. `run` waits for its program like a foreground command should,
  with ctrl+c to stop it. also `cat` takes several files, unknown commands
  suggest the nearest match (and a bare filename points at the path it lives
  under), `ls` prints paths you can actually retype, and `ps` prints in id
  order.
- **0.0.16**: files. `ramdisk/` becomes a ustar tar at build time, the
  loader passes it as a module, and `ls`/`cat` read straight out of it with
  no copying. the parser is fed hand-built archives in the tests --
  block-sized files, empty files, gnu tar's leading `./`, a header with no
  magic, a size field that lies -- and then the real archive the build
  produces, which is the one that catches what tar actually emits. also
  ctrl+l to clear without losing the line, and two commands that had to be
  persona-inspired: `arcana` for the version, rendered as the rank of a
  social link, and `persona`, a fastfetch that shows the machine's face
  along with what cpu it wears.
- **0.0.15**: a real line editor. cursor movement and mid-line editing,
  ctrl+a/e/w/u/k, del, and tab completion. the enabling change was making
  the console's `\b` non-destructive like an actual terminal, which meant
  giving it a shadow buffer of the text on screen so the block cursor can
  sit on a character and put it back afterwards. new commands: `poweroff`
  (so you stop killing qemu), `date` off the cmos clock, `hexdump` that
  checks the page tables before reading, `kill`, `history` and `time`. the
  rtc's decoding is split from its io and tested -- bcd, the pm bit hiding
  in the top of the hour byte, and 12am being hour zero are each their own
  small trap.
- **0.0.14**: symbolized backtraces. `tools/gensyms.py` turns the kernel's
  own `nm` output into a table baked into a `.ksyms` section, and panics,
  exception dumps and double faults all print a symbolized call chain. the
  section sits after `.text` so folding it in can never move a function, and
  the build verifies that rather than trusting it. found two things on the
  way: my `backtrace()` was colliding with glibc's in the host tests and
  being silently shadowed (now `kbacktrace`), and the test binaries had no
  prerequisite on the kernel sources they `#include`, so they were happily
  running against stale builds.
- **0.0.13**: a tss at last, with an IST stack for the double fault vector.
  that turns stack overflow from a silent triple-fault reboot into a report
  naming the thread and its guard page, because the cpu can always find a
  good stack for that vector even when `rsp` is in the hole. and the pmm now
  reclaims the loader's memory (~1 MiB): the shell moved onto its own
  pmm-backed thread so the boot thread can exit and stop standing on the
  loader's stack, and the bitmap grew to cover the reclaimable regions,
  which sit above the last usable one and were previously off the end of the
  map entirely. new test suite for the tss descriptor encoding, which
  scatters a base address across two qwords and fails silently when you get
  it wrong.
- **0.0.12**: kprintf learned the `-` (left justify) flag, which it had
  been claiming to support by virtue of gcc's format checking without ever
  implementing. the vmm's boot log used `%-7s`, so the specifier printed
  literally, every following argument landed in the wrong slot, and the
  kernel read `__data_end` as a string and page faulted. added
  `tools/checkfmt.py` to `make test` so no format string can outrun the
  formatter again.
- **0.0.11**: my own page tables. four levels built at boot, direct map in
  2MiB pages, kernel mapped per-section with W^X, NX enabled properly via
  EFER (and treated as a runtime capability, since a hardcoded NX bit faults
  on a cpu that lacks it), CR0.WP set so read-only means read-only even in
  ring 0. `vmm_init` verifies the whole thing by walking its own tables in
  software -- including the current stack -- before daring to load cr3.
  guard pages under every thread stack, which needed 2MiB page splitting to
  punch a hole in the direct map. exception dumps now name the thread that
  died and say when the address is a guard page. new shell commands: `vmm`
  to look up any address, `smash` to run off the end of the stack on
  purpose. 40-odd host assertions for the page table code, because a mistake
  there is a triple fault with nothing to read.
- **0.0.10**: milestone 7. serial input on irq4, with a translation layer
  for the terminal dialect (cr means enter, del means backspace, `ESC[A`
  means up) so the shell is drivable over the wire. keyboard and serial now
  feed one shared input queue in `drivers/input.c` instead of the keyboard
  owning the buffer privately. six host test suites moved into `tests/`
  behind `make test`, plus `tools/boottest.sh` which boots the iso and types
  at it. github actions runs the lot on every push. panics can now be
  escaped over serial too, not just from the keyboard.
- **0.0.9**: the shell grew the things you immediately miss when you sit
  down at it. the keyboard driver now decodes ctrl as a modifier
  (ctrl+letter arrives as a control code, so ctrl+c is 3) and stops throwing
  away the e0-prefixed arrow keys, which meant widening the ring buffer to
  16 bits so arrows cant be mistaken for characters. on top of that: 16
  lines of command history on up/down, ctrl+c to abandon a line and recall
  running personas, and a panic you can escape -- it polls the 8042 by hand
  and resets on any keypress instead of halting forever and making you kill
  qemu. keyboard and shell tests grew to 20 and 32 cases.
- **0.0.8**: milestone 6. an interactive shell with line editing and nine
  commands, running as a real thread (the boot thread renames itself `shell`
  and takes the job). a waitq in the scheduler plus a blocking
  `keyboard_getchar_blocking()`, so the prompt costs nothing while it waits
  instead of spinning on hlt. `summon` spawns persona threads on demand,
  which replaces the m5 demo threads that used to print forever and made the
  console unusable. the `FAULT_DEMO` build flag is gone -- the `crash`
  command does the same job better, and from thread context rather than
  early boot. shell parsing and dispatch are host-tested (20 cases, incl.
  argv clamping and empty lines).
- **0.0.7**: milestone 5. the pit ticks at 100hz on irq0 with a global tick
  counter and uptime. threads: kernel stacks from the pmm, a fabricated
  initial stack so a brand new thread can be "resumed" into existence, and a
  16-instruction context switch in asm. preemptive round-robin scheduling on
  a 50ms quantum, blocking `sleep_ms()`, `thread_exit()` with a reaper that
  frees dead threads' stacks (from a different thread's stack, which is the
  only safe way). an idle thread that hlts so theres always somebody to hand
  the cpu to. the allocators and kprintf take interrupts down while they
  work, since a half-updated free list is nobodys friend. demo: pixie and
  jack-frost count at different rates, the herald says its piece and dies to
  give the reaper something to do, and typing still works throughout. the
  context switch is host-tested, including whether all six callee-saved
  registers actually survive a round trip.
- **0.0.6**: milestone 4. physical memory manager: parses the loader's
  memory map (and prints it at boot), bitmap over every 4k frame, contiguous
  multi-page allocation with a rotating search hint, stats. kernel heap on
  top: first-fit free list, 16-byte aligned payloads, magic-guarded headers
  that catch double frees and wild pointers, address-ordered coalescing,
  grows by whole pages from the pmm. boot runs a self-test over both (8
  frames + 5 heap blocks, pattern verified, freed out of order, books must
  balance) and panics if anything is off. tested on the host too, 25
  assertions incl. draining ram dry and checking no frame is ever handed out
  twice.
- **0.0.5**: milestone 3. 8259 pic remapped to vectors 32-47 with spurious
  irq filtering, an irq_register() layer so drivers can claim lines, and a
  ps/2 keyboard driver: scancode set 1 -> ascii, shift + capslock state
  (they cancel, as the gods intended), e0 prefixes swallowed, all landing in
  a ring buffer. the scancode state machine is split from the irq handler
  and tested on the host (11 scenarios). boot now ends at a prompt that
  echoes thy keystrokes, live.
- **0.0.4**: milestone 2. my own gdt (tss slot reserved), idt with 256
  macro-generated isr stubs, and an exception handler that prints the vector
  name, decoded page fault info (cr2 + error bits) and a full register dump
  before panicking. at the time this shipped with a `FAULT_DEMO=1` build
  flag to trigger it; as of 0.0.8 thats the shell's `crash` command instead.
  the kernel also now boots, panics and (eventually) reboots with the
  appropriate persona social link ceremony. thou art I, and I am thou.
- **0.0.3**: milestone 1 done. kprintf (with actual tested number
  formatting), framebuffer console with the spleen 8x16 font, glyph
  blitting, scrolling, block cursor, and panic(). boot banner shows up on
  screen and serial at the same time. the temporary decimal-printer hack
  from 0.0.2 is gone, unmourned.
- **0.0.2**: com1 uart driver (polled, 115200 8n1, with loopback self
  test). framebuffer request to the loader, boot info logged over serial,
  test pattern on screen. run `make run` and watch the serial chatter in
  your terminal.
- **0.0.1**: project scaffold. the loader v9.x boots a stub kernel that
  halts politely. build system, linker script, license, this readme.

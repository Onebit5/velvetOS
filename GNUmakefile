# velvetOS build. needs gcc, nasm, make, python3 and git: the kernel is
# a velvet release, fetched into obj/velvet/ by the first build that
# wants it. `make run` and `make boottest` additionally need qemu.
#
# targets:
#   make              -> the bootable disk image, velvetos.img
#   make kernel       -> the kernel, its bootloader and its build tools,
#                        built out of obj/velvet/ into bin/kernel/
#   make userland     -> the programs and the library they link
#   make run          -> boot it in qemu (CPUS=4, MEM=2G, QEMU_EXTRA=...)
#   make test         -> the host suites and every checker
#   make kernel-test  -> the kernel's own suites, run in its checkout
#   make kernel-update -> move kernel-release.mk to the newest velvet release
#   make boottest     -> boot it headless and drive the shell over serial
#   make selfboot     -> and boot the kernel our own tools built
#   make clean

# the bootable image, and the default target. defined up here with the
# other names because a prerequisite is expanded where it is written:
# further down, `all: $(BOOTIMG)` quietly meant `all:` with nothing to do
BOOTIMG := velvetos.img

# and the disk `make run` attaches, for the same reason: `run` names it as
# a prerequisite long before the rule that builds it, and defined down
# there it was empty up here, so a fresh checkout's `make run` handed
# qemu a drive nobody had made
DISK := disk.img

# knobs for `make run`. they have to be make variables rather than extra
# words on the command line, because `make run -smp 4` hands -s -m -p to
# *make*, and -p means "print the entire database", which is a
# surprising amount of ukrainian
CPUS ?= 4
MEM  ?= 2G
QEMU_EXTRA ?=

NASM := nasm
CC   := gcc
LD   := ld

#
# the kernel is a project of its own, velvet, and this tree does not
# carry it. kernel-release.mk names one release of it by its version,
# and tools/kernel.py clones that release's tag into obj/velvet/ before
# anything below reads a file of it, and again only when the version
# changes. it is done here, while the makefile is read, because half of
# what follows finds the kernel's files with $(shell find) at that
# moment, and a kernel fetched later would be a kernel those lists had
# already missed.
#
# obj/, because the clone is something a build makes, like everything
# else in there: git here never sees it, and `make clean` throws it
# away with the rest.
#
#   make KERNEL_URL=../velvet   the same release, cloned out of a local copy
#   make KERNEL_SRC=../velvet   that checkout as it is, unchecked, for
#                               working on the kernel and the system at once
#
# nothing that does not read the kernel asks for it, so `make clean` and
# `make lint` work on a tree that has never fetched one
include kernel-release.mk

KERNEL_RELEASE := obj/velvet
KERNEL_SRC ?= $(KERNEL_RELEASE)

ifneq ($(if $(MAKECMDGOALS),$(filter-out clean distclean lint kernel-update,$(MAKECMDGOALS)),all),)
ifeq ($(KERNEL_SRC),$(KERNEL_RELEASE))
KERNEL_FOUND := $(shell python3 tools/kernel.py fetch '$(KERNEL_SRC)' \
                  '$(KERNEL_URL)' '$(KERNEL_VERSION)' >&2; echo $$?)
else
KERNEL_FOUND := $(shell python3 tools/kernel.py local '$(KERNEL_SRC)' \
                  '$(KERNEL_VERSION)' >&2; echo $$?)
endif
ifneq ($(KERNEL_FOUND),0)
$(error $(KERNEL_SRC) holds no kernel this tree will build, for the reason above)
endif

# what was built here was built from one kernel, and an object keeps no
# record of which: a dependency file names the tree it was made in, and
# a header or a flag that differs between two trees changes nothing that
# make can see. so going between the release and a checkout, or moving
# the pin, would link whatever was built last wherever it is newer than
# the other tree's source. a different kernel is a clean build, the
# fetched release excepted
KERNEL_FROM := $(abspath $(KERNEL_SRC)) $(KERNEL_VERSION)
ifneq ($(KERNEL_FROM),$(shell cat obj/kernel-from 2>/dev/null))
$(shell rm -rf bin base/ramdisk/bin base/ramdisk/init \
            $(filter-out $(KERNEL_RELEASE),$(wildcard obj/*)) && \
        mkdir -p obj && echo '$(KERNEL_FROM)' > obj/kernel-from)
endif
endif

#
# with the kernel in $(KERNEL_SRC), this tree asks it for what a system
# needs (the kernel, philemon, and the C tools that write disks and
# images) and says where to put them, so a build here leaves nothing in
# the kernel's checkout and a build there never sees anything of this
# one's.
#
# SOURCE is the one thing it is told that it could not work out for
# itself. the kernel carries the hash of the source shipped beside it,
# and what ships beside it here is velvetOS with the kernel in it,
# rather than the kernel alone
KERNEL := velvet
KBIN   := bin/kernel
KOBJ   := obj/kernel
KT     := $(KBIN)/tests

SOURCE := bin/source.tar

KMAKE := $(MAKE) --no-print-directory -C $(KERNEL_SRC) \
         BINDIR=$(CURDIR)/$(KBIN) OBJROOT=$(CURDIR)/$(KOBJ) \
         SOURCE=$(CURDIR)/$(SOURCE)

KERNEL_OUT := $(KBIN)/$(KERNEL) $(KBIN)/boot/philemon.bin \
              $(KBIN)/boot/philemon64.bin \
              $(KT)/mkext4 $(KT)/mkboot $(KT)/gensyms $(KT)/bin2c

.PHONY: all kernel userland kernel-test run bootimg clean distclean FORCE

all: $(BOOTIMG)

# always asked, because whether the kernel is up to date is the kernel's
# makefile's question and not this one's. it answers in a moment when
# nothing has changed, and leaves the files alone, so nothing here that
# depends on them is rebuilt for the asking
kernel: $(KERNEL_OUT)

$(KERNEL_OUT) &: $(SOURCE) FORCE
	@$(KMAKE) kernel philemon tools

FORCE:

# the kernel's own suites, in the kernel's own checkout, the way its CI
# runs them. not part of `make test`: a released kernel has passed these
# already, in velvet, before it was released
kernel-test:
	@$(MAKE) --no-print-directory -C $(KERNEL_SRC) test

# the newest stable release of velvet, written into kernel-release.mk and
# fetched. it changes one file and commits nothing: the commit that moves
# the system to a new kernel is somebody's decision, and it is the one
# line in the log that says every build after it ships that kernel
.PHONY: kernel-update
kernel-update:
	@python3 tools/kernel.py update '$(KERNEL_RELEASE)' '$(KERNEL_URL)'

#
# a ring 3 program is built much like the kernel (freestanding, no
# libc) but without -mcmodel=kernel, since it lives at 0x400000 in
# the low half rather than up in the higher one. it gets copied into
# the ramdisk, which is how the kernel finds it.

UCFLAGS := -Wall -Wextra -std=gnu11 -O1 \
	-ffreestanding -fno-stack-protector -fno-stack-check -fno-pic \
	-fno-omit-frame-pointer \
	-m64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -Iuserland \
	-Iuserland/libc -I$(KERNEL_SRC)/include

# what a program and the kernel agree on: the syscall numbers, the structs
# the calls hand back, signals, keys and terminal flags. they are the
# kernel's, in its include/uapi/velvet/, and userland/syscall.h
# includes them rather than keeping a copy, so everything that includes
# that header is rebuilt when the pin moves to a kernel that changed them
UAPI := $(wildcard $(KERNEL_SRC)/include/uapi/velvet/*.h)

AR ?= ar
ULDFLAGS := -nostdlib -static -T toolchain/linker.ld

USER_PROGS := base/ramdisk/bin/hello base/ramdisk/bin/counter base/ramdisk/bin/fail \
              base/ramdisk/bin/reader base/ramdisk/bin/parent base/ramdisk/bin/ask \
              base/ramdisk/bin/echo base/ramdisk/bin/cat base/ramdisk/bin/uptime base/ramdisk/bin/ls \
              base/ramdisk/bin/whoami \
              base/ramdisk/bin/mkdir base/ramdisk/bin/rmdir \
              base/ramdisk/bin/rm base/ramdisk/bin/cp base/ramdisk/bin/mv base/ramdisk/bin/touch \
              base/ramdisk/bin/head base/ramdisk/bin/wc base/ramdisk/bin/grep base/ramdisk/bin/sort \
              base/ramdisk/bin/margaret base/ramdisk/bin/gemini base/ramdisk/bin/chmod \
              base/ramdisk/bin/chown base/ramdisk/bin/ln \
              base/ramdisk/bin/tartarus base/ramdisk/bin/hermes \
              base/ramdisk/bin/theodore base/ramdisk/bin/patience \
              base/ramdisk/bin/diff base/ramdisk/bin/find base/ramdisk/bin/less \
              base/ramdisk/bin/wordcount base/ramdisk/bin/sha1sum \
              base/ramdisk/bin/git \
              base/ramdisk/bin/mem base/ramdisk/bin/slabs \
              base/ramdisk/bin/ps base/ramdisk/bin/vmm base/ramdisk/bin/bt \
              base/ramdisk/bin/lspci base/ramdisk/bin/disk \
              base/ramdisk/bin/parts base/ramdisk/bin/cpus \
              base/ramdisk/bin/mount \
              base/ramdisk/bin/dmesg base/ramdisk/bin/date \
              base/ramdisk/bin/sync base/ramdisk/bin/fsck \
              base/ramdisk/bin/resize \
              base/ramdisk/bin/src \
              base/ramdisk/bin/arcana base/ramdisk/bin/persona \
              base/ramdisk/bin/kill base/ramdisk/bin/poweroff \
              base/ramdisk/bin/reboot \
              base/ramdisk/bin/sh \
              base/ramdisk/bin/login \
              base/ramdisk/init

userland: $(USER_PROGS)

# every program links the argument parser, so that what a program takes
# is declared once and read by both the parser and whatever has to
# explain the program to somebody
# the library every program links against, added in 0.3.8.
#
# `start.c` is the one that matters: it defines `_start`, calls `main`
# and exits with what it returned. before it, every program here was
# entered at `_start` and had to call `exit` itself, which no program
# written anywhere else does, so every one of them had to be edited
# before it would build. that edit is what 0.3.8 removes.
#
# the library is an *archive* rather than a list of objects, and that is
# load-bearing rather than tidiness. an object named on a link line is
# always included; a member of an archive is pulled in only if it
# resolves something still undefined. so a program that writes its own
# `_start` never drags start.o in and never sees a duplicate, while one
# that writes `main` gets it, which is exactly the rule that lets old
# programs and ported ones share a build.
LIBC_SRCS := userland/libc/stdlib.c userland/libc/format.c \
             userland/libc/stdio.c userland/libc/start.c \
             userland/libc/string.c userland/libc/hash.c \
             userland/libc/inflate.c userland/libc/deflate.c \
             userland/libc/epoch.c
LIBC_OBJS := $(patsubst userland/libc/%.c,obj/libc_%.o,$(LIBC_SRCS))
LIBC_HDRS := userland/libc/string.h userland/libc/stdlib.h userland/libc/stdio.h \
             userland/libc/format.h userland/libc/hash.h \
             userland/libc/deflate.h userland/libc/epoch.h

# the string functions, the hashes, deflate and the date arithmetic are
# the library's own. they began as copies of the kernel's, taken at
# velvet 0.4.2, and stay here rather than being compiled out of its
# lib/: that directory is the kernel's insides, which it changes as it
# likes, and deflate is in it only for git, which is ours. what a
# program shares with the kernel is include/uapi/velvet/ and nothing
# else.
#
# the hashes go in the archive rather than into every program, which is
# what an archive is for: a member is pulled in only if something
# undefined needs it, so `sha1sum` gets them and `echo` does not
obj/libc_%.o: userland/libc/%.c $(LIBC_HDRS) userland/syscall.h $(UAPI)
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -c $< -o $@

# `D` for the same reason the ramdisk tar carries --mtime=@0: an archive
# built twice from the same objects should be the same file, and the one
# thing that would stop it is a timestamp. whether that is ar's default
# is decided when binutils is compiled, so it is asked for rather than
# assumed
obj/libc.a: $(LIBC_OBJS)
	$(AR) rcsD $@ $^

# the three helpers most programs link, each compiled once by a rule of
# its own. every program's rule used to compile all three itself, into
# the same three object files, which is harmless one at a time and a race
# under `make -j`: one link reads args.o while another program's rule is
# half way through writing it, and the link fails on symbols that were
# there a moment ago
obj/user_args.o: userland/args.c userland/args.h
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -c $< -o $@

obj/user_textbuf.o: userland/textbuf.c userland/textbuf.h
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -c $< -o $@

obj/user_difflib.o: userland/difflib.c userland/difflib.h
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -c $< -o $@

# git is the one program that links more than the common set: the object
# store and the seam under it. an explicit rule rather than adding them
# to every program, since nothing else wants them
base/ramdisk/bin/git: userland/git.c userland/gitobj.c userland/gitio_velvet.c userland/gitobj.h \
                 userland/gitio.h userland/syscall.h $(UAPI) \
                 obj/user_args.o obj/user_difflib.o toolchain/linker.ld \
                 obj/libc.a
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -c userland/git.c -o obj/user_git.o
	$(CC) $(UCFLAGS) -c userland/gitobj.c -o obj/user_gitobj.o
	$(CC) $(UCFLAGS) -c userland/gitio_velvet.c -o obj/user_gitio.o
	$(LD) $(ULDFLAGS) obj/user_git.o obj/user_gitobj.o obj/user_gitio.o \
	      obj/user_args.o obj/user_difflib.o obj/libc.a -o $@

# the machine's init, at the root of the ramdisk, which is where the kernel
# looks for the program to make pid 1
base/ramdisk/init: userland/init.c userland/syscall.h $(UAPI) \
                   toolchain/linker.ld obj/libc.a
	$(CC) $(UCFLAGS) -c userland/init.c -o obj/user_init.o
	$(LD) $(ULDFLAGS) obj/user_init.o obj/libc.a -o $@

# the shell, whose line parser is a file of its own so the host suite can
# take it without the half that forks
base/ramdisk/bin/sh: userland/sh.c userland/shparse.c userland/shparse.h \
                userland/syscall.h $(UAPI) toolchain/linker.ld obj/libc.a
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -c userland/sh.c -o obj/user_sh.o
	$(CC) $(UCFLAGS) -c userland/shparse.c -o obj/user_shparse.o
	$(LD) $(ULDFLAGS) obj/user_sh.o obj/user_shparse.o obj/libc.a -o $@

base/ramdisk/bin/%: userland/%.c userland/syscall.h $(UAPI) userland/args.h userland/lines.h \
               userland/report.h \
               userland/textbuf.h userland/difflib.h \
               obj/user_args.o obj/user_textbuf.o obj/user_difflib.o \
               toolchain/linker.ld obj/libc.a
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -c $< -o obj/user_$*.o
	$(LD) $(ULDFLAGS) obj/user_$*.o obj/user_args.o obj/user_textbuf.o \
	      obj/user_difflib.o obj/libc.a -o $@

# the ramdisk is a plain tar. --format=ustar because thats the one the
# kernel knows how to read, and the flags after it keep the archive
# byte-identical between builds so the image doesnt churn
RAMDISK := bin/ramdisk.tar
RAMDISK_FILES := $(shell find base/ramdisk -type f 2>/dev/null)

$(RAMDISK): $(USER_PROGS) $(RAMDISK_FILES)
	@mkdir -p $(@D)
	@# git only tracks the execute bit, so the modes that matter are set
	@# here rather than trusted to the checkout. velvet-room.txt is the
	@# one the kernel refuses to a guest
	@chmod 600 base/ramdisk/velvet-room.txt
	@chmod 644 base/ramdisk/passwd base/ramdisk/*.txt 2>/dev/null || true
	@chmod 600 base/ramdisk/velvet-room.txt
	tar --format=ustar --sort=name --owner=0 --group=0 --numeric-owner \
		--mtime=@0 -cf $@ -C base/ramdisk .

#
# 0.3.26. everything above builds a machine; this puts the thing it was
# built *from* on the same medium, so that a disk which boots is also a
# disk you could rebuild from. it is the last piece 0.4.0 needs that is
# not a compiler.
#
# it is another ustar, for the plainest of reasons: the kernel already
# reads one, and a second container format would be a second parser to
# get wrong. it is *not* compressed, which is a decision rather than an
# oversight: the one moment anybody wants this archive is the moment
# something has gone badly wrong, and a recovery path that needs a
# decompressor between you and the source is a recovery path with one
# more thing in it to be broken. three and a half megabytes of a disk is
# not worth that.
#
# what goes in: everything the project is made of, which is a longer
# list than "everything the compiler reads". the tests are the reason
# any of this is trustworthy and the prose is where the reasoning
# lives, so both ship. what stays out is anything the build *produces*
# (bin/, obj/, base/ramdisk/bin/), because a source tree carrying its own
# output is a source tree that disagrees with itself the moment you
# build it.
#
# and the kernel's files go in with this tree's, because a disk that can
# rebuild the system has to be able to rebuild the kernel too. they go in
# under kernel/, wherever this build found them, so the archive says the
# same thing about a kernel whether it was fetched or worked on beside
# this tree. they are found rather than asked of git, so a tarball of
# the kernel builds the same archive as a checkout of it; what is left
# out is what the kernel's own build would leave out, plus the git
# history the clone keeps and the caches python leaves in it.
#
# KERNEL_FILES are named from inside the checkout, which is what lets
# them be put under kernel/ at all
SOURCE_FILES := GNUmakefile README.md CHANGELOG.md ROADMAP.md \
                LICENSE .gitignore kernel-release.mk \
                CONTRIBUTING.md CODE_OF_CONDUCT.md \
                $(shell find .github userland toolchain tools tests docs -type f | sort) \
                $(shell find base/ramdisk base/diskroot -type f \
                        -not -path 'base/ramdisk/bin/*' | sort)
KERNEL_FILES := $(shell cd $(KERNEL_SRC) 2>/dev/null && \
                  find . -path ./.git -prune -o -path ./bin -prune \
                         -o -path ./obj -prune -o -name __pycache__ -prune \
                         -o -name '*.img' -prune -o -type f -print \
                  | sed 's,^\./,,' | sort)

# --sort=name and a fixed mtime for the same reason the ramdisk has
# them: two builds of the same tree must produce the same bytes, or the
# stamp below is a fact about when I built it rather than about what I
# built.
#
# --mode is there for a subtler version of the same thing. git records
# one bit of a mode, the ramdisk rule above sets others by hand, and the
# result is that base/ramdisk/velvet-room.txt is 0600 after a build and 0644
# in a fresh checkout, so the archive, and therefore the stamp, would
# depend on whether anybody had built anything yet. it states what it
# wants instead: everyone may read, only the owner may write, and
# whatever was executable stays executable
#
# the kernel is a second tar appended to the first rather than more names
# in it, because -C and --transform are what put its files under kernel/
# and they would rename this tree's files too
$(SOURCE): $(SOURCE_FILES) $(addprefix $(KERNEL_SRC)/,$(KERNEL_FILES))
	@mkdir -p $(@D)
	@echo '  TAR     $@ ($(words $(SOURCE_FILES) $(KERNEL_FILES)) files)'
	@tar --format=ustar --sort=name --owner=0 --group=0 --numeric-owner \
		--mtime=@0 --mode='u+rw,go+r,go-w' -cf $@ $(SOURCE_FILES)
	@tar --format=ustar --sort=name --owner=0 --group=0 --numeric-owner \
		--mtime=@0 --mode='u+rw,go+r,go-w' -rf $@ -C $(KERNEL_SRC) \
		--transform 's,^,kernel/,' $(KERNEL_FILES)

# the image is philemon's format, so it is written by philemon's tool,
# which lives with philemon in the kernel. what goes in it is this system's:
# its ramdisk and its source
$(BOOTIMG): $(KBIN)/boot/philemon.bin $(KBIN)/boot/philemon64.bin \
            $(KBIN)/$(KERNEL) $(RAMDISK) $(SOURCE) $(KERNEL_SRC)/tools/mkboot.py \
            $(KERNEL_SRC)/boot/philemon.inc $(KERNEL_SRC)/include/velvet/philemon.h
	@python3 $(KERNEL_SRC)/tools/mkboot.py $@ $(KBIN)/boot/philemon.bin \
		$(KBIN)/boot/philemon64.bin $(KBIN)/$(KERNEL) $(RAMDISK) $(SOURCE)

bootimg: $(BOOTIMG)

# two drives: the one philemon is on, and the one with the files. the
# boot image goes first and is named as the boot device twice over:
# `order=c` so the bios does not go looking for a floppy it has not got,
# and a bootindex on each so the order is not left to chance.
#
# the kernel does not care which is which: it tries every drive until one
# has a filesystem it recognises, and the boot image has none
#   make run            four cpus, 2G
#   make run CPUS=1     one, which must always still work
#   make run MEM=512M QEMU_EXTRA="-d int"
#
# ports 5555 (tcp) and 5556 (udp) are forwarded in from the host, so a
# program running *inside* can be reached from outside. without them the
# guest can only ever make outbound connections, which tests half of tcp
# and none of a server.
#
# and the forward is the *only* way in. qemu's user-mode networking is a
# userspace nat, so 10.0.2.15 is not an address the host can route to:
# `nc -u 10.0.2.15 5556` from outside goes nowhere at all, silently,
# which looks exactly like a guest that is ignoring its own port. it is
# `nc -u localhost 5556` that arrives
run: $(BOOTIMG) $(DISK)
	qemu-system-x86_64 -M q35 -m $(MEM) -smp $(CPUS) -serial stdio \
		-boot order=c \
		-drive id=boot,file=$(BOOTIMG),format=raw,if=none \
		-device ide-hd,drive=boot,bus=ide.0,bootindex=0 \
		-drive id=data,file=$(DISK),format=raw,if=none \
		-device ide-hd,drive=data,bus=ide.1,bootindex=1 \
		-netdev user,id=n0,hostfwd=tcp::5555-:5555,hostfwd=udp::5556-:5556 \
		-device e1000,netdev=n0 \
		$(QEMU_EXTRA)

# the disk, which is a real filesystem rather than an archive: built out
# of whatever is in base/diskroot/, and attached to qemu as a second
# drive. it is deliberately NOT rebuilt by `make run` once it exists:
# the whole point is that what you write to it stays written, and
# regenerating it every boot would quietly undo that. `make disk` starts
# over when you want a clean one
#
# partitioned since 0.2.15, because a disk is not a filesystem: it is
# a table saying where several of them are. three of them since 0.3.25:
# the system, somewhere to *work* that is not the system, and the fat32
# one that has been here since 0.2.15, because a machine that mounts
# its work beside its system is the point of that version, and a third
# filesystem of another kind is still what `mount <n>` has to have
# something to do.
#
# the system partition is built by the C formatter rather than the
# python one, so the disk the machine actually boots is a real ext4,
# extents and all. and the work image is made smaller than the partition
# it goes in, and that is deliberate: 8 MiB of filesystem inside 24 MiB
# of partition is exactly the situation `resize` exists for, so `make
# run` boots a machine with something to grow. both are labelled here,
# by name: the formatter's default label is the kernel's to choose
$(DISK): $(KT)/mkext4 $(KERNEL_SRC)/tools/mkfat.py $(KERNEL_SRC)/tools/mkdisk.py \
         $(shell find base/diskroot -type f 2>/dev/null)
	@mkdir -p bin/workroot
	@echo 'somewhere to work that is not the system' > bin/workroot/readme.txt
	@$(KT)/mkext4 bin/diskroot.img base/diskroot 32 velvetos >/dev/null
	@$(KT)/mkext4 bin/work.img bin/workroot 8 work >/dev/null
	@dd if=/dev/zero of=bin/work.img bs=1M count=0 seek=24 2>/dev/null
	@python3 $(KERNEL_SRC)/tools/mkfat.py  bin/compendium.img base/diskroot 24 >/dev/null
	@python3 $(KERNEL_SRC)/tools/mkdisk.py $@ gpt bin/diskroot.img:linux bin/work.img:work \
		bin/compendium.img:fat32

.PHONY: disk
disk:
	@rm -f $(DISK)
	@$(MAKE) --no-print-directory $(DISK)

#
# the host suites for what this tree wrote: the library, the helpers the
# programs share, git's object store, and the assembler. the kernel's
# suites are the kernel's, in its tests/, and `make kernel-test` runs
# them.

HOSTCC    := gcc
HOSTFLAGS := -std=gnu11 -Wall -Wextra -Wno-stringop-truncation -Wno-stringop-overread -g -DVELVETOS_HOSTED -I$(KERNEL_SRC)/include

TEST_BINS := bin/tests/args bin/tests/libc bin/tests/textbuf \
             bin/tests/difflib bin/tests/git bin/tests/shparse \
             bin/tests/hash bin/tests/inflate bin/tests/epoch

bin/tests/args:     tests/test_args.c     userland/args.c
# the shell's line parser: words, quotes, $ and the operators, which is the
# half of a shell where a mistake runs the wrong thing
bin/tests/shparse:  tests/test_shparse.c  userland/shparse.c
# the library programs link against. the allocator is included rather
# than linked so its free list can be reset between sections, and
# `libc_get_pages` is the one seam: what is tested is the real
# allocator with only the source of memory replaced
bin/tests/libc:     tests/test_libc.c     userland/libc/stdlib.c \
                    userland/libc/format.c userland/libc/string.c
bin/tests/libc:     SRCS = tests/test_libc.c
# what an editor edits. an editor is a drawing loop around a data
# structure: the drawing needs a terminal and the structure needs
# nothing, and the structure is the half where a bug eats a file
bin/tests/textbuf:  tests/test_textbuf.c  userland/textbuf.c
# the property that matters is not that it spots a change (anything
# does) but that its edits, applied to the first file, give the
# second. that is checkable exactly on any pair of inputs
bin/tests/difflib:  tests/test_difflib.c  userland/difflib.c
# the git object store. a suite with no arguments, and with some the
# tool tools/gitcheck.py uses to build a whole repository, which real
# git is then asked to read
bin/tests/git:      tests/test_git.c      userland/gitobj.c userland/gitio_host.c \
                    userland/libc/hash.c userland/libc/inflate.c \
                    userland/libc/deflate.c
bin/tests/git:      HOSTFLAGS += -Iuserland
# sha-1, crc32 and adler32, a suite with no arguments and a hashing tool
# with some, which is how tools/hashcheck.py drives it against python,
# zlib and git
bin/tests/hash:     tests/test_hash.c     userland/libc/hash.c
# deflate and inflate, a suite with no arguments and a compressor or a
# decompressor with some, which is how tools/zlibcheck.py runs it
# against python's zlib and a real git repository
bin/tests/inflate:  tests/test_inflate.c  userland/libc/inflate.c \
                    userland/libc/deflate.c userland/libc/hash.c
# a date as a number and back, against the host's timegm
bin/tests/epoch:    tests/test_epoch.c    userland/libc/epoch.c

# not a suite that checks itself: it is the assembler, built as a
# program so tools/asmcheck.py can run it against nasm. an assembler
# cannot be eyeballed: wrong bytes look exactly like right ones
# built by a rule of its own, because it is not in TEST_BINS: it is not
# a suite that judges itself, so `make test` must not run it expecting a
# pass/fail. tools/asmcheck.py drives it instead
# the linker, built the same way and for the same reason: it produces an
# ELF nobody can read, and a wrong relocation is a program that loads
# perfectly and jumps four bytes past where it meant to
bin/tests/link: toolchain/linker.c
	@mkdir -p $(@D)
	$(HOSTCC) $(HOSTFLAGS) toolchain/linker.c -o $@

# and make, the same way again. what it produces is not commands but a
# *decision*, whether to run one at all, and a wrong decision looks
# exactly like a build that worked
bin/tests/make: toolchain/make.c
	@mkdir -p $(@D)
	$(HOSTCC) $(HOSTFLAGS) toolchain/make.c -o $@

# the archive writer, which is the other half of the linker reading one.
# checked by tools/archeck.py against GNU ar, both as a file and as
# something the other side can read
bin/tests/ar: toolchain/ar.c
	@mkdir -p $(@D)
	$(HOSTCC) $(HOSTFLAGS) toolchain/ar.c -o $@

bin/tests/asm: tests/test_asm.c toolchain/asmlib.c toolchain/asmlib.h \
               toolchain/asmpre.c toolchain/asmpre.h toolchain/asmelf.c toolchain/asmelf.h
	@mkdir -p $(@D)
	$(HOSTCC) $(HOSTFLAGS) tests/test_asm.c toolchain/asmlib.c toolchain/asmpre.c toolchain/asmelf.c -o $@

# every test depends on every header it could reach.
#
# coarse on purpose. the tests are compiled straight to a binary in one
# step and had no header prerequisites at all, so editing a header and
# running `make test` ran the *previous* binaries and reported them
# passing, which is the worst possible failure for a test suite, since
# it is indistinguishable from the change being harmless.
TEST_HEADERS := $(shell find userland toolchain tools -name '*.h' 2>/dev/null) \
                $(UAPI)
$(TEST_BINS): $(TEST_HEADERS)

# SRCS overrides what gets compiled, for tests that #include a .c file
# directly: that file still belongs in the prerequisites so make
# rebuilds when it changes, but compiling it twice would give me
# duplicate symbols
$(TEST_BINS):
	@mkdir -p $(@D)
	$(HOSTCC) $(HOSTFLAGS) $(if $(SRCS),$(SRCS),$(filter %.c,$^)) -o $@ $(LDLIBS)

# what asmcheck is run over: the fixtures, and all four kernel assembly
# files, every one of which now comes out indistinguishable from nasm's.
# trampoline.asm was absent until 0.4.0 because it diverged at byte 22
# on real mode's addressing, and tests/asm/real.asm is beside it for the
# same reason a fixture ever exists: the trampoline uses one corner of
# sixteen bits and that file uses the whole of it
ASM_CHECKED := tests/asm/basic.asm tests/asm/data.asm tests/asm/jumps.asm \
               tests/asm/memory.asm tests/asm/relocs.asm tests/asm/real.asm \
               $(KERNEL_SRC)/arch/x86_64/switch.asm \
               $(KERNEL_SRC)/arch/x86_64/syscall.asm \
               $(KERNEL_SRC)/arch/x86_64/isr.asm \
               $(KERNEL_SRC)/arch/x86_64/trampoline.asm

# the assembler is not a suite that checks itself: it is a program, and
# the only thing that can judge it is another assembler. so it gets a
# step of its own: every source in tests/asm/ is assembled by ours and
# by nasm and compared byte for byte
.PHONY: asmcheck
asmcheck: bin/tests/asm
	@python3 tools/asmcheck.py $(ASM_CHECKED)

# a repository written by ours and read by real git: fsck, log, status,
# and the tree hash compared against what `git write-tree` makes of the
# same directory. a store only this machine can read is a store that
# agrees with itself
.PHONY: gitcheck
gitcheck: bin/tests/git
	@python3 tools/gitcheck.py

# the sums under it, against python, zlib and `git hash-object`, every
# input fed again in chunks either side of the block size, since a
# single buffer hashed in one go is the case that cannot fail
.PHONY: hashcheck
hashcheck: bin/tests/hash
	@python3 tools/hashcheck.py

# deflate against python's zlib both ways round, at every level, and
# inflate against objects real git wrote
.PHONY: zlibcheck
zlibcheck: bin/tests/inflate
	@python3 tools/zlibcheck.py

# and make against gnu make. a fixture is a makefile and a sequence of
# edits, replayed against both, the edits being the point, since a
# make that gets a cold build right and an incremental one wrong is the
# normal kind of broken
# and the three of them together. our make reads a makefile, runs our
# assembler over two sources, and hands the objects to our linker: no
# gcc, no nasm, no ld, no gnu make anywhere in it. then the result is
# compared against what nasm and ld produce from the same two files,
# because "it built something" and "it built the right thing" are not
# the same claim
.PHONY: toolchain
toolchain: bin/tests/asm bin/tests/link bin/tests/make
	@rm -rf bin/toolchain
	@./bin/tests/make -f tests/toolchain/build.mk
	@nasm -f elf64 tests/link/a.asm -o bin/toolchain/n_a.o
	@nasm -f elf64 tests/link/b.asm -o bin/toolchain/n_b.o
	@ld -T tests/link/simple.ld -o bin/toolchain/gnu.elf \
		bin/toolchain/n_a.o bin/toolchain/n_b.o
	@for s in .text .rodata; do \
		objcopy -O binary --only-section=$$s bin/toolchain/demo.elf \
			bin/toolchain/ours.bin; \
		objcopy -O binary --only-section=$$s bin/toolchain/gnu.elf \
			bin/toolchain/theirs.bin; \
		cmp bin/toolchain/ours.bin bin/toolchain/theirs.bin \
			|| exit 1; \
	done
	@echo '  toolchain  ok      built by ours, identical to nasm and ld'

# and the whole of it, on the thing the project is for. our make reads
# tests/toolchain/kernel.mk, our assembler builds the four assembly
# files, our linker links ninety objects through the kernel's own
# script, and gcc is the only program in the chain that is not ours.
#
# then the result is held against the kernel gnu make, nasm and ld
# built: identical, except for the six bytes of __TIME__ that say when
# each of them was compiled
.PHONY: selfhost
#
# our make takes no variables on its command line, so where the kernel is
# goes to it in a file kernel.mk includes
selfhost: bin/tests/make bin/tests/asm bin/tests/link $(KERNEL_OUT) \
          $(RAMDISK) $(SOURCE)
	@mkdir -p obj/toolchain
	@echo 'KSRC = $(KERNEL_SRC)' > obj/toolchain/kernel-src.mk
	@./bin/tests/make -f tests/toolchain/kernel.mk
	@python3 tools/selfcheck.py

# and the last question, which no comparison can answer: does the thing
# it built run. needs qemu, so it is a target of its own beside
# boottest rather than part of the suite
.PHONY: selfboot
selfboot: selfhost $(KT)/mkext4
	KERNEL_SRC=$(KERNEL_SRC) ./tools/boottest.sh bin/toolchain/velvetos.img

.PHONY: makecheck
makecheck: bin/tests/make
	@python3 tools/makecheck.py tests/make/*

.PHONY: linkcheck
linkcheck: bin/tests/link
	@python3 tools/linkcheck.py tests/link/simple.ld \
		tests/link/a.asm tests/link/b.asm
	@python3 tools/linkcheck.py tests/link/paged.ld \
		tests/link/a.asm tests/link/b.asm tests/link/c.asm

# and the kernel, which is the version's whole claim: ninety objects, a
# script with PHDRS, symbol assignments, KEEP and /DISCARD/ in it, and
# every byte at the address ld puts it at. a fixture can be got right by
# accident; this one cannot. the objects are the ones the kernel's own
# build linked, in the order it linked them, which it writes down beside
# them for exactly this
KERNEL_OBJECTS := $(KOBJ)/x86_64/kernel.objects

.PHONY: kernelcheck
kernelcheck: bin/tests/link $(KERNEL_OUT)
	@python3 tools/linkcheck.py $(KERNEL_SRC)/arch/x86_64/linker.ld $$(cat $(KERNEL_OBJECTS))

.PHONY: archeck
archeck: bin/tests/ar bin/tests/link
	@python3 tools/archeck.py

# the kernel and the boot image are prerequisites, and that is not
# decoration: what this tree builds is a system, and a suite that passes
# while the system does not link has said nothing about the system. and
# since 0.3.26 there is a check that starts from the image rather than
# from anything the build says about it
.PHONY: test
test: $(KERNEL_OUT) $(BOOTIMG) $(USER_PROGS) $(RAMDISK) $(TEST_BINS) \
      bin/tests/asm bin/tests/link bin/tests/make bin/tests/ar
	@fail=0; \
	for t in $(TEST_BINS); do \
		printf '  %-10s ' "$$(basename $$t)"; \
		if out=$$(./$$t 2>&1); then \
			echo 'ok'; \
		else \
			echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
		fi; \
	done; \
	printf '  %-10s ' asmcheck; \
	if out=$$(python3 tools/asmcheck.py $(ASM_CHECKED) 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	printf '  %-10s ' linkcheck; \
	if out=$$(python3 tools/linkcheck.py tests/link/simple.ld \
			tests/link/a.asm tests/link/b.asm 2>&1 && \
		  python3 tools/linkcheck.py tests/link/paged.ld \
			tests/link/a.asm tests/link/b.asm tests/link/c.asm 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	printf '  %-10s ' kernelcheck; \
	if out=$$(python3 tools/linkcheck.py $(KERNEL_SRC)/arch/x86_64/linker.ld \
			$$(cat $(KERNEL_OBJECTS)) 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	printf '  %-10s ' archeck; \
	if out=$$(python3 tools/archeck.py 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	printf '  %-10s ' gitcheck; \
	if out=$$(python3 tools/gitcheck.py 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	printf '  %-10s ' hashcheck; \
	if out=$$(python3 tools/hashcheck.py 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	printf '  %-10s ' zlibcheck; \
	if out=$$(python3 tools/zlibcheck.py 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	printf '  %-10s ' srccheck; \
	if out=$$(python3 $(KERNEL_SRC)/tools/srccheck.py \
			--with kernel=$(KERNEL_SRC) $(BOOTIMG) 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	printf '  %-10s ' toolchain; \
	if out=$$($(MAKE) --no-print-directory toolchain 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	printf '  %-10s ' makecheck; \
	if out=$$(python3 tools/makecheck.py tests/make/* 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	printf '  %-10s ' selfhost; \
	if out=$$($(MAKE) --no-print-directory selfhost 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	if [ $$fail -eq 0 ]; then echo '  all suites passed'; else exit 1; fi

# the style guide, as much of it as a machine can hold: the file header,
# the shape of a multi-line comment, the function brace, dashes and first
# person in prose, tabs, the `//` rule, and a marker with an owner. the
# whole tree by default, or the files named in FILES while working on one.
# the kernel is not in the whole tree: it is fetched into obj/velvet/,
# which git here ignores, and its own `make lint` holds it to its own
# copy of the guide.
#
#   make lint                       the whole tree
#   make lint FILES=userland/ls.c   just that file
#
# naming prefixes are warnings rather than errors until the tree has been
# converted to them, which is why a clean run still prints a count.
.PHONY: lint
lint:
	@python3 tools/lint.py $(FILES)

# boot the image and drive the shell over serial. needs qemu
.PHONY: boottest
boottest: $(BOOTIMG) $(KT)/mkext4
	KERNEL_SRC=$(KERNEL_SRC) ./tools/boottest.sh $(BOOTIMG)

# obj/ is where the release was fetched to, so the next build fetches
# it again
clean:
	rm -rf bin obj

distclean: clean

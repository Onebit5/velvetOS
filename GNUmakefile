# velvetOS build. needs gcc, nasm, make. `make run` additionally needs qemu.
#
# targets:
#   make        -> kernel elf in bin/
#   make run    -> boot it in qemu (CPUS=4, MEM=512M, QEMU_EXTRA=...)
#   make clean

KERNEL := velvetos

# the bootable image, and the default target. defined up here with the
# other names because a prerequisite is expanded where it is written --
# further down, `all: $(BOOTIMG)` quietly meant `all:` with nothing to do
BOOTIMG := velvetos.img

# knobs for `make run`. they have to be make variables rather than extra
# words on the command line, because `make run -smp 4` hands -s -m -p to
# *make* -- and -p means "print the entire database", which is a
# surprising amount of ukrainian
CPUS ?= 4
MEM  ?= 2G
QEMU_EXTRA ?=

NASM := nasm

#
# x86_64 is the only one, and the mechanism for there being more than
# one is kept deliberately.
#
# an aarch64 port was written and removed again -- see ROADMAP.md. what
# it found while it existed is still here and is most of why 0.2.20's
# boundary is worth anything: a portable file building an x86 stack
# frame, kmain naming a machine, and a serial driver that turned out to
# be a terminal with a chip stuck to it. none of that needed the port to
# survive in order to stay fixed.
#
# what does still work without one:
#
#   make portable-check LIST=1     the portable half, against no arch
#   make arch-check ARCH=x86_64    one architecture, compiled alone
#
ARCH  ?= x86_64
CROSS ?=

CC := $(CROSS)gcc
LD := $(CROSS)ld

# one object tree per architecture. without this, switching ARCH and
# rebuilding links x86 objects into an arm kernel and the error it gives
# is about relocations rather than about what you actually did
OBJDIR := obj/$(ARCH)

# the portable drivers an architecture does not want, because it brings
# its own. empty while there is one architecture, and the mechanism is
# kept because the *reason* for it survived the port that needed it:
# drivers/serial.c is the terminal now and the chip lives in arch/
ARCH_SKIP_x86_64 :=
ARCH_SKIP := $(addprefix kernel/,$(ARCH_SKIP_$(ARCH)))

# freestanding kernel flags. the -mno-* soup is because I cant use fpu/sse
# in the kernel (no context saving yet), and no red zone because interrupts
# would trash it
# what both machines want: no libc, no floating point, no surprises
CFLAGS := -g -Wall -Wextra -std=gnu11 \
	-ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fno-PIC \
	-Ikernel -Iboot -MMD -MP -fno-omit-frame-pointer \
	-DVELVETOS_ARCH_$(shell echo $(ARCH) | tr a-z A-Z)

# x86: the -mno-* soup is because I cant use fpu/sse in the kernel (no
# context saving yet), and no red zone because interrupts would trash it
CFLAGS_x86_64 := -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
                 -mno-red-zone -mcmodel=kernel

CFLAGS += $(CFLAGS_$(ARCH))

LDFLAGS_x86_64  := -z max-page-size=0x1000 -T kernel/linker.ld
LDFLAGS := -nostdlib -static $(LDFLAGS_$(ARCH))

NASMFLAGS := -f elf64 -g

# the portable half, plus exactly one architecture.
#
# this used to be a plain `find kernel`, which was correct while
# there was one architecture and wrong the instant there were three --
# a build that globs everything is a build that assumes there is only
# one of everything, which stopped being true the moment arch/none
# existed -- it was quietly compiled into the x86 kernel.
CSRC := $(filter-out $(ARCH_SKIP), \
          $(shell find kernel -name '*.c' -not -path 'kernel/arch/*')) \
        $(shell find kernel/arch/$(ARCH) -name '*.c' 2>/dev/null)
ASRC := $(filter-out kernel/arch/$(ARCH)/trampoline.asm, \
          $(shell find kernel -name '*.asm' -not -path 'kernel/arch/*') \
          $(shell find kernel/arch/$(ARCH) -name '*.asm' 2>/dev/null))

# gnu-as sources. none today -- x86 uses nasm -- and the rule stays
# because an architecture that wants them should not have to add it back
SSRC := $(shell find kernel/arch/$(ARCH) -name '*.S' 2>/dev/null)
OBJ  := $(patsubst kernel/%.c,$(OBJDIR)/%.c.o,$(CSRC)) \
        $(patsubst kernel/%.asm,$(OBJDIR)/%.asm.o,$(ASRC)) \
        $(patsubst kernel/%.S,$(OBJDIR)/%.S.o,$(SSRC)) \
        $(if $(filter x86_64,$(ARCH)),$(OBJDIR)/arch/x86_64/trampoline.c.o,)

.PHONY: all run bootimg clean distclean

# the pc boots from an image philemon wrote. this board is handed an elf
# by qemu and there is nothing to write an image with, so the kernel
# itself is the deliverable
all: $(BOOTIMG)

# two passes, because the symbol table describes addresses and linking
# it in changes them. .ksyms sits after .text in the linker script, so
# folding it in shifts .data but cannot move a single function -- and
# gensyms --check proves that held instead of me just hoping.
bin/$(KERNEL): $(OBJ) $(OBJDIR)/srcstamp.o kernel/linker.ld tools/gensyms.py
	@mkdir -p $(@D) $(OBJDIR)
	@python3 tools/gensyms.py --stub > $(OBJDIR)/ksyms.c
	@$(CC) $(CFLAGS) -c $(OBJDIR)/ksyms.c -o $(OBJDIR)/ksyms.o
	@echo '  LD      pass 1 (to find out where everything landed)'
	@$(LD) $(LDFLAGS) $(OBJ) $(OBJDIR)/srcstamp.o $(OBJDIR)/ksyms.o -o $@.pass1
	@echo '  GENSYMS $(OBJDIR)/ksyms.c'
	@python3 tools/gensyms.py $@.pass1 > $(OBJDIR)/ksyms.c
	@$(CC) $(CFLAGS) -c $(OBJDIR)/ksyms.c -o $(OBJDIR)/ksyms.o
	@echo '  LD      pass 2 (with the symbols folded in)'
	@$(LD) $(LDFLAGS) $(OBJ) $(OBJDIR)/srcstamp.o $(OBJDIR)/ksyms.o -o $@
	@python3 tools/gensyms.py --check $@ $(OBJDIR)/ksyms.c
	@rm -f $@.pass1

$(OBJDIR)/%.c.o: kernel/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.S.o: kernel/%.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.asm.o: kernel/%.asm
	@mkdir -p $(@D)
	$(NASM) $(NASMFLAGS) $< -o $@

# the code a second cpu wakes up in. it runs in real mode at a fixed low
# address, which is nowhere the linker would put anything, so it is
# assembled flat and carried inside the kernel as bytes
$(OBJDIR)/arch/x86_64/trampoline.bin: kernel/arch/x86_64/trampoline.asm
	@mkdir -p $(@D)
	$(NASM) -f bin $< -o $@

$(OBJDIR)/arch/x86_64/trampoline.c: $(OBJDIR)/arch/x86_64/trampoline.bin tools/bin2c.py
	@python3 tools/bin2c.py smp_trampoline $< > $@

$(OBJDIR)/arch/x86_64/trampoline.c.o: $(OBJDIR)/arch/x86_64/trampoline.c
	$(CC) $(CFLAGS) -c $< -o $@

-include $(OBJ:.o=.d)

#
# a ring 3 program is built much like the kernel -- freestanding, no
# libc -- but without -mcmodel=kernel, since it lives at 0x400000 in
# the low half rather than up in the higher one. it gets copied into
# the ramdisk, which is how the kernel finds it.

UCFLAGS := -Wall -Wextra -std=gnu11 -O1 \
	-ffreestanding -fno-stack-protector -fno-stack-check -fno-pic \
	-fno-omit-frame-pointer \
	-m64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -Iuserland \
	-Iuserland/libc

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
              base/ramdisk/bin/git

# every program links the argument parser, so that what a program takes
# is declared once and read by both the parser and whatever has to
# explain the program to somebody
# the library every program links against, added in 0.3.8.
#
# `start.c` is the one that matters: it defines `_start`, calls `main`
# and exits with what it returned. before it, every program here was
# entered at `_start` and had to call `exit` itself -- which no program
# written anywhere else does, so every one of them had to be edited
# before it would build. that edit is what 0.3.8 removes.
#
# the library is an *archive* rather than a list of objects, and that is
# load-bearing rather than tidiness. an object named on a link line is
# always included; a member of an archive is pulled in only if it
# resolves something still undefined. so a program that writes its own
# `_start` never drags start.o in and never sees a duplicate, while one
# that writes `main` gets it -- which is exactly the rule that lets old
# programs and ported ones share a build.
LIBC_SRCS := userland/libc/stdlib.c userland/libc/format.c \
             userland/libc/stdio.c userland/libc/start.c
LIBC_OBJS := $(patsubst userland/libc/%.c,obj/libc_%.o,$(LIBC_SRCS))
LIBC_HDRS := userland/libc/string.h userland/libc/stdlib.h userland/libc/stdio.h \
             userland/libc/format.h

obj/libc_%.o: userland/libc/%.c $(LIBC_HDRS) userland/syscall.h
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -c $< -o $@

# the hashes go in the archive rather than into every program, which is
# what an archive is for: a member is pulled in only if something
# undefined needs it, so `sha1sum` gets them and `echo` does not. the
# source is the kernel's -- one implementation, checked once, and the
# alternative is the same file twice, which this project already has one
# of and does not need two
# the string functions, from the kernel's copy of them -- which is now
# the only copy. compiled twice: freestanding up there, hosted here
obj/libc_string.o: kernel/lib/string.c kernel/lib/string.h
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -Ikernel -c $< -o $@

obj/libc_hash.o: kernel/lib/hash.c kernel/lib/hash.h
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -Ikernel -c $< -o $@

# and the same for deflate, which a ring 3 program will need before it
# can read a git object -- and which being in here at all proves it
# compiles freestanding, with no libc of its own underneath it
obj/libc_inflate.o: kernel/lib/inflate.c kernel/lib/deflate.h
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -Ikernel -c $< -o $@

obj/libc_deflate.o: kernel/lib/deflate.c kernel/lib/deflate.h
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -Ikernel -c $< -o $@

# the date conversion, so a commit can carry one. there is no clock
# syscall; a file's timestamp and this are between them enough
obj/libc_epoch.o: kernel/lib/epoch.c kernel/lib/epoch.h
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -Ikernel -c $< -o $@

# `D` for the same reason the ramdisk tar carries --mtime=@0: an archive
# built twice from the same objects should be the same file, and the one
# thing that would stop it is a timestamp. whether that is ar's default
# is decided when binutils is compiled, so it is asked for rather than
# assumed
obj/libc.a: $(LIBC_OBJS) obj/libc_string.o obj/libc_hash.o \
            obj/libc_inflate.o obj/libc_deflate.o obj/libc_epoch.o
	$(AR) rcsD $@ $^

# git is the one program that links more than the common set: the object
# store and the seam under it. an explicit rule rather than adding them
# to every program, since nothing else wants them
base/ramdisk/bin/git: userland/git.c userland/gitobj.c userland/gitio_velvet.c userland/gitobj.h \
                 userland/gitio.h userland/args.c userland/difflib.c toolchain/linker.ld \
                 obj/libc.a
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -Ikernel -c userland/git.c -o obj/user_git.o
	$(CC) $(UCFLAGS) -Ikernel -c userland/gitobj.c -o obj/user_gitobj.o
	$(CC) $(UCFLAGS) -Ikernel -c userland/gitio_velvet.c -o obj/user_gitio.o
	$(CC) $(UCFLAGS) -c userland/args.c -o obj/user_args.o
	$(CC) $(UCFLAGS) -c userland/difflib.c -o obj/user_difflib.o
	$(LD) $(ULDFLAGS) obj/user_git.o obj/user_gitobj.o obj/user_gitio.o \
	      obj/user_args.o obj/user_difflib.o obj/libc.a -o $@

base/ramdisk/bin/%: userland/%.c userland/syscall.h userland/args.h userland/args.c userland/lines.h \
               userland/textbuf.c userland/textbuf.h userland/difflib.c userland/difflib.h \
               toolchain/linker.ld obj/libc.a
	@mkdir -p $(@D)
	$(CC) $(UCFLAGS) -Ikernel -c $< -o obj/user_$*.o
	$(CC) $(UCFLAGS) -c userland/args.c -o obj/user_args.o
	$(CC) $(UCFLAGS) -c userland/textbuf.c -o obj/user_textbuf.o
	$(CC) $(UCFLAGS) -c userland/difflib.c -o obj/user_difflib.o
	$(LD) $(ULDFLAGS) obj/user_$*.o obj/user_args.o obj/user_textbuf.o \
	      obj/user_difflib.o obj/libc.a -o $@

# the ramdisk is a plain tar. --format=ustar because thats the one the
# kernel knows how to read, and the flags after it keep the archive
# byte-identical between builds so the iso doesnt churn
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
# oversight -- the one moment anybody wants this archive is the moment
# something has gone badly wrong, and a recovery path that needs a
# decompressor between you and the source is a recovery path with one
# more thing in it to be broken. three and a half megabytes of a disk is
# not worth that.
#
# what goes in: everything the project is made of, which is a longer
# list than "everything the compiler reads". the tests are the reason
# any of this is trustworthy and the prose is where the reasoning
# lives, so both ship. what stays out is anything the build *produces*
# -- bin/, obj/, base/ramdisk/bin/ -- because a source tree carrying its own
# output is a source tree that disagrees with itself the moment you
# build it.
SOURCE := bin/source.tar

SOURCE_FILES := GNUmakefile README.md CHANGELOG.md ROADMAP.md \
                LICENSE FONT-LICENSE .gitignore \
                CONTRIBUTING.md CODE_OF_CONDUCT.md \
                $(shell find .github kernel boot userland toolchain tools tests docs -type f | sort) \
                $(shell find base/ramdisk base/diskroot -type f \
                        -not -path 'base/ramdisk/bin/*' | sort)

# --sort=name and a fixed mtime for the same reason the ramdisk has
# them: two builds of the same tree must produce the same bytes, or the
# stamp below is a fact about when I built it rather than about what I
# built.
#
# --mode is there for a subtler version of the same thing. git records
# one bit of a mode, the ramdisk rule above sets others by hand, and the
# result is that base/ramdisk/velvet-room.txt is 0600 after a build and 0644
# in a fresh checkout -- so the archive, and therefore the stamp, would
# depend on whether anybody had built anything yet. it states what it
# wants instead: everyone may read, only the owner may write, and
# whatever was executable stays executable
$(SOURCE): $(SOURCE_FILES)
	@mkdir -p $(@D)
	@echo '  TAR     $@ ($(words $(SOURCE_FILES)) files)'
	@tar --format=ustar --sort=name --owner=0 --group=0 --numeric-owner \
		--mtime=@0 --mode='u+rw,go+r,go-w' -cf $@ $(SOURCE_FILES)

# and the kernel is told what its own source hashes to.
#
# without this the machine can say "there is a source tree here" and
# nothing more, which is the weaker half of what is wanted. a disk that
# survives its author has to be able to answer "is this the source this
# kernel was built from?" -- and the only way to answer it is to have
# written the answer down at the one moment both halves were in the
# same room, which is now.
#
# the cost is that editing anything at all -- a line of the roadmap
# included -- relinks the kernel. that is not overhead so much as the
# claim being true: a kernel whose stamp survived an edit to its source
# would be a kernel telling a lie.
$(OBJDIR)/srcstamp.c: $(SOURCE) tools/srcstamp.py
	@mkdir -p $(@D)
	@python3 tools/srcstamp.py $(SOURCE) > $@

$(OBJDIR)/srcstamp.o: $(OBJDIR)/srcstamp.c kernel/fs/source.h
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@

#
# there was a borrowed one here until 0.1.12. writing a bootloader and
# then booting with somebody else's is not much of a bootloader, so it
# is gone -- along with the iso, the uefi path and the protocol that
# came with it. the kernel is handed one struct now, in rdi, and knows
# nothing about anybody's boot protocol including mine.

BOOTCC    := gcc
BOOTCFLAGS := -std=gnu11 -ffreestanding -fno-pic -fno-stack-protector \
              -mno-red-zone -mno-sse -mno-mmx -mno-80387 -mcmodel=small \
              -Wall -Wextra -O2 -Iboot

bin/boot/philemon.bin: boot/philemon.asm boot/philemon.inc
	@mkdir -p $(@D)
	$(NASM) -f bin $< -o $@

bin/boot/philemon64.bin: boot/philemon.c boot/philemon.h boot/philemon.ld
	@mkdir -p $(@D) $(OBJDIR)
	$(BOOTCC) $(BOOTCFLAGS) -c boot/philemon.c -o obj/philemon.o
	$(LD) -T boot/philemon.ld -nostdlib -static --no-warn-rwx-segments \
		obj/philemon.o -o obj/philemon.elf
	objcopy -O binary obj/philemon.elf $@

$(BOOTIMG): bin/boot/philemon.bin bin/boot/philemon64.bin \
            bin/$(KERNEL) $(RAMDISK) $(SOURCE) tools/mkboot.py \
            boot/philemon.inc boot/philemon.h
	@python3 tools/mkboot.py $@ bin/boot/philemon.bin \
		bin/boot/philemon64.bin bin/$(KERNEL) $(RAMDISK) $(SOURCE)

.PHONY: bootimg
bootimg: $(BOOTIMG)

# two drives: the one philemon is on, and the one with the files. the
# boot image goes first and is named as the boot device twice over --
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
# userspace nat, so 10.0.2.15 is not an address the host can route to --
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

# the disk, which is a real filesystem rather than an archive: built by
# tools/mkfat.py out of whatever is in base/diskroot/, and attached to qemu
# as a sata drive. it is deliberately NOT rebuilt by `make run` once it
# exists -- the whole point is that what you write to it stays written,
# and regenerating it every boot would quietly undo that. `make disk`
# starts over when you want a clean one
DISK := disk.img

# partitioned since 0.2.15, because a disk is not a filesystem -- it is
# a table saying where several of them are. two of them here, of
# different kinds, so that `parts` and `mount <n>` have something real
# to do and so that the two filesystems 0.2.14 left behind are both on
# the machine at once.
#
# gpt rather than mbr because it is the one with checksums, and a table
# that can be known to be corrupt is worth exercising. `mbr` in place of
# `gpt` below builds the other kind and everything works the same.
# the system partition is built by the C formatter rather than the
# python one, so the disk the machine actually boots is a real ext4 --
# extents and all -- rather than the ext2 the python still writes. that
# matters beyond tidiness: what this kernel runs on every day should be
# the thing it will have to format for itself in 0.3.25
# three of them since 0.3.25. the system, somewhere to *work* that is
# not the system, and the fat32 one that has been here since 0.2.15 --
# because a machine that mounts its work beside its system is the point
# of that version, and a third filesystem of another kind is still what
# `mount <n>` has to have something to do.
#
# the work image is made smaller than the partition it goes in, and
# that is deliberate: 8 MiB of filesystem inside 24 MiB of partition is
# exactly the situation `resize` exists for, so `make run` boots a
# machine with something to grow
$(DISK): bin/tests/mkext4 tools/mkfat.py tools/mkdisk.py          $(shell find base/diskroot -type f 2>/dev/null)
	@mkdir -p bin/workroot
	@echo 'somewhere to work that is not the system' > bin/workroot/readme.txt
	@bin/tests/mkext4 bin/velvet.img base/diskroot 32 >/dev/null
	@bin/tests/mkext4 bin/work.img bin/workroot 8 work >/dev/null
	@dd if=/dev/zero of=bin/work.img bs=1M count=0 seek=24 2>/dev/null
	@python3 tools/mkfat.py  bin/compendium.img base/diskroot 24 >/dev/null
	@python3 tools/mkdisk.py $@ gpt bin/velvet.img:linux bin/work.img:work \
		bin/compendium.img:fat32

.PHONY: disk
disk:
	@rm -f $(DISK)
	@$(MAKE) --no-print-directory $(DISK)

#
# the testable guts of the kernel are deliberately split from the parts
# that touch hardware: pmm_init_from_map() takes a memory map instead of
# asking the loader, keyboard_feed() takes a scancode instead of reading a
# port, and so on. that lets all of this run as ordinary linux programs.
# VELVETOS_HOSTED turns irq_save/irq_restore into no-ops, since userspace
# gets shot for saying cli.

HOSTCC    := gcc
HOSTFLAGS := -std=gnu11 -Wall -Wextra -Wno-stringop-truncation -Wno-stringop-overread -g -DVELVETOS_HOSTED -Ikernel -Iboot -DVELVETOS_ARCH_X86_64

TEST_BINS := bin/tests/kprintf bin/tests/mm bin/tests/buddy bin/tests/slab \
             bin/tests/vmm bin/tests/gdt \
             bin/tests/ksyms bin/tests/rtc bin/tests/ramdisk bin/tests/elf \
             bin/tests/addrspace bin/tests/process \
             bin/tests/syscall bin/tests/tty bin/tests/auth bin/tests/acpi bin/tests/pci \
             bin/tests/keyboard bin/tests/serial \
             bin/tests/fat32 bin/tests/vfs bin/tests/philemon \
             bin/tests/locks bin/tests/path bin/tests/args \
             bin/tests/shell bin/tests/pipe bin/tests/bcache \
             bin/tests/ext4 bin/tests/part bin/tests/console \
             bin/tests/mouse bin/tests/init bin/tests/mkfs \
             bin/tests/install bin/tests/net bin/tests/dhcp \
             bin/tests/tcp bin/tests/dns bin/tests/http \
             bin/tests/signal bin/tests/libc bin/tests/ansi \
             bin/tests/termios bin/tests/textbuf bin/tests/difflib \
             bin/tests/epoch bin/tests/pressure bin/tests/hash \
             bin/tests/inflate bin/tests/git bin/tests/crash \
             bin/tests/resize bin/tests/switch bin/tests/source

bin/tests/kprintf:  tests/test_kprintf.c  kernel/lib/kprintf.c \
                    kernel/sched/spinlock.c
bin/tests/mm:       tests/test_mm.c       kernel/sched/spinlock.c \
                    kernel/mm/pmm.c \
                    kernel/mm/buddy.c kernel/mm/slab.c \
                    kernel/mm/kmalloc.c kernel/lib/string.c
bin/tests/buddy:    tests/test_buddy.c    kernel/mm/buddy.c \
                    kernel/lib/string.c
bin/tests/slab:     tests/test_slab.c     kernel/sched/spinlock.c \
                    kernel/mm/slab.c \
                    kernel/mm/pmm.c kernel/mm/buddy.c \
                    kernel/mm/kmalloc.c kernel/lib/string.c
bin/tests/addrspace: tests/test_addrspace.c kernel/mm/addrspace.c kernel/mm/pressure.c \
                    kernel/mm/vmm.c kernel/mm/slab.c \
                    kernel/sched/spinlock.c kernel/lib/string.c
bin/tests/vmm:      tests/test_vmm.c      kernel/mm/vmm.c \
                    kernel/lib/string.c
bin/tests/ksyms:    tests/test_ksyms.c    kernel/lib/ksyms.c
bin/tests/rtc:      tests/test_rtc.c      kernel/drivers/rtc.c kernel/lib/epoch.c \
                    kernel/sched/spinlock.c
bin/tests/process:  tests/test_process.c  kernel/sched/spinlock.c \
                    kernel/fs/path.c kernel/sched/process.c kernel/sched/signal.c \
                    kernel/lib/env.c kernel/lib/string.c
bin/tests/pci:      tests/test_pci.c      kernel/drivers/pci.c \
                    kernel/lib/string.c
bin/tests/acpi:     tests/test_acpi.c     kernel/arch/x86_64/acpi.c \
                    kernel/lib/string.c
bin/tests/auth:     tests/test_auth.c     kernel/sched/auth.c \
                    kernel/lib/string.c
bin/tests/tty:      tests/test_tty.c      kernel/sched/spinlock.c \
                    kernel/drivers/tty.c kernel/drivers/termios.c \
                    kernel/sched/process.c kernel/sched/signal.c kernel/lib/env.c kernel/lib/string.c
# the real pipe, not a stub: what is under test here is that stdin and
# stdout end up somewhere other than the terminal when a pipeline says
# they should, and a stubbed pipe could only ever agree with itself
bin/tests/syscall:  tests/test_syscall.c  kernel/sched/spinlock.c \
                    kernel/fs/path.c kernel/arch/x86_64/syscall.c \
                    kernel/sched/process.c kernel/sched/signal.c kernel/lib/string.c \
                    kernel/lib/env.c \
                    kernel/fs/vfs.c kernel/fs/source.c \
                    kernel/lib/hash.c kernel/fs/pipe.c
bin/tests/elf:      tests/test_elf.c      kernel/fs/elf.c \
                    kernel/lib/string.c
bin/tests/ramdisk:  tests/test_ramdisk.c  kernel/fs/ramdisk.c \
                    kernel/lib/string.c
bin/tests/fat32:    tests/test_fat32.c    kernel/fs/fat32.c \
                    kernel/lib/string.c
# the one suite that can see a race: real threads through the real
# allocators, so it needs the real thread library
bin/tests/locks:    tests/test_locks.c    kernel/sched/spinlock.c \
                    kernel/mm/pmm.c kernel/mm/buddy.c \
                    kernel/mm/slab.c kernel/mm/kmalloc.c \
                    kernel/lib/string.c
bin/tests/locks:    LDLIBS = -pthread

bin/tests/path:     tests/test_path.c     kernel/fs/path.c
bin/tests/pipe:     tests/test_pipe.c     kernel/fs/pipe.c \
                    kernel/sched/spinlock.c kernel/lib/string.c
bin/tests/bcache:   tests/test_bcache.c   kernel/fs/bcache.c \
                    kernel/lib/string.c
bin/tests/ext4:     tests/test_ext4.c     kernel/fs/ext4.c kernel/fs/jbd2.c \
                    kernel/lib/string.c
# the formatter, checked against the *driver* rather than against itself.
# a formatter and a reader by one author can agree on something wrong and
# both be happy, which is exactly what happened to mkfat.py in 0.1.10
bin/tests/mkfs:     tests/test_mkfs.c     kernel/fs/mkfs.c \
                    kernel/fs/ext4.c kernel/fs/jbd2.c \
                    kernel/lib/string.c
# the three steps each have a test of their own; what this checks is the
# order they happen in, which none of the three can see
# almost all of a network stack is arithmetic over a byte buffer, so
# almost all of it can be checked with no card and nothing on the wire.
# the fixtures that matter are real captured packets -- a header I
# invent and a parser I invent agree with each other and with nothing
# else on earth
# a response arrives as whatever pieces tcp felt like delivering, so the
# central test is not "does this parse" but "does the split point
# matter" -- every fixture is driven through split at every byte offset
bin/tests/http:     tests/test_http.c     kernel/net/http.c \
                    tests/fixtures/http_captured.h \
                    kernel/net/net.c kernel/lib/string.c
# the first protocol here whose other end is somebody else's software,
# so most of the suite is malformed replies -- and nearly all of that is
# name compression, where a pointer may point backwards, forwards, or at
# itself, and nothing in the format forbids a cycle
bin/tests/dns:      tests/test_dns.c      kernel/net/dns.c \
                    kernel/net/net.c kernel/lib/string.c
# the largest state machine in the kernel, and the one whose bugs are
# least visible from outside -- a stack that gets the close sequence
# wrong works perfectly for every connection anybody watches
bin/tests/tcp:      tests/test_tcp.c      kernel/net/tcp.c \
                    kernel/net/net.c kernel/lib/string.c
# the first protocol here that remembers anything, so it gets a suite of
# its own: the interesting behaviour is in the *sequence*, and a clock
# that is an ordinary variable is what makes an eight-second backoff and
# a twelve-hour lease testable at all
bin/tests/dhcp:     tests/test_dhcp.c     kernel/net/dhcp.c \
                    kernel/net/net.c kernel/lib/string.c
bin/tests/net:      tests/test_net.c      kernel/net/net.c \
                    kernel/net/ether.c kernel/net/arp.c \
                    kernel/net/ip.c kernel/net/icmp.c \
                    kernel/net/udp.c kernel/net/socket.c \
                    kernel/lib/string.c
bin/tests/install:  tests/test_install.c  kernel/fs/install.c \
                    kernel/fs/mkfs.c kernel/drivers/part.c kernel/lib/hash.c \
                    kernel/fs/ext4.c kernel/fs/jbd2.c \
                    kernel/lib/string.c
bin/tests/part:     tests/test_part.c     kernel/drivers/part.c \
                    kernel/lib/string.c kernel/lib/hash.c
# sha-1, crc32 and adler32. a suite when run with no arguments and a
# hashing tool when run with some, which is how tools/hashcheck.py
# drives it against python, zlib and git
bin/tests/hash:     tests/test_hash.c     kernel/lib/hash.c \
                    kernel/lib/string.c
# deflate and inflate. a suite with no arguments, a compressor or a
# decompressor with some, which is how tools/zlibcheck.py runs it
# against python's zlib and against a real git repository
bin/tests/inflate:  tests/test_inflate.c  kernel/lib/inflate.c \
                    kernel/lib/deflate.c kernel/lib/hash.c \
                    kernel/lib/string.c
# the six python build tools, rewritten in C. not suites -- they are
# programs, and nothing they produce can judge itself, so they build
# outside TEST_BINS the way the assembler and the linker do and
# tools/toolcheck.py is what runs them.
#
# and built *beside* the python rather than instead of it, so that
# toolcheck has something to compare against: a swap that changes the
# build's output by accident is the failure this version is most likely
# to have, and it would look exactly like a build that worked
TOOL_BINS := bin/tests/bin2c bin/tests/checkfmt bin/tests/gensyms \
             bin/tests/mkboot bin/tests/mkext4 bin/tests/mkfat \
             bin/tests/fsck

bin/tests/bin2c:    tools/bin2c.c
bin/tests/checkfmt: tools/checkfmt.c
# gensyms reads the symbol table out of the ELF itself rather than
# asking nm, which is the point: a self-hosting machine has no more
# binutils on it than it has python
bin/tests/gensyms:  tools/gensyms.c
bin/tests/mkboot:   tools/mkboot.c
# not a rewrite so much as a driver: mkfs.c already formats ext2 and
# ext2.c already writes files, and both have been tested since 0.2.14.
# what was missing was a way to point them at a host file and a host
# directory -- 460 lines of python that laid out ext2 a second time
bin/tests/mkext4:   tools/mkext4.c         kernel/fs/mkfs.c \
                    kernel/fs/ext4.c kernel/fs/jbd2.c \
                    kernel/lib/string.c
# the last of the six, and the only one with code of its own in it:
# fat32.c mounts, creates and writes, but nothing anywhere formatted a
# fat32 volume -- mkfat.py was the only thing that knew how. so the
# boot sector, the two tables and the root cluster are laid out here,
# and the files go in through the same driver the kernel uses
bin/tests/mkfat:    tools/mkfat.c          kernel/fs/fat32.c \
                    kernel/lib/string.c
# and the checker, which is not one of the six -- there was no python
# fsck to replace, because until 0.3.23 the only checker was
# tools/readext4.py and it runs on the machine this was developed on
# rather than on this one
bin/tests/fsck:     userland/fsck.c           kernel/fs/fsck.c \
                    kernel/lib/string.c
# pulling the plug at every write there is. the driver, the formatter
# and the checker all at once, because what is under test is not any one
# of them -- it is whether the *order* they write in leaves damage that
# has an answer
bin/tests/crash:    tests/test_crash.c    kernel/fs/ext4.c \
                    kernel/fs/jbd2.c \
                    kernel/fs/mkfs.c kernel/fs/fsck.c \
                    kernel/lib/string.c
# growing a filesystem onto a disk bigger than the one it was made for.
# the formatter, the driver and the checker again, and for the same
# reason: what is under test is whether a resize leaves a filesystem,
# which is not a question the thing doing the resizing may answer
bin/tests/resize:   tests/test_resize.c   kernel/fs/ext4.c \
                    kernel/fs/jbd2.c \
                    kernel/fs/mkfs.c kernel/fs/fsck.c \
                    kernel/lib/string.c

# the git object store. a suite with no arguments, and with some the
# tool tools/gitcheck.py uses to build a whole repository -- which real
# git is then asked to read
bin/tests/git:      tests/test_git.c      userland/gitobj.c userland/gitio_host.c \
                    kernel/lib/hash.c kernel/lib/inflate.c \
                    kernel/lib/deflate.c
bin/tests/git:      HOSTFLAGS += -Iuserland
# the framebuffer is an array here, so what the screen shows can be read
# back. every path that touches pixels ends up in one place, which is
# the whole reason a console is testable without a machine
bin/tests/console:  tests/test_console.c  kernel/drivers/console.c \
                    kernel/drivers/ansi.c \
                    kernel/drivers/font.c kernel/lib/string.c
# only the decoder, which is the half with no hardware in it -- and the
# half where losing sync looks like a hardware fault and is not
bin/tests/mouse:    tests/test_mouse.c    kernel/drivers/mouse.c \
                    kernel/lib/string.c
# only the policy: what init decides about a service that keeps dying.
# the half that starts threads and takes the machine down is the half
# with a machine in it, and is #ifdef'd out here
bin/tests/init:     tests/test_init.c     kernel/sched/init.c \
                    kernel/lib/string.c
# delivery needs a machine and the rules do not -- and the rules are
# where signals go wrong: one that can be caught when it must not be, a
# handler re-entering itself, a pending set surviving a fork
bin/tests/signal:   tests/test_signal.c   kernel/sched/signal.c \
                    kernel/lib/string.c
# the library programs link against. the allocator is included rather
# than linked so its free list can be reset between sections, and
# `libc_get_pages` is the one seam -- what is tested is the real
# allocator with only the source of memory replaced
bin/tests/libc:     tests/test_libc.c     userland/libc/stdlib.c \
                    userland/libc/format.c
bin/tests/libc:     SRCS = tests/test_libc.c
# the escape sequences a program written for a real terminal sends. a
# pure state machine, and fed one byte at a time by the suite because
# that is what a program writing \033 and [2J separately produces
bin/tests/ansi:     tests/test_ansi.c     kernel/drivers/ansi.c
# what a terminal does about a key. the reading needs a machine and
# the deciding does not, and the deciding is where ctrl+c arrives as
# a signal or as the byte 0x03
bin/tests/termios:  tests/test_termios.c  kernel/drivers/termios.c
# what an editor edits. an editor is a drawing loop around a data
# structure: the drawing needs a terminal and the structure needs
# nothing, and the structure is the half where a bug eats a file
bin/tests/textbuf:  tests/test_textbuf.c  userland/textbuf.c
# the property that matters is not that it spots a change -- anything
# does -- but that its edits, applied to the first file, give the
# second. that is checkable exactly on any pair of inputs
bin/tests/difflib:  tests/test_difflib.c  userland/difflib.c
# a date into a number. checked against the host's own timegm, which
# is somebody else's implementation and the only opinion worth having
bin/tests/epoch:    tests/test_epoch.c    kernel/lib/epoch.c
# who gets refused when memory runs out. a policy rather than a data
# structure, and its interesting cases are all at the edges
bin/tests/pressure: tests/test_pressure.c kernel/mm/pressure.c
# not a suite that checks itself -- it is the assembler, built as a
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
# *decision* -- whether to run one at all -- and a wrong decision looks
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
bin/tests/args:     tests/test_args.c     userland/args.c
# the source mount is part of the namespace, so the suite that is
# responsible for resolution builds the real one and hands it a medium
# made of bytes rather than a drive
bin/tests/vfs:      tests/test_vfs.c      kernel/fs/vfs.c \
                    kernel/fs/ramdisk.c kernel/fs/source.c \
                    kernel/lib/hash.c kernel/lib/string.c

# and the archive on its own: a tar read a sector at a time off
# something that is not a filesystem, which is the one thing here that
# no other suite covers
bin/tests/source:   tests/test_source.c   kernel/fs/source.c \
                    kernel/lib/hash.c kernel/lib/string.c
bin/tests/philemon:  tests/test_philemon.c boot/philemon.c boot/philemon.h
bin/tests/gdt:      tests/test_gdt.c      kernel/arch/x86_64/gdt.c
bin/tests/gdt:      SRCS = tests/test_gdt.c
bin/tests/keyboard: tests/test_keyboard.c kernel/drivers/keyboard.c \
                    kernel/drivers/input.c \
                    kernel/sched/spinlock.c
bin/tests/serial:   tests/test_serial.c   kernel/drivers/serial.c \
                    kernel/drivers/input.c \
                    kernel/sched/spinlock.c
# the real process table, because the environment lives in it and the
# rules about what "already set" means are worth running rather than
# restating in a stub -- and the real init policy for the same reason,
# so that what `init` prints is what init would actually have decided
bin/tests/shell:    tests/test_shell.c    kernel/lib/string.c \
                    kernel/fs/ramdisk.c kernel/sched/auth.c \
                    kernel/drivers/pci.c kernel/fs/vfs.c \
                    kernel/fs/source.c \
                    kernel/sched/spinlock.c kernel/fs/path.c \
                    kernel/lib/env.c kernel/sched/init.c \
                    kernel/fs/install.c kernel/fs/mkfs.c \
                    kernel/drivers/part.c kernel/lib/hash.c kernel/fs/fsck.c \
                    kernel/net/net.c kernel/net/arp.c \
                    kernel/net/dhcp.c kernel/net/tcp.c \
                    kernel/net/dns.c kernel/net/ip.c kernel/mm/pressure.c \
                    kernel/sched/signal.c \
                    kernel/net/http.c \
                    kernel/shell/shell.c kernel/version.h
bin/tests/shell:    SRCS = tests/test_shell.c kernel/lib/string.c \
                           kernel/fs/ramdisk.c kernel/sched/auth.c \
                           kernel/drivers/pci.c kernel/fs/vfs.c \
                           kernel/fs/source.c \
                           kernel/sched/spinlock.c kernel/fs/path.c \
                           kernel/lib/env.c kernel/sched/init.c \
                           kernel/fs/install.c kernel/fs/mkfs.c \
                           kernel/drivers/part.c kernel/lib/hash.c kernel/fs/fsck.c \
                           kernel/net/net.c kernel/net/arp.c \
                           kernel/net/dhcp.c kernel/net/tcp.c \
                           kernel/net/dns.c kernel/net/ip.c \
                           kernel/net/http.c kernel/sched/signal.c \
                           kernel/mm/pressure.c

# every test depends on every kernel header.
#
# coarse on purpose. the kernel's own objects rebuild correctly because
# they are compiled one at a time with -MMD and the .d files are
# included; the tests are compiled straight to a binary in one step and
# had no header prerequisites at all. so editing a header and running
# `make test` ran the *previous* binaries and reported them passing --
# which is the worst possible failure for a test suite, since it is
# indistinguishable from the change being harmless.
#
# found by breaking a function in tcp.h on purpose and watching the
# suite pass anyway. rebuilding every test when any header changes costs
# a few seconds and removes the whole class of it.
TEST_HEADERS := $(shell find kernel boot userland toolchain tools -name '*.h' 2>/dev/null)
$(TEST_BINS): $(TEST_HEADERS)
$(TOOL_BINS): $(TEST_HEADERS)
	@mkdir -p $(@D)
	$(HOSTCC) $(HOSTFLAGS) $(filter %.c,$^) -o $@

# SRCS overrides what gets compiled, for tests that #include a kernel
# .c file directly -- that file still belongs in the prerequisites so
# make rebuilds when it changes, but compiling it twice would give me
# duplicate symbols
$(filter-out bin/tests/switch,$(TEST_BINS)):
	@mkdir -p $(@D)
	$(HOSTCC) $(HOSTFLAGS) $(if $(SRCS),$(SRCS),$(filter %.c,$^)) -o $@ $(LDLIBS)

# the switch test calls into the real switch.asm, and needs -no-pie so
# the `callq switch_context` in its inline asm resolves
obj/tests/switch.asm.o: kernel/arch/x86_64/switch.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@

bin/tests/switch: tests/test_switch.c obj/tests/switch.asm.o
	@mkdir -p $(@D)
	$(HOSTCC) $(HOSTFLAGS) -no-pie $(filter %.c %.o,$^) -o $@

# what asmcheck is run over: the fixtures, and all four kernel assembly
# files, every one of which now comes out indistinguishable from nasm's.
# trampoline.asm was absent until 0.4.0 because it diverged at byte 22
# on real mode's addressing, and tests/asm/real.asm is beside it for the
# same reason a fixture ever exists -- the trampoline uses one corner of
# sixteen bits and that file uses the whole of it
ASM_CHECKED := tests/asm/basic.asm tests/asm/data.asm tests/asm/jumps.asm \
               tests/asm/memory.asm tests/asm/relocs.asm tests/asm/real.asm \
               kernel/arch/x86_64/switch.asm \
               kernel/arch/x86_64/syscall.asm \
               kernel/arch/x86_64/isr.asm \
               kernel/arch/x86_64/trampoline.asm

# the assembler is not a suite that checks itself: it is a program, and
# the only thing that can judge it is another assembler. so it gets a
# step of its own, the way fsck and mkfsck do -- every source in
# tests/asm/ is assembled by ours and by nasm and compared byte for byte
.PHONY: asmcheck
asmcheck: bin/tests/asm
	@python3 tools/asmcheck.py $(ASM_CHECKED)

# and the linker, for the same reason and by the same method: link the
# same objects with ours and with ld and compare what actually gets
# loaded. two scripts, deliberately -- simple.ld packs everything into
# one page and paged.ld aligns each section onto its own, and a linker
# that always started a new segment per section matched ld on one of
# them and not the other
# the C build tools against the python they replace, while both exist
.PHONY: toolcheck
toolcheck: $(TOOL_BINS)
	@python3 tools/toolcheck.py

# a repository written by ours and read by real git: fsck, log, status,
# and the tree hash compared against what `git write-tree` makes of the
# same directory. a store only this machine can read is a store that
# agrees with itself
.PHONY: gitcheck
gitcheck: bin/tests/git
	@python3 tools/gitcheck.py

# deflate against python's zlib, both ways round, at every compression
# level -- the level decides which of the three block types comes out,
# so a decompressor checked only against our own compressor has seen one
# third of the format. plus the loose objects of a real git repository
.PHONY: zlibcheck
zlibcheck: bin/tests/inflate
	@python3 tools/zlibcheck.py

# the three sums against python, zlib and git. a published vector only
# ever tests one buffer hashed in one go, which is the case that cannot
# fail -- so this feeds three hundred lengths through in pieces of 1, 7,
# 63, 64 and 65 bytes as well
.PHONY: hashcheck
hashcheck: bin/tests/hash
	@python3 tools/hashcheck.py

# and make against gnu make. a fixture is a makefile and a sequence of
# edits, replayed against both -- the edits being the point, since a
# make that gets a cold build right and an incremental one wrong is the
# normal kind of broken
# and the three of them together. our make reads a makefile, runs our
# assembler over two sources, and hands the objects to our linker -- no
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
selfhost: bin/tests/make bin/tests/asm bin/tests/link bin/tests/gensyms \
          bin/tests/bin2c bin/tests/mkboot bin/$(KERNEL) $(RAMDISK) $(SOURCE)
	@./bin/tests/make -f tests/toolchain/kernel.mk
	@python3 tools/selfcheck.py

# and the last question, which no comparison can answer: does the thing
# it built run. needs qemu, so it is a target of its own beside
# boottest rather than part of the suite
.PHONY: selfboot
selfboot: selfhost bin/tests/mkext4
	./tools/boottest.sh bin/toolchain/velvetos.img

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
# accident; this one cannot
.PHONY: kernelcheck
kernelcheck: bin/tests/link bin/$(KERNEL)
	@python3 tools/linkcheck.py kernel/linker.ld \
		$(OBJ) $(OBJDIR)/srcstamp.o $(OBJDIR)/ksyms.o

.PHONY: archeck
archeck: bin/tests/ar bin/tests/link
	@python3 tools/archeck.py

.PHONY: test
# the fat32 suite runs against a real filesystem rather than a fixture,
# so it gets a throwaway image built by the same tool that builds the
# real one. rebuilt every time, without fail: the tests write to it, and
# a suite that passes only on a disk its last run left behind is worse
# than no suite at all
#
# and when the suite has finished writing to it, tools/readfat.py reads
# it back -- the `fatfsck` step below. that is not the same check as
# toolcheck's: toolcheck judges what the *formatter* wrote, this judges
# what the *driver* wrote, which since 0.3.21 includes long names it
# mints itself. a long name written wrong reads back perfectly here,
# because the code reading it is the code that wrote it, and is a
# different file on every other machine the disk is plugged into
.PHONY: fat32-image
fat32-image:
	@mkdir -p bin/tests
	@rm -f bin/tests/fat32.img
	@python3 tools/mkfat.py bin/tests/fat32.img base/diskroot 64 >/dev/null

# the ext2 suite gets a throwaway image too, and one with more in it
# than base/diskroot has: a file past twelve blocks so the indirect block is
# exercised, one past twelve plus two hundred and fifty six so the
# double indirect one is, and symlinks of both kinds -- short enough to
# live in the inode, and not
# two partitioned disks for the partition suite: one of each scheme,
# built by tools/mkdisk.py -- which was written from the specification
# separately from the parser that reads them back
.PHONY: part-images
part-images: ext4-image
	@mkdir -p bin/tests
	@rm -f bin/tests/mbr.img bin/tests/gpt.img bin/tests/small.img
	@python3 tools/mkext4.py bin/tests/small.img base/diskroot 4 >/dev/null
	@python3 tools/mkdisk.py bin/tests/mbr.img mbr \
		bin/tests/small.img:linux bin/tests/small.img:fat32 >/dev/null
	@python3 tools/mkdisk.py bin/tests/gpt.img gpt \
		bin/tests/small.img:linux bin/tests/small.img:linux >/dev/null

.PHONY: ext4-image
ext4-image: bin/tests/mkext4
	@mkdir -p bin/tests/ext4root/sub bin/tests/ext4root/notes
	@cp -r base/diskroot/. bin/tests/ext4root/ 2>/dev/null || true
	@python3 -c "open('bin/tests/ext4root/indirect.bin','wb').write(bytes((i*7+3)&0xff for i in range(20000)))"
	@python3 -c "open('bin/tests/ext4root/double.bin','wb').write(bytes((i*13+5)&0xff for i in range(400000)))"
	@echo deep > bin/tests/ext4root/notes/deep.txt
	@echo nested > bin/tests/ext4root/sub/nested.txt
	@rm -f bin/tests/ext4root/sub/link bin/tests/ext4root/slowlink
	@ln -s ../indirect.bin bin/tests/ext4root/sub/link
	@ln -s /a/very/long/target/path/that/will/not/fit/in/sixty/bytes/at/all/no bin/tests/ext4root/slowlink
	@rm -f bin/tests/ext4.img
# built by the C tool rather than the python, and that is the whole
# point of it since 0.3.22: mkfs.c writes a filesystem with the extents
# feature set and mkext4 populates it through fs/ext4.c, so the image
# the suite runs against is one whose files are mapped by extent trees
# the driver built itself. the python formatter still writes ext2, and
# still matters -- toolcheck holds the two against each other, which is
# what keeps the driver honest about reading both
	@bin/tests/mkext4 bin/tests/ext4.img bin/tests/ext4root 16 >/dev/null

# the kernel itself is a prerequisite, and that is not decoration. twice
# now every host suite has passed while the kernel did not link -- disk.c
# and shell.c are compiled into no host test, so nothing but building
# the real thing catches a missing function there
# and the boot image, because since 0.3.26 there is a check that starts
# from the image rather than from anything the build says about it
test: checkfmt checkarch portable-check bin/velvetos $(BOOTIMG) \
      $(USER_PROGS) $(RAMDISK) $(TEST_BINS) \
      bin/tests/asm bin/tests/link bin/tests/make bin/tests/ar $(TOOL_BINS) \
      fat32-image ext4-image part-images
	@fail=0; \
	for t in $(TEST_BINS); do \
		printf '  %-10s ' "$$(basename $$t)"; \
		case $$t in *fat32) arg=bin/tests/fat32.img;; *) arg=;; esac; \
		if out=$$(./$$t $$arg 2>&1); then \
			echo 'ok'; \
		else \
			echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
		fi; \
	done; \
	printf '  %-10s ' fatfsck; \
	if out=$$(python3 tools/readfat.py bin/tests/fat32.img 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	printf '  %-10s ' fsckcheck; \
	if out=$$(python3 tools/fsckcheck.py 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	printf '  %-10s ' fsck; \
	if out=$$(python3 tools/readext4.py bin/tests/ext4.img 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	printf '  %-10s ' mkfsck; \
	if out=$$(python3 tools/readext4.py bin/tests/mkfs.img 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
	printf '  %-10s ' growcheck; \
	if out=$$(python3 tools/readext4.py bin/tests/resize.img 2>&1); then \
		echo 'ok'; \
	else \
		echo 'FAILED'; echo "$$out" | sed 's/^/    /'; fail=1; \
	fi; \
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
	if out=$$(python3 tools/linkcheck.py kernel/linker.ld \
			$(OBJ) $(OBJDIR)/srcstamp.o $(OBJDIR)/ksyms.o 2>&1); then \
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
	printf '  %-10s ' toolcheck; \
	if out=$$(python3 tools/toolcheck.py 2>&1); then \
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
	printf '  %-10s ' zlibcheck; \
	if out=$$(python3 tools/zlibcheck.py 2>&1); then \
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
	printf '  %-10s ' srccheck; \
	if out=$$(python3 tools/srccheck.py $(BOOTIMG) 2>&1); then \
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

# the ext2 image is checked *after* the suite has finished writing to
# it, by a reader written from the on-disk layout rather than from the
# driver or the formatter. there is no e2fsck on this machine, so that
# separately written second opinion is the only thing standing between
# "the driver agrees with itself" and "the driver is right"

# gcc checks my format strings against real printf, which accepts far
# more than my kprintf implements. this catches the difference.
.PHONY: checkfmt
checkfmt:
	@printf '  %-10s ' checkfmt
	@python3 tools/checkfmt.py kernel && echo 'ok'

# the style guide, as much of it as a machine can hold: the file header,
# the shape of a multi-line comment, the function brace, dashes and first
# person in prose, tabs, the `//` rule, and a marker with an owner. the
# whole tree by default, or the files named in FILES while working on one.
#
#   make lint                   the whole tree
#   make lint FILES=userland/ls.c   just that file
#
# naming prefixes are warnings rather than errors until the tree has been
# converted to them, which is why a clean run still prints a count.
.PHONY: fuzz
fuzz: bin/tests/fuzz_parsers
	@bin/tests/fuzz_parsers

bin/tests/fuzz_parsers: tools/fuzz_parsers.c kernel/net/dns.c kernel/net/http.c                         kernel/net/tcp.c kernel/net/net.c kernel/lib/string.c
	$(HOSTCC) $(HOSTFLAGS) -fsanitize=undefined -fsanitize-trap=undefined 	    -fno-sanitize-recover=all $^ -o $@

.PHONY: lint
lint:
	@python3 tools/lint.py $(FILES)

# the arch boundary, which is worth exactly as much as whatever checks
# it. a line drawn in 0.2.20 and not checked would decay the first time
# somebody needed a `hlt` in a hurry -- silently, because the kernel goes
# on building and booting perfectly either way
.PHONY: checkarch
checkarch:
	@python3 tools/checkarch.py

# and the half checkarch cannot see. it greps -- it catches an #include
# and a line of inline asm, and it cannot catch a portable file that
# quietly *depends* on something only x86 supplies. this compiles the
# portable kernel against an architecture that does nothing, which can.
#
#   make portable-check LIST=1    and what a port would have to supply
.PHONY: portable-check
portable-check:
	@python3 tools/portable.py $(if $(LIST),--list,)

#
# compiles kernel/arch/$(ARCH)/ and nothing else, which is the only
# thing that can honestly be said about an architecture with no boot
# code yet. selftest.c calls every contract exactly once, because the
# headers are almost all `static inline` -- and an inline nobody calls
# is one the assembler never reads, so a header full of mistyped system
# registers would compile clean and say nothing at all.
ARCH_DIR   := kernel/arch/$(ARCH)
ARCH_CC    := $(CROSS)gcc
ARCH_UPPER := $(shell echo $(ARCH) | tr a-z A-Z)
ARCH_FLAGS := -std=gnu11 -Wall -Wextra -g -ffreestanding \
              -fno-stack-protector -fno-stack-check -fno-omit-frame-pointer \
              -DVELVETOS_ARCH_$(ARCH_UPPER) -Ikernel -Iboot -c

.PHONY: arch-check
arch-check:
	@command -v $(ARCH_CC) >/dev/null 2>&1 || { \
	  echo "  arch-check  no $(ARCH_CC) on PATH."; \
	  echo "              set CROSS= to whatever prefix that architecture uses."; \
	  exit 1; }
	@mkdir -p obj/archcheck
	@echo "  ARCH        $(ARCH), with $(ARCH_CC)"
	@for f in $(ARCH_DIR)/*.c $(ARCH_DIR)/*.S; do \
	   [ -e "$$f" ] || continue; \
	   printf '  CC          %s\n' "$$f"; \
	   $(ARCH_CC) $(ARCH_FLAGS) "$$f" -o obj/archcheck/`basename $$f`.o || exit 1; \
	 done
	@echo "  arch-check  ok -- $(ARCH) compiles. whether it is correct still needs a machine"

# boot the iso and drive the shell over serial. needs qemu
.PHONY: boottest
boottest: $(BOOTIMG) bin/tests/mkext4
	./tools/boottest.sh $(BOOTIMG)

clean:
	rm -rf bin obj iso_root $(ISO)

distclean: clean

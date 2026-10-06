# the kernel, built by this project's own tools.
#
# tests/toolchain/build.mk was 0.3.16's version of this: four rules, two
# sources and a demonstration. this one is the real thing. our make
# reads it, our assembler turns the four assembly files into objects,
# our linker turns ninety objects and a script with PHDRS in it into a
# kernel, and the only program in the chain that is not ours is the C
# compiler -- which is what 0.4.x is about and what the versions after
# this one replace.
#
# it is a separate file rather than a switch in the GNUmakefile on
# purpose. the two builds have to be able to run side by side and be
# compared, and a build that can only be had by turning the other one
# off cannot be.
#
# what it uses that our make could not read before 0.4.0:
#
#   $(shell find ...)  the sources are found rather than listed, which
#                      is what makes adding a file to the tree cost no
#                      edit here
#   -include           the dependency files gcc writes. they do not
#                      exist the first time, and a make that stopped
#                      there could never build anything once
#   ifdef              two ways of being told what happened
#   ::                 below, on the one target that has two reasons

CC    = gcc
ASM   = bin/tests/asm
LINK  = bin/tests/link
GENSYMS = bin/tests/gensyms
BIN2C = bin/tests/bin2c
MKBOOT = bin/tests/mkboot

OUT   = obj/toolchain
BIN   = bin/toolchain

# the same flags the ordinary build uses, because the point is that the
# *tools* changed and nothing else did
CFLAGS = -g -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
         -fno-stack-check -fno-lto -fno-PIC -Ikernel -Iboot -MMD -MP \
         -fno-omit-frame-pointer -DVELVETOS_ARCH_X86_64 \
         -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
         -mno-red-zone -mcmodel=kernel

CSRC = $(shell find kernel -name '*.c' -not -path 'kernel/arch/*') \
       $(shell find kernel/arch/x86_64 -name '*.c')
ASMSRC = $(shell find kernel -name '*.asm' -not -name 'trampoline.asm')

OBJ = $(patsubst kernel/%.c,$(OUT)/%.c.o,$(CSRC)) \
      $(patsubst kernel/%.asm,$(OUT)/%.asm.o,$(ASMSRC)) \
      $(OUT)/arch/x86_64/trampoline.c.o $(OUT)/srcstamp.o

all: $(BIN)/velvetos.img

#
# the symbol table records addresses and linking it in changes them.
# .ksyms sits after .text in the script, so folding it in shifts .data
# and can never move a function -- and `gensyms --check` proves that
# held rather than assuming it.
#
# it is the same dance the ordinary build does, with our linker doing
# the linking and the C gensyms reading the symbol table our linker
# wrote. that second part is the one worth watching: a kernel linked
# without a symbol table boots perfectly and cannot say where it died
$(BIN)/velvetos: $(OBJ) kernel/linker.ld $(GENSYMS)
	@mkdir -p $(BIN) $(OUT)
	@echo '  GENSYMS stub'
	@$(GENSYMS) --stub > $(OUT)/ksyms.c
	@$(CC) $(CFLAGS) -c $(OUT)/ksyms.c -o $(OUT)/ksyms.o
	@echo '  LINK    pass 1 (ours)'
	@$(LINK) -T kernel/linker.ld -o $@.pass1 $(OBJ) $(OUT)/ksyms.o
	@echo '  GENSYMS from a kernel our linker wrote'
	@$(GENSYMS) $@.pass1 > $(OUT)/ksyms.c
	@$(CC) $(CFLAGS) -c $(OUT)/ksyms.c -o $(OUT)/ksyms.o
	@echo '  LINK    pass 2 (ours)'
	@$(LINK) -T kernel/linker.ld -o $@ $(OBJ) $(OUT)/ksyms.o
	@$(GENSYMS) --check $@ $(OUT)/ksyms.c

# the image. philemon is taken from the ordinary build: a bootloader is
# sixteen-bit code with an `a32` in it and unreal mode underneath, which
# is the assembler's next mile rather than this one. the ramdisk and the
# source archive are taken the same way, and for the same reason -- what
# is being claimed here is the *kernel*, and saying so is better than
# quietly building less of the image than it looks like
$(BIN)/velvetos.img: $(BIN)/velvetos bin/boot/philemon.bin \
                     bin/boot/philemon64.bin bin/ramdisk.tar bin/source.tar
	@$(MKBOOT) $@ bin/boot/philemon.bin bin/boot/philemon64.bin \
		$(BIN)/velvetos bin/ramdisk.tar bin/source.tar


$(OUT)/%.c.o: kernel/%.c
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@

# ours, where the ordinary build says nasm
$(OUT)/%.asm.o: kernel/%.asm
	@mkdir -p $(@D)
	@$(ASM) -o $@ $<

# and the trampoline, which is not linked at all: it runs in real mode
# at a fixed low address, so it is assembled flat and carried into the
# kernel as bytes
$(OUT)/arch/x86_64/trampoline.bin: kernel/arch/x86_64/trampoline.asm
	@mkdir -p $(@D)
	@$(ASM) $< > $@

$(OUT)/arch/x86_64/trampoline.c: $(OUT)/arch/x86_64/trampoline.bin
	@$(BIN2C) smp_trampoline $< > $@

$(OUT)/arch/x86_64/trampoline.c.o: $(OUT)/arch/x86_64/trampoline.c
	@$(CC) $(CFLAGS) -c $< -o $@

# the stamp that says which source this kernel was built from. still
# python, and named here rather than hidden: 0.4.10 is where it becomes
# a C program, and until then this is the one thing in the chain that
# is neither ours nor the compiler's
$(OUT)/srcstamp.c: bin/source.tar
	@mkdir -p $(@D)
	@python3 tools/srcstamp.py bin/source.tar > $@

$(OUT)/srcstamp.o: $(OUT)/srcstamp.c
	@$(CC) $(CFLAGS) -c $< -o $@

# what gcc wrote down about which headers each object needs. absent on
# the first build, which is exactly why the dash is there
-include $(patsubst %.o,%.d,$(OBJ))

#
# two rules for one target, which is what `::` is for: one says how big
# the thing is and one says what built it, and they are independent
# reasons to say something rather than one recipe with two halves
said:: $(BIN)/velvetos
	@echo "  the kernel:  $(shell wc -c < $(BIN)/velvetos) bytes"

said:: $(BIN)/velvetos.img
	@echo "  the image:   $(shell wc -c < $(BIN)/velvetos.img) bytes"

ifdef QUIET
report:
	@echo '  built by ours'
else
report: said
	@echo '  built with no nasm, no ld and no gnu make in it'
endif

.PHONY: all report

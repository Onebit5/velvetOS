# the first thing built by this project's own tools rather than by the
# machine's. our make reads this file, our assembler turns the sources
# into objects, and our linker turns those into an executable -- and
# none of gcc, nasm, ld or gnu make is involved in any of it.
#
# it is four rules and a variable, which is the point: the tools are
# what is being demonstrated, not the build.
ASM   = bin/tests/asm
LINK  = bin/tests/link
OUT   = bin/toolchain

$(OUT)/demo.elf: $(OUT)/a.o $(OUT)/b.o
	$(LINK) -T tests/link/simple.ld -o $@ $^

$(OUT)/%.o: tests/link/%.asm
	@mkdir -p $(@D)
	$(ASM) -o $@ $<

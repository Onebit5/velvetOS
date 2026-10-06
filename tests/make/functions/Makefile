# the text functions. make has no numbers and no data structures -- a
# list is a string with spaces in it, and these are all there is for
# taking one apart.
SRCS = src/main.c src/util.c src/net.c
OBJS = $(patsubst %.c,%.o,$(SRCS))

all:
	@echo "objs   = $(OBJS)"
	@echo "subst  = $(subst src/,build/,$(SRCS))"
	@echo "notdir = $(notdir $(SRCS))"
	@echo "dir    = $(dir $(SRCS))"
	@echo "base   = $(basename $(notdir $(SRCS)))"
	@echo "filter = $(filter %util.c %net.c,$(SRCS))"
	@echo "out    = $(filter-out src/net.c,$(SRCS))"
	@echo "pre    = $(addprefix obj/,$(notdir $(OBJS)))"
	@echo "suf    = $(addsuffix .bak,$(notdir $(SRCS)))"
	@echo "first  = $(firstword $(SRCS)) of $(words $(SRCS))"
	@echo "strip  = [$(strip   spaced   out   )]"
	@echo "glob   = $(wildcard *.c) / $(wildcard here/*.h)"
	@echo "nested = $(subst .c,.o,$(filter %.c,$(SRCS)))"

.PHONY: all

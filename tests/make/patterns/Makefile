# one rule standing for however many files there are, which is what the
# automatic variables are for: without them a pattern rule could not say
# what it was working on.
CC = cc-pretend

all: a.o b.o
	@echo "^=$^ +=$+ <=$< @=$@"

%.o: %.c
	@echo "$(CC) $< -o $@ stem=$*"
	@cat $< > $@

# what a dependency file from -MMD says: one more prerequisite for a
# target whose recipe is the pattern's. the pattern's own prerequisite
# still comes first, so $< is the source and not the header, however
# the header got there
c.o: c.h

# a target that names the same prerequisite twice, so $^ and $+ differ
double: a.o a.o b.o
	@echo "^=$^"
	@echo "+=$+"

.PHONY: all double

# the shapes a graph actually comes in, none of which are a list.
#
# `shared` is needed by both halves and must be built once. that is not
# an optimisation -- a recipe that runs twice for one target is a recipe
# racing itself, and the second run of a compiler may well overwrite the
# output the first one's reader is already using.
all: left right
	@echo done

left: shared
	@echo left
	@cat shared > left

right: shared
	@echo right
	@cat shared > right

shared:
	@echo making shared once
	@echo contents > shared

# one rule, two targets: it says the same thing about both of them
pair.a pair.b: seed
	@echo pairing $@
	@cat seed > $@

# and one target across two rules: the prerequisites add up, and the
# recipe comes from whichever rule brought one
extra: pair.a
extra: pair.b
	@echo "extra needs $^"

# a rule and a recipe both continued across lines. the makefile joins
# its own with a space; a recipe keeps the backslash, because the shell
# is what joins those
LIST = pair.a \
       pair.b
long: $(LIST)
	@echo one \
	      long \
	      line

.PHONY: all long

# the same shape again with nothing on disk to hide it. a phony target
# is always out of date, so if `prep` is reached twice it *runs* twice,
# which is the failure the file-backed diamond above quietly absorbs
both: one two
one: prep
	@echo one
two: prep
	@echo two
prep:
	@echo preparing

.PHONY: both one two prep

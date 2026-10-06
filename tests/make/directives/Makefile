# the four things a real makefile asks for that a fixture never did.
#
# `include` and `-include`, because the dependency files a compiler
# writes do not exist the first time round and a make that stopped
# there could never build anything once; the conditionals, because a
# build with two of anything in it -- two architectures, two ways of
# being verbose -- has to be able to ask; `$(shell)`, because the list
# of sources in this project is *found* rather than written down; and
# `::`, because a target with two independent reasons to be rebuilt has
# two recipes and not one.

include config.mk

# the forgiving kind. absent, it says nothing at all -- which is the
# whole reason it exists
-include generated.mk

ifdef FLAVOUR
NAMED = FLAVOUR was set to $(FLAVOUR)
else
NAMED = nobody said
endif

ifndef ABSENT
MISSING = and ABSENT is not set either
endif

ifeq ($(FLAVOUR),plain)
KIND = plain
else ifeq ($(FLAVOUR),fancy)
KIND = fancy
else
KIND = something else
endif

ifneq "$(FLAVOUR)" "fancy"
SHAPE = not the fancy one
endif

# read while the makefile is, and it becomes what the command printed.
# the newline rule is the point: one path per line comes back as a list
COUNTED = $(shell echo one two three)
GREETING = $(shell cat greeting.txt)

all: report
	@echo "$(NAMED) / $(MISSING)"
	@echo "$(KIND) / $(SHAPE)"
	@echo "shell   = [$(COUNTED)] [$(GREETING)] [$(words $(COUNTED))]"
	@echo "included= $(FROM_CONFIG) / $(FROM_GENERATED)"

# two rules, two reasons, two recipes. touch one prerequisite and one of
# them runs -- which is the whole difference from the single-colon kind,
# where a target has one recipe however many rules mention it
report:: alpha.txt
	@echo alpha is newer
	@cat alpha.txt >> report

report:: beta.txt
	@echo beta is newer
	@cat beta.txt >> report

.PHONY: all

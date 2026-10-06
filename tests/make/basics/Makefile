# the two assignments, and the difference between them.
#
# NAME is recursive, so GREETING sees whatever NAME means when GREETING
# is *used* -- which is the last thing this file says about it. LATE is
# simple, so it froze the value that was there on its own line.
NAME = velvet
GREETING = hello $(NAME)
NAME = velvetOS
LATE := $(NAME)
NAME = changed

FLAGS = -one
FLAGS += -two
FLAGS ?= -never
EMPTY ?= -taken

all: greet flags
	@echo done

greet:
	@echo '$(GREETING)'
	@echo 'late=$(LATE)'

flags:
	@echo '$(FLAGS)$(EMPTY) $${SHELL_NOT_MAKE}'

.PHONY: all greet flags

# what it says when it cannot go on, which matters more than it looks
# like it does: a build that stops without naming what stopped it is a
# build somebody has to bisect by hand.
missing: nowhere.c
	@echo never

broken:
	@echo before
	false
	@echo after

ignored:
	-false
	@echo carried on

circle: loop
loop: circle
	@echo went round

.PHONY: broken ignored

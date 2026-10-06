# the case make exists for. a cold build, then nothing, then one file
# edited -- and the third of those is the only one that is hard.
app: main.o util.o
	@echo linking
	@cat main.o util.o > app

main.o: main.c common.h
	@echo cc main.c
	@cat main.c common.h > main.o

util.o: util.c common.h
	@echo cc util.c
	@cat util.c common.h > util.o

CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c99
LIBS=-lm

all: SlopMaster

SlopMaster: SlopMaster.c
	$(CC) $(CFLAGS) -o SlopMaster SlopMaster.c $(LIBS)

clean:
	rm -f SlopMaster

.PHONY: all clean
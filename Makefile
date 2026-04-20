CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -O2

all: wasmrun

wasmrun: main.c wasmrun.h
	$(CC) $(CFLAGS) -o $@ main.c

clean:
	rm -f wasmrun

.PHONY: all clean

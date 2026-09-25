CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra

all: libfastchess.a libfastchess.so fcperft

fastchess.o: fastchess.c fastchess.h
	$(CC) $(CFLAGS) -fPIC -c fastchess.c -o $@

libfastchess.a: fastchess.o
	ar rcs $@ $^

libfastchess.so: fastchess.o
	$(CC) -shared -o $@ $^

fcperft: perft.c libfastchess.a
	$(CC) $(CFLAGS) -o $@ perft.c libfastchess.a

clean:
	rm -f fastchess.o libfastchess.a libfastchess.so fcperft

.PHONY: all clean

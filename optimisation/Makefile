CC=gcc
CFLAGS=--std=gnu99 -O2 -Wall -Wconversion -Wno-sign-conversion -pedantic
LDFLAGS=-lm -lgsl -lgslcblas
UNAME=$(shell uname)

TARGETS += mds calibrate

.PHONY:all $(TARGETS)
.SECONDARY:

all: $(TARGETS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

calibrate: calibrate.o common.o randlib.o
	$(CC) $(CFLAGS) -O3 -o $@ $^ -lm $(LDFLAGS)

mds: mds.o common.o randlib.o
	$(CC) $(CFLAGS) -O3 -o $@ $^ -lm $(LDFLAGS)

clean:
	rm -f *.o $(TARGETS)

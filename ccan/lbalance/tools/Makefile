#! /usr/bin/make
MODULES=../../jmap.o ../../time.o

CFLAGS=-I../../.. -g #-O2
LDFLAGS=-lJudy

lbalance: lbalance.c $(MODULES)

$(MODULES):
	make -C ../../.. $(patsubst ../../%.o, ccan/%.o, $@) EXCLUDE=

clean:
	rm -f lbalance $(MODULES)

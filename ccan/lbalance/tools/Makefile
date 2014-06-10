#! /usr/bin/make
CCANDIR=../../..

CCAN_OBJS:=ccan-jmap.o ccan-time.o

CFLAGS=-I$(CCANDIR) -Wall -g #-O2
LDLIBS=-lJudy

lbalance: lbalance.c $(CCAN_OBJS)

clean:
	rm -f lbalance $(CCAN_OBJS)

ccan-jmap.o: $(CCANDIR)/ccan/jmap/jmap.c
	$(CC) $(CFLAGS) -c -o $@ $<

ccan-time.o: $(CCANDIR)/ccan/time/time.c
	$(CC) $(CFLAGS) -c -o $@ $<

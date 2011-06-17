OBJS:=../../tdb2.o ../../hash.o ../../tally.o
CFLAGS:=-I../../.. -I.. -Wall -g -O3 #-g -pg
LDFLAGS:=-L../../..

default: tdb2torture tdb2tool tdb2dump tdb2restore mktdb2 speed growtdb-bench

tdb2dump: tdb2dump.c $(OBJS)
tdb2restore: tdb2restore.c $(OBJS)
tdb2torture: tdb2torture.c $(OBJS)
tdb2tool: tdb2tool.c $(OBJS)
mktdb2: mktdb2.c $(OBJS)
speed: speed.c $(OBJS)
growtdb-bench: growtdb-bench.c $(OBJS)

clean:
	rm -f tdb2torture tdb2dump tdb2restore tdb2tool mktdb2 speed growtdb-bench

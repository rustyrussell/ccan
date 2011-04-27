OBJS:=../../tdb2.o ../../hash.o ../../tally.o
CFLAGS:=-I../../.. -Wall -g -O3 #-g -pg
LDFLAGS:=-L../../..

default: tdbtorture tdbtool mktdb speed growtdb-bench

tdbtorture: tdbtorture.c $(OBJS)
tdbtool: tdbtool.c $(OBJS)
mktdb: mktdb.c $(OBJS)
speed: speed.c $(OBJS)
growtdb-bench: growtdb-bench.c $(OBJS)

clean:
	rm -f tdbtorture tdbtool mktdb speed growtdb-bench

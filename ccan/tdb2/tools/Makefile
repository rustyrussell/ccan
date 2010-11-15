OBJS:=../../tdb2.o ../../hash.o ../../tally.o
CFLAGS:=-I../../.. -Wall -g #-g -O3 #-g -pg
LDFLAGS:=-L../../..

default: tdbtorture tdbtool mktdb

tdbtorture: tdbtorture.c $(OBJS)
tdbtool: tdbtool.c $(OBJS)
mktdb: mktdb.c $(OBJS)

clean:
	rm -f tdbtorture tdbtool mktdb

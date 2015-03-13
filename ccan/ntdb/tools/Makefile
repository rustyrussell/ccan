OBJS:=../../ntdb.o ../../hash.o ../../tally.o
CFLAGS:=-I../../.. -I.. -Wall -g -O3 #-g -pg
LDFLAGS:=-L../../..

default: ntdbtorture ntdbtool ntdbdump ntdbrestore mkntdb speed growtdb-bench

ntdbdump: ntdbdump.c $(OBJS)
ntdbrestore: ntdbrestore.c $(OBJS)
ntdbtorture: ntdbtorture.c $(OBJS)
ntdbtool: ntdbtool.c $(OBJS)
mkntdb: mkntdb.c $(OBJS)
speed: speed.c $(OBJS)
growtdb-bench: growtdb-bench.c $(OBJS)

clean:
	rm -f ntdbtorture ntdbdump ntdbrestore ntdbtool mkntdb speed growtdb-bench

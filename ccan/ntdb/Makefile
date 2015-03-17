CC=gcc
CFLAGS=-g -O0 -Wall -W -I../../ -I./
LIBS=

LIBNTDB_OBJ = ccan_hash.o ccan_tally.o check.o free.o hash.o io.o lock.o open.o summary.o ntdb.o transaction.o traverse.o

all: ntdbtorture ntdbtool ntdbdump ntdbrestore ntdbbackup

ntdbtorture: tools/ntdbtorture.c libntdb.a
	$(CC) $(CFLAGS) -o tools/$@ tools/$@.c libntdb.a $(LIBS)

ntdbtool: tools/ntdbtool.c libntdb.a
	$(CC) $(CFLAGS) -o tools/$@ tools/$@.c libntdb.a $(LIBS)

ntdbdump: tools/ntdbdump.c libntdb.a
	$(CC) $(CFLAGS) -o tools/$@ tools/$@.c libntdb.a $(LIBS)

ntdbrestore: tools/ntdbrestore.c libntdb.a
	$(CC) $(CFLAGS) -o tools/$@ tools/$@.c libntdb.a $(LIBS)

ntdbbackup: tools/ntdbbackup.c libntdb.a
	$(CC) $(CFLAGS) -o tools/$@ tools/$@.c libntdb.a $(LIBS)

libntdb.a: $(LIBNTDB_OBJ)
	@echo Creating library $@
	ar r libntdb.a $(LIBNTDB_OBJ)
	ranlib libntdb.a

check.o: check.c
	@echo Compiling $@
	$(CC) $(CFLAGS) -c check.c -o $@

free.o: free.c
	@echo Compiling $@
	$(CC) $(CFLAGS) -c free.c -o $@

hash.o: hash.c
	@echo Compiling $@
	$(CC) $(CFLAGS) -c hash.c -o $@

io.o: io.c
	@echo Compiling $@
	$(CC) $(CFLAGS) -c io.c -o $@

lock.o: lock.c
	@echo Compiling $@
	$(CC) $(CFLAGS) -c lock.c -o $@

open.o: open.c
	@echo Compiling $@
	$(CC) $(CFLAGS) -c open.c -o $@

summary.o: summary.c
	@echo Compiling $@
	$(CC) $(CFLAGS) -c summary.c -o $@

ntdb.o: ntdb.c
	@echo Compiling $@
	$(CC) $(CFLAGS) -c ntdb.c -o $@

transaction.o: transaction.c
	@echo Compiling $@
	$(CC) $(CFLAGS) -c transaction.c -o $@

traverse.o: traverse.c
	@echo Compiling $@
	$(CC) $(CFLAGS) -c traverse.c -o $@

ccan_hash.o: ../hash/hash.c
	@echo Compiling $@
	$(CC) $(CFLAGS) -c ../hash/hash.c -o $@

ccan_tally.o: ../tally/tally.c
	@echo Compiling $@
	$(CC) $(CFLAGS) -c ../tally/tally.c -o $@

clean:
	rm -f *.o
	rm -f *.a
	rm -f tools/ntdbtorture tools/ntdbtool tools/ntdbdump tools/ntdbrestore tools/ntdbbackup

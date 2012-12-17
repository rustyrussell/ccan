CFLAGS=-g -Wall -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -I../../.. -O3 -flto
LDFLAGS=-O3 -flto
LDLIBS=-ljpeg -lm

DEPS := ccan-alloc.o ccan-alloc-bitops.o ccan-alloc-tiny.o ccan-err.o ccan-list.o ccan-noerr.o ccan-tal.o ccan-tal-str.o ccan-ilog.o ccan-likely.o ccan-str.o ccan-take.o ccan-antithread.o

all: dns_lookup arabella

dns_lookup: dns_lookup.o $(DEPS)
arabella: arabella.o $(DEPS)

ccan-alloc.o: ../alloc/alloc.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-alloc-bitops.o: ../alloc/bitops.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-alloc-tiny.o: ../alloc/tiny.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-antithread.o: ../antithread.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-err.o: ../../err/err.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-list.o: ../../list/list.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-noerr.o: ../../noerr/noerr.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-tal.o: ../../tal/tal.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-tal-str.o: ../../tal/str/str.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-ilog.o: ../../ilog/ilog.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-likely.o: ../../likely/likely.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-str.o: ../../str/str.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-take.o: ../../take/take.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f dns_lookup arabella *.o

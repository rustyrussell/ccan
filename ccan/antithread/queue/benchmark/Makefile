CFLAGS=-g -Wall -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -I../../../.. -O3 -flto
LDFLAGS=-O3 -flto
LDLIBS=-lrt

DEPS := ccan-alloc.o ccan-alloc-bitops.o ccan-alloc-tiny.o ccan-err.o ccan-list.o ccan-noerr.o ccan-tal.o ccan-tal-str.o ccan-time.o ccan-ilog.o ccan-likely.o ccan-queue.o ccan-str.o ccan-take.o ccan-antithread.o

STATS=stats.c --trim-outliers

loop=for i in `seq 100`; do $1; done | $(STATS)

all: qbench

benchmarks: benchmarks-dumb benchmarks-full benchmarks-excl

# Measuring a dumb queue
benchmarks-dumb: qbench
	@$(call loop,./qbench --dumb 1000000 1 1)
	@$(call loop,./qbench --dumb 1000000 1 2)
	@$(call loop,./qbench --dumb 1000000 1 3)
	@$(call loop,./qbench --dumb 1000000 2 1)
	@$(call loop,./qbench --dumb 1000000 3 1)
	@$(call loop,./qbench --dumb 1000000 2 2)

# Completely generic, no assumptions about exclusivity.
benchmarks-full:
	@$(call loop,./qbench 1000000 1 1)
	@$(call loop,./qbench 1000000 1 2)
	@$(call loop,./qbench 1000000 1 3)
	@$(call loop,./qbench 1000000 2 1)
	@$(call loop,./qbench 1000000 3 1)
	@$(call loop,./qbench 1000000 2 2)

# Benchmark using excl where possible
benchmarks-excl: qbench
	@$(call loop,./qbench --xprod 1000000 1 1)
	@$(call loop,./qbench --xprod 1000000 1 2)
	@$(call loop,./qbench --xprod 1000000 1 3)
	@$(call loop,./qbench --xprod --xcons 1000000 1 1)
	@$(call loop,./qbench --xcons 1000000 1 1)
	@$(call loop,./qbench --xcons 1000000 2 1)
	@$(call loop,./qbench --xcons 1000000 3 1)

qbench: qbench.o $(DEPS)

ccan-alloc.o: ../../alloc/alloc.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-alloc-bitops.o: ../../alloc/bitops.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-alloc-tiny.o: ../../alloc/tiny.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-antithread.o: ../../antithread.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-queue.o: ../queue.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-err.o: ../../../err/err.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-list.o: ../../../list/list.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-noerr.o: ../../../noerr/noerr.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-tal.o: ../../../tal/tal.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-tal-str.o: ../../../tal/str/str.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-time.o: ../../../time/time.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-ilog.o: ../../../ilog/ilog.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-likely.o: ../../../likely/likely.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-str.o: ../../../str/str.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-take.o: ../../../take/take.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f qbench *.o

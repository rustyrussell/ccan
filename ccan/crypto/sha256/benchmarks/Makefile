CCANDIR := ../../../../
CFLAGS := -Wall -I$(CCANDIR) -O3 -flto
LDFLAGS := -O3 -flto

double-sha-bench: double-sha-bench.o ccan-time.o ccan-crypto-sha256.o

ccan-crypto-sha256.o: $(CCANDIR)/ccan/crypto/sha256/sha256.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-time.o: $(CCANDIR)/ccan/time/time.c
	$(CC) $(CFLAGS) -c -o $@ $<

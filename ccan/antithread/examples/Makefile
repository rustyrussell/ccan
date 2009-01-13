CFLAGS=-g -O3 -Wall -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -I../../..

all: dns_lookup

dns_lookup: dns_lookup.c ../../../libccan.a
	$(CC) $(CFLAGS) -o $@ $^

arabella: arabella.c ../../../libccan.a
	$(CC) $(CFLAGS) -o $@ $^ -ljpeg -lm

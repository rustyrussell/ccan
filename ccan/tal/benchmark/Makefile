CFLAGS=-O3 -Wall -flto -I../../..
#CFLAGS=-O3 -Wall -I../../..
#CFLAGS=-g -Wall -I../../..
LDFLAGS=-O3 -flto
LDLIBS=-lrt

all: speed samba-allocs

speed: speed.o tal.o talloc.o time.o hash.o list.o
samba-allocs: samba-allocs.o tal.o talloc.o time.o hash.o list.o

tal.o: ../tal.c
	$(CC) $(CFLAGS) -c -o $@ $<
talloc.o: ../../talloc/talloc.c
	$(CC) $(CFLAGS) -c -o $@ $<
time.o: ../../time/time.c
	$(CC) $(CFLAGS) -c -o $@ $<
hash.o: ../../hash/hash.c
	$(CC) $(CFLAGS) -c -o $@ $<
list.o: ../../list/list.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f speed samba-allocs *.o

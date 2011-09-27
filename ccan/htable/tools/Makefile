CFLAGS=-Wall -Werror -O3 -I../../..
#CFLAGS=-Wall -Werror -g -I../../..

all: speed stringspeed hsearchspeed

speed: speed.o hash.o

speed.o: speed.c ../htable.h ../htable.c

hash.o: ../../hash/hash.c
	$(CC) $(CFLAGS) -c -o $@ $<

stringspeed: stringspeed.o hash.o ../../talloc.o ../../str_talloc.o ../../grab_file.o ../../str.o ../../time.o ../../noerr.o

stringspeed.o: speed.c ../htable.h ../htable.c

hsearchspeed: hsearchspeed.o ../../talloc.o ../../str_talloc.o ../../grab_file.o ../../str.o ../../time.o ../../noerr.o

clean:
	rm -f stringspeed speed hsearchspeed *.o

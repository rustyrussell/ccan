CFLAGS=-Wall -Werror -O3 -I../../..
#CFLAGS=-Wall -Werror -g -I../../..

all: cbspeed speed

cbspeed: cbspeed.o ../../talloc.o ../../str_talloc.o ../../grab_file.o ../../str.o ../../time.o ../../noerr.o

speed: speed.o ../../talloc.o ../../str_talloc.o ../../grab_file.o ../../str.o ../../time.o ../../noerr.o

clean:
	rm -f cbspeed speed speed.o cbspeed.o

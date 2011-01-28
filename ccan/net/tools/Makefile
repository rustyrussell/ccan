CFLAGS=-g -Wall -W -I../../..

test-net: test-net.o net.o noerr.o

net.o: ../net.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

noerr.o: ../../noerr/noerr.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

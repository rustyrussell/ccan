LIBS=
CC=gcc
CFLAGS=-g -O0 -Wall -W -I../..
LIBISCSI_OBJ = socket.o init.o login.o nop.o pdu.o discovery.o scsi-command.o scsi-lowlevel.o

all: tools/iscsiclient

tools/iscsiclient: tools/iscsiclient.o libiscsi.a
	$(CC) $(CFLAGS) -o $@ tools/iscsiclient.o libiscsi.a $(LIBS)

libiscsi.a: $(LIBISCSI_OBJ)
	@echo Creating library $@
	ar r libiscsi.a $(LIBISCSI_OBJ) 
	ranlib libiscsi.a

tools/iscsiclient.o: tools/iscsiclient.c
	@echo Compiling $@
	$(CC) $(CFLAGS) -c tools/iscsiclient.c -o $@

socket.o: socket.c iscsi.h iscsi-private.h

init.o: init.c iscsi.h iscsi-private.h

login.o: login.c iscsi.h iscsi-private.h

pdu.o: pdu.c iscsi.h iscsi-private.h

nop.o: nop.c iscsi.h iscsi-private.h

discovery.o: discovery.c iscsi.h iscsi-private.h

scsi-command.o: scsi-command.c iscsi.h iscsi-private.h

scsi-lowlevel.o: scsi-lowlevel.c scsi-lowlevel.h

clean:
	rm -f tools/iscsiclient
	rm -f *.o
	rm -f libiscsi.a

CC=gcc
CFLAGS=-g -O0 -Wall -W -I../..
LIBS=

LIBNFS_OBJ = libnfs-raw-mount.o libnfs-raw-portmap.o libnfs-raw-nfs.o libnfs-raw-nfsacl.o mount.o nfs.o nfsacl.o portmap.o pdu.o init.o socket.o libnfs.o libnfs-sync.o

LICENSE_LINE = echo '/* Licensed under GPLv3+ - see LICENSE file for details */' > $@

all: tools/nfsclient-raw tools/nfsclient-async tools/nfsclient-sync

tools/nfsclient-async: tools/nfsclient-async.c libnfs.a
	$(CC) $(CFLAGS) -o $@ tools/nfsclient-async.c libnfs.a $(LIBS)

tools/nfsclient-sync: tools/nfsclient-sync.c libnfs.a
	$(CC) $(CFLAGS) -o $@ tools/nfsclient-sync.c libnfs.a $(LIBS)

tools/nfsclient-raw: tools/nfsclient-raw.c libnfs.a
	$(CC) $(CFLAGS) -o $@ tools/nfsclient-raw.c libnfs.a $(LIBS)

libnfs.a: $(LIBNFS_OBJ)
	@echo Creating library $@
	ar r libnfs.a $(LIBNFS_OBJ) 
	ranlib libnfs.a

rpc/mount.h: rpc/mount.x
	@echo Generating $@
	$(LICENSE_LINE) && rpcgen -h rpc/mount.x >> $@

libnfs-raw-mount.c: rpc/mount.x
	@echo Generating $@
	$(LICENSE_LINE) && rpcgen -c rpc/mount.x >> $@

libnfs-raw-mount.o: libnfs-raw-mount.c rpc/mount.h
	@echo Compiling $@
	gcc -g -c libnfs-raw-mount.c -o $@

rpc/nfs.h: rpc/nfs.x
	@echo Generating $@
	$(LICENSE_LINE) && rpcgen -h rpc/nfs.x >> $@

libnfs-raw-nfs.c: rpc/nfs.x
	@echo Generating $@
	$(LICENSE_LINE) && rpcgen -c rpc/nfs.x >> $@

libnfs-raw-nfs.o: libnfs-raw-nfs.c rpc/nfs.h
	@echo Compiling $@
	gcc -g -c libnfs-raw-nfs.c -o $@

rpc/nfsacl.h: rpc/nfsacl.x
	@echo Generating $@
	$(LICENSE_LINE) && rpcgen -h rpc/nfsacl.x >> $@

libnfs-raw-nfsacl.c: rpc/nfsacl.x
	@echo Generating $@
	$(LICENSE_LINE) && rpcgen -c rpc/nfsacl.x >> $@

libnfs-raw-nfsacl.o: libnfs-raw-nfsacl.c rpc/nfsacl.h
	@echo Compiling $@
	gcc -g -c libnfs-raw-nfsacl.c -o $@

rpc/portmap.h: rpc/portmap.x
	@echo Generating $@
	$(LICENSE_LINE) && rpcgen -h rpc/portmap.x >> $@

libnfs-raw-portmap.c: rpc/portmap.x
	@echo Generating $@
	$(LICENSE_LINE) && rpcgen -c rpc/portmap.x >> $@

libnfs-raw-portmap.o: libnfs-raw-portmap.c rpc/portmap.h
	@echo Compiling $@
	gcc -g -c libnfs-raw-portmap.c -o $@

clean:
	rm -f *.o
	rm -f *.a
	rm -f rpc/mount.h libnfs-raw-mount.c
	rm -f rpc/nfs.h libnfs-raw-nfs.c
	rm -f rpc/nfsacl.h libnfs-raw-nfsacl.c
	rm -f rpc/portmap.h libnfs-raw-portmap.c
	rm -f tools/nfsclient-raw tools/nfsclient-async tools/nfsclient-sync


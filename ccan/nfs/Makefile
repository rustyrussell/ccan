CC=gcc
CFLAGS=-g -O0 -Wall -W -I. "-D_U_=__attribute__((unused))" -D_FILE_OFFSET_BITS=64
LIBS=

LIBNFS_OBJ = libnfs-raw-mount.o libnfs-raw-portmap.o libnfs-raw-nfs.o libnfs-raw-nfsacl.o mount.o nfs.o nfsacl.o portmap.o pdu.o init.o socket.o libnfs.o libnfs-sync.o

all: nfsclient-raw nfsclient-async nfsclient-sync

nfsclient-async: nfsclient-async.c libnfs.a
	$(CC) -o $@ nfsclient-async.c libnfs.a $(LIBS)

nfsclient-sync: nfsclient-sync.c libnfs.a
	$(CC) -o $@ nfsclient-sync.c libnfs.a $(LIBS)

nfsclient-raw: nfsclient-raw.c libnfs.a
	$(CC) -o $@ nfsclient-raw.c libnfs.a $(LIBS)

libnfs.a: $(LIBNFS_OBJ)
	@echo Creating library $@
	ar r libnfs.a $(LIBNFS_OBJ) 
	ranlib libnfs.a

libnfs-raw-mount.h: mount.x
	@echo Generating $@
	rpcgen -h mount.x > libnfs-raw-mount.h

libnfs-raw-mount.c: mount.x
	@echo Generating $@
	rpcgen -c mount.x | sed -e "s/#include \"mount.h\"/#include \"libnfs-raw-mount.h\"/" > libnfs-raw-mount.c

libnfs-raw-mount.o: libnfs-raw-mount.c libnfs-raw-mount.h
	@echo Compiling $@
	gcc -g -c libnfs-raw-mount.c -o $@

libnfs-raw-nfs.h: nfs.x
	@echo Generating $@
	rpcgen -h nfs.x > libnfs-raw-nfs.h

libnfs-raw-nfs.c: nfs.x
	@echo Generating $@
	rpcgen -c nfs.x | sed -e "s/#include \"nfs.h\"/#include \"libnfs-raw-nfs.h\"/" > libnfs-raw-nfs.c

libnfs-raw-nfs.o: libnfs-raw-nfs.c libnfs-raw-nfs.h
	@echo Compiling $@
	gcc -g -c libnfs-raw-nfs.c -o $@

libnfs-raw-nfsacl.h: nfsacl.x
	@echo Generating $@
	rpcgen -h nfsacl.x > libnfs-raw-nfsacl.h

libnfs-raw-nfsacl.c: nfsacl.x
	@echo Generating $@
	rpcgen -c nfsacl.x | sed -e "s/#include \"nfsacl.h\"/#include \"libnfs-raw-nfsacl.h\"/" > libnfs-raw-nfsacl.c

libnfs-raw-nfsacl.o: libnfs-raw-nfsacl.c libnfs-raw-nfsacl.h
	@echo Compiling $@
	gcc -g -c libnfs-raw-nfsacl.c -o $@

libnfs-raw-portmap.h: portmap.x
	@echo Generating $@
	rpcgen -h portmap.x > libnfs-raw-portmap.h

libnfs-raw-portmap.c: portmap.x
	@echo Generating $@
	rpcgen -c portmap.x | sed -e "s/#include \"portmap.h\"/#include \"libnfs-raw-portmap.h\"/" > libnfs-raw-portmap.c

libnfs-raw-portmap.o: libnfs-raw-portmap.c libnfs-raw-portmap.h
	@echo Compiling $@
	gcc -g -c libnfs-raw-portmap.c -o $@

clean:
	rm -f *.o
	rm -f *.a
	rm -f libnfs-raw-mount.h libnfs-raw-mount.c
	rm -f libnfs-raw-nfs.h libnfs-raw-nfs.c
	rm -f libnfs-raw-nfsacl.h libnfs-raw-nfsacl.c
	rm -f libnfs-raw-portmap.h libnfs-raw-portmap.c
	rm -f nfsclient-raw nfsclient-async nfsclient-sync


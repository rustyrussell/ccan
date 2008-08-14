CFLAGS=-g -Wall -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -I../../..

all: find_md5 md5_worker dns_lookup

find_md5: md5_server.c ../../../libccan.a
	$(CC) $(CFLAGS) -o $@ $^ 

md5_worker: md5_worker.c ../../../libccan.a
	$(CC) $(CFLAGS) -o $@ $^ 

dns_lookup: dns_lookup.c ../../../libccan.a
	$(CC) $(CFLAGS) -o $@ $^ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/**
 ** grok -- program to a alter bytes at an offset
 **
 ** Sometimes it useful to alter bytes in a binary file.
 ** To those who would 'just use emacs', the binary is not always a file on disk.  
 **
 ** codedr@gmail.com
 **
 ** License: Public Domain
 */

int
main(int argc, char **argv)
{
    int i, dc = 0, fd;
    int sz, cnt, offset;
    char *fn;
    unsigned char *buf, *data;
    /* grok fn offset data */

    if (3 > argc) {
	fputs("usage: grok fn offset [cdata]+\n", stderr);
	exit(1);
    }
    fn = argv[1];
    offset = strtol(argv[2], NULL, 16);
    data = (unsigned char *) calloc(argc - 2, sizeof(unsigned char));
    for (i = 3; i < argc; i++) {
	long x;
	x = strtol(argv[i], NULL, 16);
	data[dc++] = 0xff & x;
    }

    if (0 > (fd = open(fn, O_RDWR))) {
	fprintf(stderr, "cannot open %s, errno %d\n", fn, errno);
	exit(2);
    }

    if (0 > lseek(fd, offset, SEEK_SET)) {
	fprintf(stderr, "cannot seek to %x, errno %d\n", offset, errno);
	exit(2);
    }

    buf = (unsigned char *) calloc(dc, sizeof(unsigned char));

    if (0 > (cnt = read(fd, buf, dc))) {
	fprintf(stderr, "cannot read %d bytes at %x, errno %d\n", 
	    dc, offset, errno);
	exit(3);
    }
    printf("read %d of %d bytes\n", cnt, dc);
    /* dumpBuf(buf, cnt); */

    if (0 > lseek(fd, offset, SEEK_SET)) {
	fprintf(stderr, "cannot seek to %x, errno %d\n", offset, errno);
	exit(2);
    }

    if (0 > (sz = write(fd, data, cnt))) {
	fprintf(stderr, "cannot write %d bytes at %x, errno %d\n", 
	    cnt, offset, errno);
	exit(4);
    }
    printf("wrote %d of %d bytes\n", sz, cnt);
    exit(0);
}

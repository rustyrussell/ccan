#include "ntdb.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <ccan/err/err.h>

int main(int argc, char *argv[])
{
	unsigned int i, num_recs;
	struct ntdb_context *ntdb;

	if (argc != 3 || (num_recs = atoi(argv[2])) == 0)
		errx(1, "Usage: mktdb <tdbfile> <numrecords>");

	ntdb = ntdb_open(argv[1], NTDB_DEFAULT, O_CREAT|O_TRUNC|O_RDWR, 0600,NULL);
	if (!ntdb)
		err(1, "Opening %s", argv[1]);

	for (i = 0; i < num_recs; i++) {
		NTDB_DATA d;

		d.dptr = (void *)&i;
		d.dsize = sizeof(i);
		if (ntdb_store(ntdb, d, d, NTDB_INSERT) != 0)
			err(1, "Failed to store record %i", i);
	}
	printf("Done\n");
	return 0;
}

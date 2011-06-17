#include "tdb2.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <err.h>

int main(int argc, char *argv[])
{
	unsigned int i, num_recs;
	struct tdb_context *tdb;

	if (argc != 3 || (num_recs = atoi(argv[2])) == 0)
		errx(1, "Usage: mktdb <tdbfile> <numrecords>");

	tdb = tdb_open(argv[1], TDB_DEFAULT, O_CREAT|O_TRUNC|O_RDWR, 0600,NULL);
	if (!tdb)
		err(1, "Opening %s", argv[1]);

	for (i = 0; i < num_recs; i++) {
		TDB_DATA d;

		d.dptr = (void *)&i;
		d.dsize = sizeof(i);
		if (tdb_store(tdb, d, d, TDB_INSERT) != 0)
			err(1, "Failed to store record %i", i);
	}
	printf("Done\n");
	return 0;
}

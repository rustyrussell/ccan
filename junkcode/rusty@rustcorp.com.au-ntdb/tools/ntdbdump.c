/*
   simple ntdb dump util
   Copyright (C) Andrew Tridgell              2001
   Copyright (C) Rusty Russell                2011

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "config.h"
#include "ntdb.h"
#include "private.h"

static void print_data(NTDB_DATA d)
{
	unsigned char *p = (unsigned char *)d.dptr;
	int len = d.dsize;
	while (len--) {
		if (isprint(*p) && !strchr("\"\\", *p)) {
			fputc(*p, stdout);
		} else {
			printf("\\%02X", *p);
		}
		p++;
	}
}

static int traverse_fn(struct ntdb_context *ntdb, NTDB_DATA key, NTDB_DATA dbuf, void *state)
{
	printf("{\n");
	printf("key(%d) = \"", (int)key.dsize);
	print_data(key);
	printf("\"\n");
	printf("data(%d) = \"", (int)dbuf.dsize);
	print_data(dbuf);
	printf("\"\n");
	printf("}\n");
	return 0;
}

static int dump_ntdb(const char *fname, const char *keyname)
{
	struct ntdb_context *ntdb;
	NTDB_DATA key, value;

	ntdb = ntdb_open(fname, 0, O_RDONLY, 0, NULL);
	if (!ntdb) {
		printf("Failed to open %s\n", fname);
		return 1;
	}

	if (!keyname) {
		ntdb_traverse(ntdb, traverse_fn, NULL);
	} else {
		key = ntdb_mkdata(keyname, strlen(keyname));
		if (ntdb_fetch(ntdb, key, &value) != 0) {
			return 1;
		} else {
			print_data(value);
			free(value.dptr);
		}
	}

	return 0;
}

static void usage( void)
{
	printf( "Usage: ntdbdump [options] <filename>\n\n");
	printf( "   -h          this help message\n");
	printf( "   -k keyname  dumps value of keyname\n");
}

 int main(int argc, char *argv[])
{
	char *fname, *keyname=NULL;
	int c;

	if (argc < 2) {
		printf("Usage: ntdbdump <fname>\n");
		exit(1);
	}

	while ((c = getopt( argc, argv, "hk:")) != -1) {
		switch (c) {
		case 'h':
			usage();
			exit( 0);
		case 'k':
			keyname = optarg;
			break;
		default:
			usage();
			exit( 1);
		}
	}

	fname = argv[optind];

	return dump_ntdb(fname, keyname);
}

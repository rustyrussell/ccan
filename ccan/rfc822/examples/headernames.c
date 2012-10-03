#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <errno.h>
#include <unistd.h>

#include <ccan/talloc/talloc.h>
#include <ccan/grab_file/grab_file.h>
#include <ccan/rfc822/rfc822.h>

static void process_file(const char *name)
{
	void *ctx = talloc_new(NULL);
	size_t size;
	void *buf;
	struct rfc822_msg *msg;
	struct rfc822_header *hdr;

	buf = grab_file(ctx, name, &size);

	msg = rfc822_start(ctx, buf, size);

	rfc822_for_each_header(msg, hdr) {
		struct bytestring hname = rfc822_header_raw_name(msg, hdr);

		printf("%.*s\n", hname.len, hname.ptr);
	}

	talloc_free(ctx);
}

int main(int argc, char *argv[])
{
	int i;

	for (i = 0; i < (argc - 1); i++)
		process_file(argv[i + 1]);

	exit(0);
}

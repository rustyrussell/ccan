/* Tries to find data with a given MD5 (up to N bits). */
#include "ccan/antithread/antithread.h"
#include "ccan/string/string.h"
#include "ccan/talloc/talloc.h"
#include "md5_finder.h"
#include <err.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>

static void usage(void)
{
	errx(1, "Usage: md5calc <hexstring> <numcpus>");
}

static void parse_hexstring(const char *string, struct md5_search *md5s)
{
	unsigned int i;

	if (strstarts(string, "0x") || strstarts(string, "0X"))
		string += 2;

	for (i = 0; i < MD5_HASH_WORDS; i++) {
		unsigned int n[4], j;
		int ret;

		ret = sscanf(string, "%02x%02x%02x%02x",
			     &n[0], &n[1], &n[2], &n[3]);
		string += 8;

		if (ret == EOF)
			break;
		for (j = 0; j < ret; j++) {
			md5s->mask[MD5_HASH_WORDS-i-1] |= (0xFF << (8*j));
			md5s->md5[MD5_HASH_WORDS-i-1] |= (n[j] << (8*j));
		}

		if (ret != 4)
			break;
	}
}

static void init_pattern(u8 *pattern, unsigned int num_bytes, u64 total)
{
	unsigned int i;

	for (i = 0; i < num_bytes; i++) {
		pattern[i] = 'A' + (total % 26);
		total /= 26;
	}
}

#define PATTERN_BYTES 32

int main(int argc, char *argv[])
{
	struct at_pool *atp;
	struct md5_search md5s;
	unsigned int i, maxfd, numathreads = argc == 3 ? atoi(argv[2]) : 0;
	u64 total = 0;
	fd_set fds;
	char *cmdline[] = { "./md5_worker", NULL };
	struct athread *at[numathreads];

	if (numathreads == 0)
		usage();

	memset(&md5s, 0, sizeof(md5s));
	parse_hexstring(argv[1], &md5s);

	md5s.num_tries = 1024*1024;
	md5s.num_bytes = PATTERN_BYTES;

	/* *2 to allow for allocation inefficiency. */
	atp = at_pool((sizeof(md5s) + PATTERN_BYTES) * (numathreads + 1) * 2);
	if (!atp)
		err(1, "Can't create pool");

	/* Free pool on exit. */
//	talloc_steal(talloc_autofree_context(), atp);

	FD_ZERO(&fds);
	maxfd = 0;
	for (i = 0; i < numathreads; i++) {
		at[i] = at_spawn(atp, NULL, cmdline);
		if (!at[i])
			err(1, "Can't spawn child");
		FD_SET(at_fd(at[i]), &fds);
		if (at_fd(at[i]) > maxfd)
			maxfd = at_fd(at[i]);
	}

	for (;;) {
		struct md5_search *m, *res;
		fd_set in = fds;

		/* Shouldn't fail! */
		m = talloc(at_pool_ctx(atp), struct md5_search);
		*m = md5s;
		md5s.num_tries++;
		m->pattern = talloc_array(m, u8, m->num_bytes);
		init_pattern(m->pattern, m->num_bytes, total);

		select(maxfd+1, &in, NULL, NULL, NULL);
		for (i = 0; i < numathreads; i++)
			if (FD_ISSET(at_fd(at[i]), &in))
				break;
		if (i == numathreads)
			errx(1, "Select returned, but noone ready?");

		res = at_read(at[i]);
		if (res == NULL) {
			warn("Thread died?");
			FD_CLR(at_fd(at[i]), &fds);
			continue;
		}
		if (res != INITIAL_POINTER) {
			if (res->success) {
				printf("Success! '%.*s'\n",
				       res->num_bytes, (char *)res->pattern);
				exit(0);
			}
			m->num_tries++;
			talloc_free(res);
		}
		at_tell(at[i], m);
		total += m->num_tries;
	}
}		

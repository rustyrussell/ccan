#include "config.h"
#include "../ntdb.h"
#include "../private.h"
#include "tap-interface.h"
#include <ccan/hash/hash.h>
#include <assert.h>

#include "logging.h"
#include "helpapi-external-agent.h"

static const struct ntdb_context *curr_ntdb;
static const struct ntdb_file *curr_file;

static int owner_null_count,
	owner_weird_count, alloc_count, free_count, expand_count;

static void *test_alloc(const void *owner, size_t len, void *priv_data)
{
	void *ret;

	if (!owner) {
		owner_null_count++;
	} else if (owner != curr_ntdb && owner != curr_file) {
		owner_weird_count++;
	}

	alloc_count++;
	ret = malloc(len);

	/* The first time, this is the current ntdb, next is
	 * for the file struct. */
	if (!owner) {
		if (!curr_ntdb) {
			curr_ntdb = ret;
		} else if (!curr_file) {
			curr_file = ret;
		}
	}
	assert(priv_data == &owner_weird_count);
	return ret;
}

static void *test_expand(void *old, size_t newlen, void *priv_data)
{
	expand_count++;

	assert(priv_data == &owner_weird_count);
	return realloc(old, newlen);
}

static void test_free(void *old, void *priv_data)
{
	assert(priv_data == &owner_weird_count);
	if (old) {
		free_count++;
	}
	free(old);
}

int main(int argc, char *argv[])
{
	unsigned int i, j;
	union ntdb_attribute alloc_attr;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };
	NTDB_DATA key = { (unsigned char *)&j, sizeof(j) };
	NTDB_DATA data = { (unsigned char *)&j, sizeof(j) };

	alloc_attr.base.next = &tap_log_attr;
	alloc_attr.base.attr = NTDB_ATTRIBUTE_ALLOCATOR;

	alloc_attr.alloc.alloc = test_alloc;
	alloc_attr.alloc.expand = test_expand;
	alloc_attr.alloc.free = test_free;
	alloc_attr.alloc.priv_data = &owner_weird_count;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * (1 + 700 * 3 + 4) + 1);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		curr_ntdb = NULL;
		curr_file = NULL;
		ntdb = ntdb_open("run-20-alloc-attr.ntdb", flags[i]|MAYBE_NOSYNC,
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &alloc_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		for (j = 0; j < 700; j++) {
			NTDB_DATA d = { NULL, 0 }; /* Bogus GCC warning */
			ok1(ntdb_store(ntdb, key, data, NTDB_REPLACE) == 0);
			ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
			ok1(ntdb_deq(d, data));
			test_free(d.dptr, &owner_weird_count);
		}
		ntdb_close(ntdb);

		ok1(owner_null_count == 2+i*2);
		ok1(owner_weird_count == 0);
		ok1(alloc_count == free_count);
		ok1(expand_count != 0);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}

/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#include <ccan/jset/jset.h>
#include <ccan/build_assert/build_assert.h>
#include <stdlib.h>
#include <string.h>

struct jset *jset_new_(size_t size)
{
	struct jset *set;

	/* Judy uses Word_t, we use unsigned long directly. */
	BUILD_ASSERT(sizeof(unsigned long) == sizeof(Word_t));
	/* We pack pointers into jset (in jset_type.h) */
	BUILD_ASSERT(sizeof(Word_t) >= sizeof(void *));

	assert(size >= sizeof(*set));
	set = malloc(size);
	if (set) {
		set->judy = NULL;
		memset(&set->err, 0, sizeof(set->err));
		set->errstr = NULL;
	}
	return set;
}

const char *jset_error_str_(struct jset *set)
{
	char *str;
	free((char *)set->errstr);
	set->errstr = str = malloc(100);
	if (!set->errstr)
		return "out of memory";

	sprintf(str,
		"JU_ERRNO_* == %d, ID == %d\n",
		JU_ERRNO(&set->err), JU_ERRID(&set->err));
	return str;
}

void jset_free_(const struct jset *set)
{
	free((char *)set->errstr);
	Judy1FreeArray((PPvoid_t)&set->judy, PJE0);
	free((void *)set);
}

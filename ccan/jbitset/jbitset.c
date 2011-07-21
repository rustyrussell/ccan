/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#include <ccan/jbitset/jbitset.h>
#include <ccan/build_assert/build_assert.h>
#include <stdlib.h>
#include <string.h>

struct jbitset *jbit_new(void)
{
	struct jbitset *set;

	/* Judy uses Word_t, we use unsigned long directly. */
	BUILD_ASSERT(sizeof(unsigned long) == sizeof(Word_t));
	/* We pack pointers into jbitset (in jbitset_type.h) */
	BUILD_ASSERT(sizeof(Word_t) >= sizeof(void *));

	set = malloc(sizeof(*set));
	if (set) {
		set->judy = NULL;
		memset(&set->err, 0, sizeof(set->err));
		set->errstr = NULL;
	}
	return set;
}

const char *jbit_error_(struct jbitset *set)
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

void jbit_free(const struct jbitset *set)
{
	free((char *)set->errstr);
	Judy1FreeArray((PPvoid_t)&set->judy, PJE0);
	free((void *)set);
}

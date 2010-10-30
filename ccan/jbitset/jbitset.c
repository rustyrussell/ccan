#include <ccan/jbitset/jbitset.h>
#include <ccan/build_assert/build_assert.h>
#include <stdlib.h>
#include <string.h>

struct jbitset *jbit_new(void)
{
	struct jbitset *set;

	/* Judy uses Word_t, we use size_t. */
	BUILD_ASSERT(sizeof(size_t) == sizeof(Word_t));

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

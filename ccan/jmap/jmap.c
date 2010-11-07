#include <ccan/jmap/jmap.h>
#include <ccan/build_assert/build_assert.h>
#include <stdlib.h>
#include <string.h>

struct jmap *jmap_new(void)
{
	struct jmap *map;

	/* Judy uses unsigned long for Word_t, we use unsigned long. */
	BUILD_ASSERT(sizeof(Word_t) == sizeof(unsigned long));
	/* We also put pointers into Judy, in jmap_types.h */
	BUILD_ASSERT(sizeof(Word_t) >= sizeof(void *));

	map = malloc(sizeof(*map));
	if (map) {
		map->judy = NULL;
		memset(&map->err, 0, sizeof(map->err));
		map->errstr = NULL;
		map->num_accesses = 0;
		map->acc_value = NULL;
		map->acc_index = 0;
		map->funcname = NULL;
	}
	return map;
}

const char *jmap_error_(struct jmap *map)
{
	char *str;
	free((char *)map->errstr);
	map->errstr = str = malloc(100);
	if (!map->errstr)
		return "out of memory";

	sprintf(str,
		"JU_ERRNO_* == %d, ID == %d\n",
		JU_ERRNO(&map->err), JU_ERRID(&map->err));
	return str;
}

void jmap_free(const struct jmap *map)
{
	free((char *)map->errstr);
	Judy1FreeArray((PPvoid_t)&map->judy, PJE0);
	free((void *)map);
}

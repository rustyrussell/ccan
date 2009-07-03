#include "dict.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

//compare dict_entries by first letter ascending, then by length descending
static int compar_dict_entry(const void *ap, const void *bp) {
	const struct dict_entry *a=ap, *b=bp;
	unsigned int first_a = (unsigned int)a->str[0];
	unsigned int first_b = (unsigned int)b->str[0];
	if (first_a < first_b)
		return -1;
	else if (first_a > first_b)
		return 1;
	else {
		size_t len_a = strlen(a->str);
		size_t len_b = strlen(b->str);
		if (len_a > len_b)
			return -1;
		else if (len_a < len_b)
			return 1;
		else
			return 0;
	}
}

struct dict *dict_build(void *ctx, const struct dict_entry *entries, size_t count) {
	struct dict *dict = talloc_zero(ctx, struct dict);
	struct dict_entry *ent;
	int i;
	
	if (!count)
		return dict;
	
	ent = talloc_array(dict, struct dict_entry, count);
	memcpy(ent, entries, count*sizeof(struct dict_entry));
	qsort(ent, count, sizeof(*ent), compar_dict_entry);
	
	if (ent->str[0]==0) {
		dict->zero = ent;
		ent++, count--;
		
		if (count && ent->str[0]==0) {
			fprintf(stderr, "dict_entry array contains multiple empty strings\n");
			exit(EXIT_FAILURE);
		}
	}
	
	for (i=1; i<256; i++) {
		if (!count)
			break;
		if (ent->str[0] == (char)i)
			dict->by_first_letter[i-1] = ent;
		while (count && ent->str[0] == (char)i)
			ent++, count--;
	}
	
	return dict;
}

struct dict_entry *dict_lookup(struct dict *dict, const char **sp, const char *e) {
	struct dict_entry *de;
	unsigned int first;
	if (*sp >= e)
		return NULL;
	first = (unsigned int)**sp & 0xFF;
	
	if (!first) {
		if (dict->zero)
			(*sp)++;
		return dict->zero;
	}
	
	de = dict->by_first_letter[first-1];
	if (!de)
		return NULL;
	
	for (;de->str[0]==(char)first; de++) {
		const char *s = *sp;
		const char *ds = de->str;
		for (;;s++,ds++) {
			if (!*ds) {
				*sp = s;
				return de;
			}
			if (s>=e || *s!=*ds)
				break;
		}
	}
	
	return NULL;
}

#ifndef CCAN_TOKENIZER_DICT_H
#define CCAN_TOKENIZER_DICT_H

#include <stdint.h>
#include <ccan/talloc/talloc.h>
	//needed for freeing the struct dict*

struct dict_entry {
	int id;
	const char *str;
};

struct dict {
	struct dict_entry *zero;
	struct dict_entry *by_first_letter[256];
};

struct dict *dict_build(void *ctx, const struct dict_entry *entries, size_t count);
struct dict_entry *dict_lookup(struct dict *dict, const char **sp, const char *e);

#endif

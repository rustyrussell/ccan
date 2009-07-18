#include "stringmap/stringmap.h"
#include "stringmap/stringmap.c"

#include "tap/tap.h"

static void test_trivial(void) {
	stringmap(int) map = stringmap_new(NULL);
	
	ok1(stringmap_lookup(map, "") == NULL);
	*stringmap_enter(map, "") = -1;
	
	ok1(stringmap_lookup(map, "0") == NULL);
	*stringmap_enter(map, "0") = 0;
	
	ok1(stringmap_lookup(map, "one") == NULL);
	*stringmap_enter(map, "one") = 1;
	
	ok1(stringmap_lookup(map, "two") == NULL);
	*stringmap_enter(map, "two") = 2;
	
	ok1(stringmap_lookup(map, "three") == NULL);
	*stringmap_enter(map, "three") = 3;
	
	ok1(stringmap_lookup(map, "four") == NULL);
	*stringmap_enter(map, "four") = 4;
	
	ok1(*stringmap_lookup(map, "three") == 3);
	ok1(*stringmap_lookup(map, "one") == 1);
	ok1(*stringmap_lookup(map, "four") == 4);
	ok1(*stringmap_lookup(map, "two") == 2);
	ok1(*stringmap_lookup(map, "") == -1);
	ok1(*stringmap_lookup(map, "0") == 0);
	
	ok1(map.t.count == 6);
	
	stringmap_free(map);
}


static void scramble(void *base, size_t nmemb, size_t size) {
   char *i = base;
   char *o;
   size_t sd;
   for (;nmemb>1;nmemb--) {
      o = i + size*(random()%nmemb);
      for (sd=size;sd--;) {
         char tmp = *o;
         *o++ = *i;
         *i++ = tmp;
      }
   }
}

//#define RANDOM_STRING_READABLE

static char *random_string(struct block_pool *bp, size_t *len_out) {
	#ifndef RANDOM_STRING_READABLE
	size_t len = random()%5 ? random()%10 : random()%1000;
	#else
	size_t len = random() % 10;
	#endif
	char *str = block_pool_alloc(bp, len);
	char *i;
	
	*len_out = len;
	
	for (i=str; len--; i++) {
		#ifndef RANDOM_STRING_READABLE
		char c = random();
		*i = c;
		#else
		//only generate characters a-z
		char c = random()%26 + 'a';
		*i = c;
		#endif
	}
	
	return str;
}

struct test_entry {
	//note: struct layout needs to match *stringmap(char*).last
	const char *str;
	size_t len;
	
	char *value;
		/* value is not a string, but a pointer to char marking that
		   this key has been entered already. */
};

static int tecmp(const struct test_entry *a, const struct test_entry *b) {
	if (a->len < b->len)
		return -1;
	else if (a->len > b->len)
		return 1;
	else
		return memcmp(a->str, b->str, a->len);
}

static int by_str(const void *a, const void *b) {
	return tecmp(a, b);
}

static void cull_duplicates(struct test_entry *entries, size_t *count) {
	struct test_entry *i, *o, *e = entries + *count;
	
	qsort(entries, *count, sizeof(*entries), by_str);
	
	for (i=entries, o=entries; i<e;) {
		//skip repeated strings
		if (o>entries) {
			struct test_entry *last = &o[-1];
			if (!tecmp(last, i)) {
				do i++; while(i<e && !tecmp(last, i));
				continue;
			}
		}
		
		//write all entries with the same value (should also have same string)
		{
			char *value = i->value;
			do *o++ = *i++; while(i<e && i->value == value);
		}
	}
	
	*count = o-entries;
}

static int test_stringmap(size_t count, FILE *out) {
	stringmap(char*) map = stringmap_new(NULL);
	
	#define print(tag, fmt, ...) do { \
			if (out) \
				fprintf(out, tag fmt "\n", ##__VA_ARGS__); \
		} while(0)
	#define err(...) do { \
			print("error: ", __VA_ARGS__); \
			goto fail; \
		} while(0)
	//#define debug(...) print("debug: ", __VA_ARGS__)
	#define debug(...) do {} while(0)
	#define msg(...) print("info: ", __VA_ARGS__)
	
	struct block_pool *bp = block_pool_new(NULL);
	struct test_entry *entries = block_pool_alloc(bp, sizeof(*entries) * count);
	struct test_entry *i, *e = entries+count;
	char *value_base = block_pool_alloc(bp, count), *value = value_base;
	size_t unique_count = 0;
	
	//we use value to track whether an entry has been added or not
	memset(value, 0, count);
	
	msg("Generating %zu test entries...", count);
	
	for (i=entries; i<e; value++) {
		size_t len;
		char *str = random_string(bp, &len);
		size_t same_count = (random()%5 ? random()%3 : random()%10) + 1;
		
		for (;same_count-- && i<e; i++) {
			i->str = str;
			i->len = len;
			i->value = value;
		}
	}
	
	cull_duplicates(entries, &count);
	e = entries+count;
	scramble(entries, count, sizeof(*entries));
	
	msg("Inserting/looking up %zu entries...", count);
	
	for (i=entries; i<e; i++) {
		char **node;
		
		debug("Looking up %s", i->str);
		
		node = stringmap_lookup_n(map, i->str, i->len);
		
		if (!node) {
			if (*i->value)
				err("Previously inserted entry not found");
			
			debug("Not found; entering %s", i->str);
			
			node = stringmap_enter_n(map, i->str, i->len);
			if (!node || tecmp(i, (void*)map.last))
				err("Node not properly entered");
			if (map.last->str[map.last->len])
				err("Entered string not zero-terminated");
			*node = i->value;
			*i->value = 1; //mark that the entry is entered
			
			unique_count++;
		} else {
			if (tecmp(i, (void*)map.last))
				err("lookup returned incorrect string");
			if (map.last->str[map.last->len])
				err("Looked-up string not zero-terminated");
			if (i->value != *node)
				err("lookup returned incorrect value");
			if (!*i->value)
				err("lookup returned bogus value");
		}
	}
	
	if (map.t.count != unique_count)
		err("Map has incorrect count");
	
	printf("stringmap test passed after %zu inserts, %zu lookups (%zu total operations)\n",
		unique_count, (i-entries)-unique_count, i-entries);
	
	block_pool_free(bp);
	stringmap_free(map);
	return 1;

fail:
	printf("stringmap test failed after %zu inserts, %zu lookups (%zu total operations)\n",
		unique_count, (i-entries)-unique_count, i-entries);
	
	block_pool_free(bp);
	stringmap_free(map);
	return 0;
	
	#undef print
	#undef err
	#undef debug
	#undef msg
}

int main(void)
{
	plan_tests(14);
	
	test_trivial();
	ok1(test_stringmap(10000, stdout));
	
	return exit_status();
}

#include "stringmap/stringmap.h"
#include "stringmap/stringmap.c"

#include "tap/tap.h"

static void test_trivial(void) {
	stringmap(int) map = stringmap_new(NULL);
	
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
	
	ok1(map.t.count == 4);
	
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

static char *random_string(struct block_pool *bp) {
	size_t len = random() % 100;
	char *str = block_pool_alloc(bp, len+1);
	char *i;
	
	for (i=str; len--; i++) {
		#ifndef RANDOM_STRING_READABLE
		char c = random();
		*i = c ? c : ' ';
		#else
		//only generate characters [32,126]
		char c = random()%95 + 32;
		*i = c;
		#endif
	}
	*i = 0;
	
	return str;
}

struct test_entry {
	const char *str;
	char *value;
		/* value is not a string, but a pointer to char marking that
		   this key has been entered already. */
};

static int by_str(const void *ap, const void *bp) {
	return strcmp(((struct test_entry*)ap)->str, ((struct test_entry*)bp)->str);
}

static void cull_duplicates(struct test_entry *entries, size_t *count) {
	struct test_entry *i, *o, *e = entries + *count;
	
	qsort(entries, *count, sizeof(*entries), by_str);
	
	for (i=entries, o=entries; i<e;) {
		//skip repeated strings
		if (i>entries) {
			const char *last = i[-1].str;
			if (!strcmp(last, i->str)) {
				do i++; while(i<e && !strcmp(last, i->str));
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
	#define debug(...) print("debug: ", __VA_ARGS__)
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
		char *str = random_string(bp);
		size_t same_count = (random()%5 ? random()%3 : random()%10) + 1;
		
		for (;same_count-- && i<e; i++) {
			i->str = str;
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
		
		node = stringmap_lookup(map, i->str);
		
		if (!node) {
			if (*i->value)
				err("Previously inserted entry not found");
			
			debug("Not found; entering");
			
			node = stringmap_enter(map, i->str);
			if (!node || strcmp(i->str, map.last->str))
				err("Node not properly entered");
			*node = i->value;
			*i->value = 1; //mark that the entry is entered
			
			unique_count++;
		} else {
			if (strcmp(i->str, map.last->str))
				err("lookup returned incorrect string");
			if (i->value != *node)
				err("lookup returned incorrect value");
			if (!*i->value)
				err("lookup returned bogus value");
		}
	}
	
	if (map.t.count != unique_count)
		err("Map has incorrect count");
	
	printf("stringmap test passed after %zu inserts, %zu lookups (%zu total operations)\n", unique_count, (i-entries)-unique_count, i-entries);
	
	block_pool_free(bp);
	stringmap_free(map);
	return 1;

fail:
	printf("stringmap test failed after %zu inserts, %zu lookups (%zu total operations)\n", unique_count, (i-entries)-unique_count, i-entries);
	
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
	plan_tests(10);
	
	test_trivial();
	ok1(test_stringmap(10000, NULL));
	
	return exit_status();
}

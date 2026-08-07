#include <ccan/ciniparser/dictionary.h>
#include <ccan/ciniparser/dictionary.c>

#include <ccan/tap/tap.h>

static void test_trivial(void) {
	dictionary *map = dictionary_new(0);
	char *notfound = (char*)0xDEADBEEF;
	
	ok1(dictionary_get(map, "notfound", notfound) == notfound);
	
	ok1(dictionary_get(map, "one", NULL) == NULL);
	ok1(dictionary_set(map, "one", "1") == 0);
	
	ok1(dictionary_get(map, "two", NULL) == NULL);
	ok1(dictionary_set(map, "two", "2") == 0);
	
	ok1(dictionary_get(map, "three", NULL) == NULL);
	ok1(dictionary_set(map, "three", "3") == 0);
	
	ok1(dictionary_get(map, "four", NULL) == NULL);
	ok1(dictionary_set(map, "four", "4") == 0);
	
	ok1(!strcmp(dictionary_get(map, "three", NULL), "3"));
	ok1(!strcmp(dictionary_get(map, "one", NULL), "1"));
	ok1(!strcmp(dictionary_get(map, "four", NULL), "4"));
	ok1(!strcmp(dictionary_get(map, "two", NULL), "2"));
	
	ok1(map->n == 4);
	
	dictionary_del(map);
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

static char *random_string(void) {
	size_t len = random() % 100;
	char *str = malloc(len+1);
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
	char *str;
	char *value;
};

static int by_str(const void *ap, const void *bp) {
	return strcmp(((struct test_entry*)ap)->str, ((struct test_entry*)bp)->str);
}

static void cull_duplicates(struct test_entry *entries, size_t *count) {
	struct test_entry *i, *o, *e = entries + *count;
	
	qsort(entries, *count, sizeof(*entries), by_str);
	
	for (i=entries, o=entries; i<e;) {
		//skip repeated strings
		if (o>entries) {
			const char *last = o[-1].str;
			if (!strcmp(last, i->str)) {
				do {
					free(i->str);
					i++;
				} while(i<e && !strcmp(last, i->str));
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

static void free_test_entries(struct test_entry *entries, size_t count) {
	struct test_entry *i = entries;
	for (;count--;i++)
		free(i->str);
	free(entries);
}

static int test_dictionary(size_t count, FILE *out) {
	dictionary *map = dictionary_new(0);
	
	#define print(tag, fmt, ...) do { \
			if (out) \
				fprintf(out, tag fmt "\n", ##__VA_ARGS__); \
		} while(0)
	#define debug(...) print("debug: ", __VA_ARGS__)
	#define msg(...) print("info: ", __VA_ARGS__)
	
	struct test_entry *entries = malloc(sizeof(*entries) * count);
	struct test_entry *i, *e = entries+count;
	char *value_base = malloc(count), *value = value_base;
	size_t unique_count = 0;
	
	//we use value to track whether an entry has been added or not
	memset(value, 0, count);
	
	msg("Generating %zu test entries...", count);
	
	for (i=entries; i<e; value++) {
		char *str = random_string();
		size_t same_count = random()%5 ? random()%3 : random()%10;
		
		i->str = str;
		i->value = value;
		i++;
		
		for (;same_count-- && i<e; i++) {
			i->str = strdup(str);
			i->value = value;
		}
	}
	
	cull_duplicates(entries, &count);
	e = entries+count;
	scramble(entries, count, sizeof(*entries));
	
	#define err(...) do { \
			print("error: ", __VA_ARGS__); \
			goto fail; \
		} while(0)
	
	msg("Inserting/looking up %zu entries...", count);
	
	for (i=entries; i<e; i++) {
		char *value;
		
		debug("Looking up %s", i->str);
		
		value = dictionary_get(map, i->str, NULL);
		
		if (!value) {
			char value_str[64];
			
			if (*i->value)
				err("Previously inserted entry not found");
			
			debug("Not found; entering");
			
			/* Because dictionary only accepts strings, and we want
			   to store pointer indices, we'll write the indices as
			   decimal numbers :) */
			sprintf(value_str, "%lu", (unsigned long)(i->value - value_base));
			
			if (dictionary_set(map, i->str, value_str) != 0)
				err("dictionary_set had an error");
			
			*i->value = 1; //mark that the entry is entered
			
			unique_count++;
		} else {
			if ((unsigned long)(i->value - value_base) !=
					strtoul(value, NULL, 10))
				err("lookup returned incorrect value");
			if (!*i->value)
				err("lookup returned bogus value");
		}
	}
	
	if (map->n != unique_count)
		err("Dictionary has incorrect count");
	
	printf("dictionary test passed after %zu inserts, %zu lookups (%zu total operations)\n", unique_count, (i-entries)-unique_count, i-entries);
	
	free_test_entries(entries, e-entries);
	free(value_base);
	dictionary_del(map);
	return 1;

fail:
	printf("dictionary test failed after %zu inserts, %zu lookups (%zu total operations)\n", unique_count, (i-entries)-unique_count, i-entries);
	
	free_test_entries(entries, e-entries);
	free(value_base);
	dictionary_del(map);
	return 0;
	
	#undef print
	#undef err
	#undef debug
	#undef msg
}

int main(void)
{
	plan_tests(15);
	
	test_trivial();
	ok1(test_dictionary(10000, NULL));
	
	return exit_status();
}

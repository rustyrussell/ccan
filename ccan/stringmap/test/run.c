#include <ccan/stringmap/stringmap.h>
#include <ccan/stringmap/stringmap.c>

#include <ccan/tap/tap.h>

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

static int tecmp(const struct test_entry *ap, const struct test_entry *bp) {
	const unsigned char *a = (unsigned char*)ap->str, *ae = a+ap->len;
	const unsigned char *b = (unsigned char*)bp->str, *be = b+bp->len;
	
	for (;;a++, b++) {
		if (a >= ae) {
			if (b >= be)
				return 0; //strings are the same
			return -1; //a is shorter than b
		}
		if (b >= be)
			return 1; //b is shorter than a
		if (*a != *b) //there is a differing character
			return (unsigned int)*a - (unsigned int)*b;
	}
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

static int test_traverse(struct stringmap *map, struct test_entry *strings, size_t size, FILE *out) {
	size_t sp = 0;
	size_t stack_alloc = 16;
	struct stringmap_node **stack = NULL;
	struct stringmap_node *n;
	void *leaf;
	#define check(lri) do { \
			leaf = n->lr[lri]; \
			if (!size--) \
				err("Fewer items in tree than counted"); \
			if (tecmp(leaf, strings++)) \
				err("%s leaf has incorrect string", lri ? "Left" : "Right"); \
		} while(0)
	#define check_left() check(0)
	#define check_right() check(1)
	
	if (map->count != size)
		err("map->count != size");
	
	if (map->count == 0)
		return 1;
	
	if (map->count == 1) {
		leaf = (struct test_entry*)map->root;
		if (!tecmp(leaf, &strings[0]))
			return 1;
		else
			err("Only leaf in tree has incorrect value");
	}
	
	stack = malloc(sizeof(*stack) * stack_alloc);
	n = map->root;
	
	for (;;) {
		//descend left
		while (!n->left_is_leaf) {
			stack[sp++] = n;
			n = n->lr[0];
			
			if (sp >= stack_alloc) {
				stack_alloc += stack_alloc;
				stack = realloc(stack, sizeof(*stack) * stack_alloc);
			}
		}
		
		check_left();
		
	ascend_right:
		while (n->right_is_leaf) {
			check_right();
			
			if (!sp)
				goto done; //we finished up the last entry
			n = stack[--sp];
		}
		
		//descend right
		n = n->lr[1];
		while (n->left_is_leaf) {
			check_left();
			
			if (n->right_is_leaf) {
				check_right();
				
				if (!sp)
					goto done;
				n = stack[--sp];
				goto ascend_right; //sorry
			}
			n = n->lr[1];
		}
	}
	
done:
	if (size != 0)
		err("More items in tree than counted");
	
	free(stack);
	return 1;
	
fail:
	if (stack)
		free(stack);
	return 0;
	
	#undef check
	#undef check_left
	#undef check_right
}

static int test_stringmap(size_t count, FILE *out) {
	stringmap(char*) map = stringmap_new(NULL);
	
	struct block_pool *bp = block_pool_new(NULL);
	struct test_entry *entries = block_pool_alloc(bp, sizeof(*entries) * count);
	struct test_entry *i, *e = entries+count, *o;
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
	
	for (i=entries, o=entries; i<e; i++) {
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
			
			//write this unique entry to the entry list to traverse later
			*o++ = *i;
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
	
	unique_count = o-entries;
	
	if (map.t.count != unique_count)
		err("Map has incorrect count");
	
	qsort(entries, unique_count, sizeof(*entries), by_str);
	
	msg("Traversing forward through %zu items", unique_count);	
	if (!test_traverse(&map.t, entries, unique_count, out))
		err("Traversal does not produce the strings in order");
	
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
}

#undef print
#undef err
#undef debug
#undef msg

int main(void)
{
	plan_tests(14);
	
	test_trivial();
	ok1(test_stringmap(10000, stdout));
	
	return exit_status();
}

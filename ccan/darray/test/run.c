#include <ccan/tap/tap.h>
#include <ccan/darray/darray.h>
#include <stdio.h>

#include "lotsOfNumbers.h"
#include "lotsOfStrings.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*(arr)))

struct {
	char *stringsF, *stringsB;
		//items of lotsOfStrings glued together
	size_t stringsSize; //total strlen of all strings combined
} amalgams;

static void generateAmalgams(void);
static void freeAmalgams(void);
static int isZeros(void *ptr, size_t size);
static void memtile(void *dest, size_t destWidth, const void *src, size_t srcWidth);


#if 0
#define testing(...) printf("Testing %s...\n", #__VA_ARGS__)
#define trace(...) do {printf(__VA_ARGS__); puts("");} while(0)
#else
#define testing(...) do {} while(0)
#define trace(...) do {} while(0)
#endif

#include "testLits.h"

int main(void) {
	darray(long) arr = darray_new();
	darray_char str = darray_new();
	#define reset(arr) do {darray_free(arr); darray_init(arr);} while(0)
	size_t i;
	
	trace("Generating amalgams (internal)");
	generateAmalgams();
	
	plan_tests(47);
	
	testLits();
	
	testing(darray_push);
	{
		for (i=0; i < ARRAY_SIZE(lotsOfNumbers); i++)
			darray_push(arr, lotsOfNumbers[i]);
		ok1(darray_size(arr) == ARRAY_SIZE(lotsOfNumbers));
		ok1(darray_alloc(arr) >= darray_size(arr));
		ok1(!memcmp(arr.item, lotsOfNumbers, sizeof(lotsOfNumbers)));
	}
	reset(arr);
	
	testing(darray_prepend, darray_pop);
	{
		for (i = ARRAY_SIZE(lotsOfNumbers); i;)
			darray_prepend(arr, lotsOfNumbers[--i]);
		ok1(darray_size(arr) == ARRAY_SIZE(lotsOfNumbers));
		ok1(darray_alloc(arr) >= darray_size(arr));
		ok1(!memcmp(arr.item, lotsOfNumbers, sizeof(lotsOfNumbers)));
		
		for (i = ARRAY_SIZE(lotsOfNumbers); i;) {
			if (darray_pop(arr) != (long)lotsOfNumbers[--i]) {
				i++;
				break;
			}
		}
		ok1(i==0);
		ok1(darray_size(arr) == 0);
	}
	reset(arr);

	testing(darray_insert);
	{
		size_t middle_i;

		for (i=0; i < ARRAY_SIZE(lotsOfNumbers); i++)
			darray_insert(arr, i, lotsOfNumbers[i]);
		ok1(darray_size(arr) == ARRAY_SIZE(lotsOfNumbers));
		ok1(darray_alloc(arr) >= darray_size(arr));
		ok1(!memcmp(arr.item, lotsOfNumbers, sizeof(lotsOfNumbers)));

		middle_i = ARRAY_SIZE(lotsOfNumbers) / 2;
		darray_insert(arr, middle_i, 42);
		ok1(darray_size(arr) == ARRAY_SIZE(lotsOfNumbers) + 1);
		ok1(arr.item[middle_i] == 42);
		ok1(arr.item[middle_i + 1] == lotsOfNumbers[middle_i]);
	}
	reset(arr);

	testing(darray_from_c, darray_foreach, darray_foreach_reverse);
	{
		long   *i;
		size_t  j;
		
		darray_from_c(arr, lotsOfNumbers);
		ok1(darray_size(arr) == ARRAY_SIZE(lotsOfNumbers));
		ok1(darray_alloc(arr) >= darray_size(arr));
		ok1(memcmp(arr.item, lotsOfNumbers, sizeof(lotsOfNumbers)) == 0);
		
		j = 0;
		darray_foreach(i, arr) {
			if (i - arr.item != j)
				break;
			if (*i != (long)lotsOfNumbers[j])
				break;
			j++;
		};
		ok1(j == ARRAY_SIZE(lotsOfNumbers));
		
		j = 0;
		darray_foreach_reverse(i, arr) {
			if (i - arr.item != darray_size(arr)-j-1)
				break;
			if (*i != (long)lotsOfNumbers[darray_size(arr)-j-1])
				break;
			j++;
		};
		ok1(j == ARRAY_SIZE(lotsOfNumbers));
	}
	reset(arr);
	
	testing(darray_append_string);
	{
		for (i=0; i < ARRAY_SIZE(lotsOfStrings); i++)
			darray_append_string(str, lotsOfStrings[i]);
		ok1(str.size == amalgams.stringsSize);
		ok1(str.alloc > str.size);
		ok1(str.item[str.size] == 0);
		ok1(!strcmp(str.item, amalgams.stringsF));
	}
	reset(str);
	
	testing(darray_prepend_string);
	{
		for (i=0; i < ARRAY_SIZE(lotsOfStrings); i++)
			darray_prepend_string(str, lotsOfStrings[i]);
		ok1(str.size == amalgams.stringsSize);
		ok1(str.alloc > str.size);
		ok1(str.item[str.size] == 0);
		ok1(!strcmp(str.item, amalgams.stringsB));
	}
	reset(str);
	
	testing(darray_from_string);
	{
		for (i=0; i < ARRAY_SIZE(lotsOfStrings); i++) {
			darray_from_string(str, lotsOfStrings[i]);
			if (str.size != strlen(lotsOfStrings[i]))
				break;
			if (str.alloc < strlen(lotsOfStrings[i])+1)
				break;
			if (strcmp(str.item, lotsOfStrings[i]))
				break;
		}
		ok1(i == ARRAY_SIZE(lotsOfStrings));
	}
	reset(str);
	
	testing(darray_resize0);
	{
		size_t prevSize=0, size;
		for (i=0; i < ARRAY_SIZE(lotsOfNumbers); i++, prevSize=size) {
			size = lotsOfNumbers[i] & 0xFFFF;
			darray_resize0(arr, size);
			if (darray_size(arr) != size)
				break;
			if (darray_alloc(arr) < size)
				break;
			if (size>prevSize) {
				if (!isZeros(arr.item+prevSize, (size-prevSize)*sizeof(*arr.item)))
					break;
			}
			//fill the darray with lotsOfNumbers garbage
			memtile(arr.item, darray_size(arr)*sizeof(*arr.item), lotsOfNumbers, sizeof(lotsOfNumbers));
		}
		ok1(i == ARRAY_SIZE(lotsOfNumbers));
	}
	reset(arr);
	
	testing(darray_realloc);
	{
		size_t s,a;
		for (i=0; i < ARRAY_SIZE(lotsOfNumbers); i++) {
			arr.size = (s = lotsOfNumbers[i] >> 16);
				//give size a nonsense value to make sure darray_realloc doesn't care about it
			a = amalgams.stringsSize/sizeof(*arr.item)+2;
			darray_realloc(arr, a = lotsOfNumbers[i] % ((amalgams.stringsSize/sizeof(*arr.item))+1));
			if (a*sizeof(*arr.item) > amalgams.stringsSize)
				break;
			if (darray_alloc(arr) != a)
				break;
			if (darray_size(arr) != s)
				break;
			memtile(arr.item, a*sizeof(*arr.item), amalgams.stringsF, a*sizeof(*arr.item));
			if (memcmp(arr.item, amalgams.stringsF, a*sizeof(*arr.item)))
				break;
		}
		ok1(i == ARRAY_SIZE(lotsOfNumbers));
	}
	reset(arr);
	
	testing(darray_growalloc);
	{
		size_t prevA, s, a;
		for (i=0; i < ARRAY_SIZE(lotsOfNumbers); i++) {
			arr.size = (s = lotsOfNumbers[i] >> 16);
				//give size a nonsense value to make sure darray_growalloc doesn't care about it
			a = amalgams.stringsSize/sizeof(*arr.item)+2;
			prevA = darray_alloc(arr);
			darray_growalloc(arr, a = lotsOfNumbers[i] % ((amalgams.stringsSize/sizeof(*arr.item))+1));
			if (a*sizeof(*arr.item) > amalgams.stringsSize)
				break;
			if (darray_alloc(arr) < a)
				break;
			if (darray_alloc(arr) < prevA)
				break;
			if (darray_size(arr) != s)
				break;
			
			memtile(arr.item, a*sizeof(*arr.item), amalgams.stringsF, a*sizeof(*arr.item));
			if (memcmp(arr.item, amalgams.stringsF, a*sizeof(*arr.item)))
				break;
			
			//clear the darray every now and then
			if (!(lotsOfNumbers[i] & 15)) {
				reset(arr);
			}
		}
		ok1(i == ARRAY_SIZE(lotsOfNumbers));
	}
	reset(arr);
	
	testing(darray_make_room);
	{
		for (i=0; i < ARRAY_SIZE(lotsOfStrings); i++) {
			char *dest = darray_make_room(str, strlen(lotsOfStrings[i]));
			if (str.alloc < str.size+strlen(lotsOfStrings[i]))
				break;
			if (dest != str.item+str.size)
				break;
			
			memcpy(dest, lotsOfStrings[i], strlen(lotsOfStrings[i]));
			str.size += strlen(lotsOfStrings[i]);
		}
		ok1(i == ARRAY_SIZE(lotsOfStrings));
		ok1(str.size == amalgams.stringsSize);
		
		darray_append(str, 0);
		ok1(!strcmp(str.item, amalgams.stringsF));
	}
	reset(str);
	
	testing(darray_appends, darray_prepends, darray_pop_check);
	{
		darray(const char*) arr = darray_new();
		const char *n[9] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight"};

#if HAVE_TYPEOF		
		darray_appends(arr, n[5], n[6], n[7], n[8]);
#else
		darray_appends_t(arr, const char *, n[5], n[6], n[7], n[8]);
#endif
		ok1(darray_size(arr)==4 && darray_alloc(arr)>=4);

#if HAVE_TYPEOF		
		darray_prepends(arr, n[0], n[1], n[2], n[3], n[4]);
#else
		darray_prepends_t(arr, const char *, n[0], n[1], n[2], n[3], n[4]);
#endif

		ok1(darray_size(arr)==9 && darray_alloc(arr)>=9);
		
		ok1(arr.item[0]==n[0] &&
		    arr.item[1]==n[1] &&
		    arr.item[2]==n[2] &&
		    arr.item[3]==n[3] &&
		    arr.item[4]==n[4] &&
		    arr.item[5]==n[5] &&
		    arr.item[6]==n[6] &&
		    arr.item[7]==n[7] &&
		    arr.item[8]==n[8]);
		
		ok1(darray_pop_check(arr)==n[8] &&
		    darray_pop_check(arr)==n[7] &&
		    darray_pop_check(arr)==n[6] &&
		    darray_pop_check(arr)==n[5] &&
		    darray_pop_check(arr)==n[4] &&
		    darray_pop_check(arr)==n[3] &&
		    darray_pop_check(arr)==n[2] &&
		    darray_pop_check(arr)==n[1] &&
		    darray_pop_check(arr)==n[0]);
		
		ok1(darray_size(arr)==0);
		
		ok1(darray_pop_check(arr)==NULL && darray_pop_check(arr)==NULL && darray_pop_check(arr)==NULL);
		
		darray_free(arr);
	}
	
	trace("Freeing amalgams (internal)");
	freeAmalgams();
	
	return exit_status();
}

static void generateAmalgams(void) {
	size_t i;
	size_t lotsOfStringsLen = 0;
	const char *src;
	char *p;
	
	for (i=0; i < ARRAY_SIZE(lotsOfStrings); i++)
		lotsOfStringsLen += strlen(lotsOfStrings[i]);
	amalgams.stringsSize = lotsOfStringsLen;
	
	amalgams.stringsF = malloc(lotsOfStringsLen+1);
	amalgams.stringsB = malloc(lotsOfStringsLen+1);
	
	for (i=0,p=amalgams.stringsF; i < ARRAY_SIZE(lotsOfStrings); i++) {
		size_t len = strlen(src=lotsOfStrings[i]);
		memcpy(p, src, len);
		p += len;
	}
	*p = 0;
	ok1(p-amalgams.stringsF == (long)lotsOfStringsLen);
	ok1(strlen(amalgams.stringsF) == lotsOfStringsLen);
	
	for (i = ARRAY_SIZE(lotsOfStrings), p = amalgams.stringsB; i--;) {
		size_t len = strlen(src=lotsOfStrings[i]);
		memcpy(p, src, len);
		p += len;
	}
	*p = 0;
	ok1(p-amalgams.stringsB == (long)lotsOfStringsLen);
	ok1(strlen(amalgams.stringsB) == lotsOfStringsLen);
}

static void freeAmalgams(void) {
	free(amalgams.stringsF);
	free(amalgams.stringsB);
}

static int isZeros(void *ptr, size_t size) {
	unsigned char *pc = ptr;
	size_t *pl;
	if (size>8) {
		//test one byte at a time until we have an aligned size_t pointer
		while ((size_t)pc & (sizeof(size_t)-1))
			if (*pc++)
				return 0;
		pl = (size_t*)pc;
		size -= pc-(unsigned char*)ptr;
		while (size >= sizeof(size_t)) {
			size -= sizeof(size_t);
			if (*pl++)
				return 0;
		}
		pc = (unsigned char*)pl;
	}
	while (size--)
		if (*pc++)
			return 0;
	return 1;
}

static void memtile(void *dest, size_t destWidth, const void *src, size_t srcWidth) {
	char *d = dest;
	while (destWidth > srcWidth) {
		destWidth -= srcWidth;
		memcpy(d, src, srcWidth);
		d += srcWidth;
	}
	memcpy(d, src, destWidth);
}

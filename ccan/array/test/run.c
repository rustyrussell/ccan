#include <stdio.h>
#include <tap/tap.h>
#include "array/array.h"
#include "array/array.c"

#define countof(array...) (sizeof(array)/sizeof(*(array)))
#include "lotsOfNumbers.h"
#include "lotsOfStrings.h"

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
	#ifndef ARRAY_USE_TALLOC
	array(long) arr = array_new();
	array_char str = array_new();
	#define reset(array) do {array_free(array); array_init(array);} while(0)
	#else
	array(long) arr = array_new(NULL);
	array_char str = array_new(NULL);
	#define reset(array) do {array_free(array); array_init(array, NULL);} while(0)
	#endif
	size_t i;
	
	trace("Generating amalgams (internal)");
	generateAmalgams();
	
	plan_tests(41);
	
	testLits();
	
	testing(array_push);
	{
		for (i=0; i<countof(lotsOfNumbers); i++)
			array_push(arr, lotsOfNumbers[i]);
		ok1(arr.size == countof(lotsOfNumbers));
		ok1(arr.alloc >= arr.size);
		ok1(!memcmp(arr.item, lotsOfNumbers, sizeof(lotsOfNumbers)));
	}
	reset(arr);
	
	testing(array_prepend, array_pop_nocheck);
	{
		for (i=countof(lotsOfNumbers); i;)
			array_prepend(arr, lotsOfNumbers[--i]);
		ok1(arr.size == countof(lotsOfNumbers));
		ok1(arr.alloc >= arr.size);
		ok1(!memcmp(arr.item, lotsOfNumbers, sizeof(lotsOfNumbers)));
		
		for (i=countof(lotsOfNumbers); i;) {
			if (array_pop_nocheck(arr) != (long)lotsOfNumbers[--i]) {
				i++;
				break;
			}
		}
		ok1(i==0);
		ok1(arr.size == 0);
	}
	reset(arr);
	
	testing(array_from_c, array_for, array_rof);
	{
		size_t i_correct, r_correct;
		
		array_from_c(arr, lotsOfNumbers);
		ok1(arr.size == countof(lotsOfNumbers));
		ok1(arr.alloc >= arr.size);
		ok1(!memcmp(arr.item, lotsOfNumbers, sizeof(lotsOfNumbers)));
		
		i_correct = 0;
		r_correct = countof(lotsOfNumbers)-1;
		array_for(i, arr,
			if (i_correct != _i)
				break;
			if (r_correct != _r)
				break;
			if (i != arr.item+_i)
				break;
			if (*i != (long)lotsOfNumbers[_i])
				break;
			i_correct++;
			r_correct--;
		);
		ok1(i_correct == countof(lotsOfNumbers));
		
		i_correct = countof(lotsOfNumbers)-1;
		r_correct = 0;
		array_rof(i, arr,
			if (i_correct != _i)
				break;
			if (r_correct != _r)
				break;
			if (i != arr.item+_i)
				break;
			if (*i != (long)lotsOfNumbers[_i])
				break;
			i_correct--;
			r_correct++;
		);
		ok1(r_correct == countof(lotsOfNumbers));
	}
	reset(arr);
	
	testing(array_append_string);
	{
		for (i=0; i<countof(lotsOfStrings); i++)
			array_append_string(str, lotsOfStrings[i]);
		ok1(str.size == amalgams.stringsSize);
		ok1(str.alloc > str.size);
		ok1(str.item[str.size] == 0);
		ok1(!strcmp(str.item, amalgams.stringsF));
	}
	reset(str);
	
	testing(array_prepend_string);
	{
		for (i=0; i<countof(lotsOfStrings); i++)
			array_prepend_string(str, lotsOfStrings[i]);
		ok1(str.size == amalgams.stringsSize);
		ok1(str.alloc > str.size);
		ok1(str.item[str.size] == 0);
		ok1(!strcmp(str.item, amalgams.stringsB));
	}
	reset(str);
	
	testing(array_from_string);
	{
		for (i=0; i<countof(lotsOfStrings); i++) {
			array_from_string(str, lotsOfStrings[i]);
			if (str.size != strlen(lotsOfStrings[i]))
				break;
			if (str.alloc < strlen(lotsOfStrings[i])+1)
				break;
			if (strcmp(str.item, lotsOfStrings[i]))
				break;
		}
		ok1(i == countof(lotsOfStrings));
	}
	reset(str);
	
	testing(array_resize0);
	{
		size_t prevSize=0, size;
		for (i=0; i<countof(lotsOfNumbers); i++, prevSize=size) {
			size = lotsOfNumbers[i] & 0xFFFF;
			array_resize0(arr, size);
			if (arr.size != size)
				break;
			if (arr.alloc < size)
				break;
			if (size>prevSize) {
				if (!isZeros(arr.item+prevSize, (size-prevSize)*sizeof(*arr.item)))
					break;
			}
			//fill the array with lotsOfNumbers garbage
			memtile(arr.item, arr.size*sizeof(*arr.item), lotsOfNumbers, sizeof(lotsOfNumbers));
		}
		ok1(i==countof(lotsOfNumbers));
	}
	reset(arr);
	
	testing(array_realloc);
	{
		size_t s,a;
		for (i=0; i<countof(lotsOfNumbers); i++) {
			arr.size = (s = lotsOfNumbers[i] >> 16);
				//give size a nonsense value to make sure array_realloc doesn't care about it
			a = amalgams.stringsSize/sizeof(*arr.item)+2;
			array_realloc(arr, a = lotsOfNumbers[i] % ((amalgams.stringsSize/sizeof(*arr.item))+1));
			if (a*sizeof(*arr.item) > amalgams.stringsSize)
				break;
			if (arr.alloc != a)
				break;
			if (arr.size != s)
				break;
			memtile(arr.item, a*sizeof(*arr.item), amalgams.stringsF, a*sizeof(*arr.item));
			if (memcmp(arr.item, amalgams.stringsF, a*sizeof(*arr.item)))
				break;
		}
		ok1(i==countof(lotsOfNumbers));
	}
	reset(arr);
	
	testing(array_growalloc);
	{
		size_t prevA, s, a;
		for (i=0; i<countof(lotsOfNumbers); i++) {
			arr.size = (s = lotsOfNumbers[i] >> 16);
				//give size a nonsense value to make sure array_growalloc doesn't care about it
			a = amalgams.stringsSize/sizeof(*arr.item)+2;
			prevA = arr.alloc;
			array_growalloc(arr, a = lotsOfNumbers[i] % ((amalgams.stringsSize/sizeof(*arr.item))+1));
			if (a*sizeof(*arr.item) > amalgams.stringsSize)
				break;
			if (arr.alloc < a)
				break;
			if (arr.alloc < prevA)
				break;
			if (arr.size != s)
				break;
			
			memtile(arr.item, a*sizeof(*arr.item), amalgams.stringsF, a*sizeof(*arr.item));
			if (memcmp(arr.item, amalgams.stringsF, a*sizeof(*arr.item)))
				break;
			
			//clear the array every now and then
			if (!(lotsOfNumbers[i] & 15)) {
				reset(arr);
			}
		}
		ok1(i==countof(lotsOfNumbers));
	}
	reset(arr);
	
	testing(array_make_room);
	{
		for (i=0; i<countof(lotsOfStrings); i++) {
			char *dest = array_make_room(str, strlen(lotsOfStrings[i]));
			if (str.alloc < str.size+strlen(lotsOfStrings[i]))
				break;
			if (dest != str.item+str.size)
				break;
			
			memcpy(dest, lotsOfStrings[i], strlen(lotsOfStrings[i]));
			str.size += strlen(lotsOfStrings[i]);
		}
		ok1(i==countof(lotsOfStrings));
		ok1(str.size == amalgams.stringsSize);
		
		array_append(str, 0);
		ok1(!strcmp(str.item, amalgams.stringsF));
	}
	reset(str);
	
	testing(array_appends, array_prepends, array_pop);
	{
		#ifndef ARRAY_USE_TALLOC
		array(const char*) array = array_new();
		#else
		array(const char*) array = array_new(NULL);
		#endif
		const char *n[9] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight"};
		
		array_appends(array, n[5], n[6], n[7], n[8]);
		ok1(array.size==4 && array.alloc>=4);
		
		array_prepends(array, n[0], n[1], n[2], n[3], n[4]);
		ok1(array.size==9 && array.alloc>=9);
		
		ok1(array.item[0]==n[0] &&
		    array.item[1]==n[1] &&
		    array.item[2]==n[2] &&
		    array.item[3]==n[3] &&
		    array.item[4]==n[4] &&
		    array.item[5]==n[5] &&
		    array.item[6]==n[6] &&
		    array.item[7]==n[7] &&
		    array.item[8]==n[8]);
		
		ok1(array_pop(array)==n[8] &&
		    array_pop(array)==n[7] &&
		    array_pop(array)==n[6] &&
		    array_pop(array)==n[5] &&
		    array_pop(array)==n[4] &&
		    array_pop(array)==n[3] &&
		    array_pop(array)==n[2] &&
		    array_pop(array)==n[1] &&
		    array_pop(array)==n[0]);
		
		ok1(array.size==0);
		
		ok1(array_pop(array)==NULL && array_pop(array)==NULL && array_pop(array)==NULL);
		
		array_free(array);
	}
	
	trace("Freeing amalgams (internal)");
	freeAmalgams();
	
	return 0;
}

static void generateAmalgams(void) {
	size_t i;
	size_t lotsOfStringsLen = 0;
	const char *src;
	char *p;
	
	for (i=0; i<countof(lotsOfStrings); i++)
		lotsOfStringsLen += strlen(lotsOfStrings[i]);
	amalgams.stringsSize = lotsOfStringsLen;
	
	amalgams.stringsF = malloc(lotsOfStringsLen+1);
	amalgams.stringsB = malloc(lotsOfStringsLen+1);
	
	for (i=0,p=amalgams.stringsF; i<countof(lotsOfStrings); i++) {
		size_t len = strlen(src=lotsOfStrings[i]);
		memcpy(p, src, len);
		p += len;
	}
	*p = 0;
	ok1(p-amalgams.stringsF == (long)lotsOfStringsLen);
	ok1(strlen(amalgams.stringsF) == lotsOfStringsLen);
	
	for (i=countof(lotsOfStrings),p=amalgams.stringsB; i--;) {
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

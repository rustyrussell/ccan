#include <stdio.h>
#include <tap/tap.h>
#include "array/array.h"

#define countof(array...) (sizeof(array)/sizeof(*(array)))
#include "lotsOfNumbers.h"

int main(void) {
	Array(long) array = NewArray();
	size_t i;
	
	plan_tests(3);
	
	{
		for (i=0; i<countof(lotsOfNumbers); i++)
			AAppend(array, lotsOfNumbers[i]);
		ok1(array.size == countof(lotsOfNumbers));
		ok1(array.allocSize >= array.size);
		ok1(!memcmp(array.item, lotsOfNumbers, sizeof(lotsOfNumbers)));
	}
	AFree(array);
	AInit(array);
	
	return 0;
}

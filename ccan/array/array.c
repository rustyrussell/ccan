#include "array.h"
#include <stdarg.h>

int array_alias_helper(const void *a, const void *b) {
	(void)a, (void)b;
	return 0;
}

//grows the allocated size to accommodate the size
void array_resize_helper(array_char *a, size_t itemSize) {
	a->alloc = (a->size+63)&~63;
	#ifndef ARRAY_USE_TALLOC
	a->item = realloc(a->item, a->alloc*itemSize);
	#else
	a->item = talloc_realloc_size(NULL, a->item, a->alloc*itemSize +1);
	#endif
}

void array_resize0_helper(array_char *a, size_t itemSize, size_t newSize) {
	size_t oldSize = a->size;
	a->size = newSize;
	if (newSize > oldSize) {
		if (newSize > a->alloc)
			array_resize_helper(a, itemSize);
		memset(a->item + oldSize*itemSize, 0, (newSize-oldSize)*itemSize);
	}
}

void array_insert_items_helper(array_char *a, size_t itemSize, size_t pos, const void *items, size_t count, size_t tailSize) {
	size_t oldSize = a->size;
	size_t newSize = (a->size += count+tailSize);
	if (newSize > a->alloc)
		array_resize_helper(a, itemSize);
	{
		char *target = a->item + pos*itemSize;
		count *= itemSize;
		memmove(target+count, target, (oldSize-pos)*itemSize);
		memcpy(target, items, count);
	}
}

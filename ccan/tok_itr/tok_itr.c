/* Licensed under LGPL - see LICENSE file for details */
#include "tok_itr.h"

void tok_itr_init(struct tok_itr *itr, const char *list, char delim) {

	itr->delim = delim;
	itr->curr = list;
}

int tok_itr_end(const struct tok_itr *itr) {
	return (itr->curr == NULL);
}

void tok_itr_next(struct tok_itr *itr) {

	itr->curr = strchr(itr->curr, itr->delim);
	if(itr->curr != NULL)
		itr->curr++;
}

size_t tok_itr_val(const struct tok_itr *itr, char *val, size_t size) {
	char *next;
	size_t amt, len;

	if(size < 1)
		return 0;

	next = strchr(itr->curr, itr->delim);
	if(next == NULL)
		len = strlen(itr->curr);
	else if(next == itr->curr)
		len = 0;
	else  /* next > itr->curr */
		len = next - itr->curr;
	
	/* only want to copy at most size - 1 bytes because
	 * we need to save the last spot for '\0' */
	size--;
	amt = (len > size)? size : len;

	if(amt > 0)
		strncpy(val, itr->curr, amt);

	/* just copied bytes 0->(amt-1) for total of *amt* bytes.
	 * now add the null byte to pos at *amt*       */
	val[amt] = '\0';

	return len;
}

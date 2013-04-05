/* Licensed under LGPL - see LICENSE file for details */
#include "tok_itr.h"

void tok_itr_init(struct tok_itr *itr, const char *tokens, char delim) {

	itr->delim = delim;
	itr->next = tokens;
	/* this will set itr->curr and itr->len */
	tok_itr_next(itr);
}

void tok_itr_next(struct tok_itr *itr) {
	itr->curr = itr->next;
	if(itr->curr != NULL) {
		itr->next = strchr(itr->curr, itr->delim);

		if(itr->next == NULL)
			itr->len = strlen(itr->curr);
		else {
			itr->len = itr->next - itr->curr;
			/* increment next to the actual token, not the delimiter */
			itr->next++;
		}
	}
}

size_t tok_itr_val(const struct tok_itr *itr, char *val, size_t size) {
	size_t amt, len;

	len = tok_itr_val_len(itr);
	if(size < 1)
		return len;

	/* only want to copy at most size - 1 bytes because
	 * we need to save the last spot for '\0' */
	size--;
	amt = (len > size)? size : len;
	if(amt > 0)
		memcpy(val, itr->curr, amt);

	/* just copied bytes 0->(amt-1) for total of *amt* bytes.
	 * now add the null byte to position at *amt*       */
	val[amt] = '\0';

	return len;
}

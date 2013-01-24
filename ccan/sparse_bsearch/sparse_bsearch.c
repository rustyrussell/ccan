/* Licensed under LGPLv2+ - see LICENSE file for details */
#include <sys/types.h> //for ssize_t definition
#include "sparse_bsearch.h"

void *_sparse_bsearch(const void *key, const void *base,
		      size_t nmemb, size_t size,
		      int (*cmpfn)(const void *, const void *),
		      bool (*validfn)(const void *))
{
	ssize_t start = 0, end = nmemb - 1;
	const char *p = base;

	while (start <= end) {
		ssize_t next = (start + end) / 2;
		int result;

		while (!validfn(p + next * size)) {
			/* Try the next one along. */
			next++;
			if (next > end) {
				/* Hmm, none of these were valid. */
				next = (start + end) / 2;
				goto trim;
			}
		}

		result = cmpfn(key, p + next * size);
		if (result == 0)
			return (void *)(p + next * size);
		else if (result > 0)
			start = next + 1;
		else {
		trim:
			end = next - 1;
		}
	}
	return NULL;
}

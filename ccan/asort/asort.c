#include <ccan/asort/asort.h>
#include <stdlib.h>

void _asort(void *base, size_t nmemb, size_t size,
	    int(*compar)(const void *, const void *, const void *ctx),
	    const void *ctx)
{
#if HAVE_NESTED_FUNCTIONS
	/* This gives bogus "warning: no previous prototype for ‘cmp’"
	 * with gcc 4 with -Wmissing-prototypes.  Hence the auto crap. */
	auto int cmp(const void *a, const void *b);
	int cmp(const void *a, const void *b)
	{
		return compar(a, b, ctx);
	}
	qsort(base, nmemb, size, cmp);
#else
	#error "Need to open-code quicksort?"
	/* qsort is a classic "needed more real-life testing" API. */
#endif
}

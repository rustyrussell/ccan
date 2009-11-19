#include <ccan/asearch/asearch.h>

static int cmp(const char *key, char *const *elem)
{
	return 0;
}

int main(int argc, char **argv)
{
	const char key[] = "key";

#ifdef FAIL
	int **p;
#if !HAVE_TYPEOF
#error "Unfortunately we don't fail if no typeof."
#endif
#else
	char **p;
#endif
	p = asearch(key, argv+1, argc-1, cmp);
	return p ? 0 : 1;
}

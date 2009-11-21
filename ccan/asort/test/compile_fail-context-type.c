#include <ccan/asort/asort.h>
#include <ccan/asort/asort.c>

static int cmp(char *const *a, char *const *b, int *flag)
{
	return 0;
}

int main(int argc, char **argv)
{
#ifdef FAIL
	char flag;
#if !HAVE_TYPEOF
#error "Unfortunately we don't fail if no typeof."
#endif
#else
	int flag;
#endif
	asort(argv+1, argc-1, cmp, &flag);
	return 0;
}

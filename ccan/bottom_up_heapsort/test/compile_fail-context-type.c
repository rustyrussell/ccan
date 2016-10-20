#include <ccan/bottom_up_heapsort/bottom_up_heapsort.h>
#include <ccan/bottom_up_heapsort/bottom_up_heapsort.c>

static int cmp(char *const *a, char *const *b, int *flag)
{
	return 0;
}

int main(int argc, char **argv)
{
#ifdef FAIL
#if HAVE_TYPEOF && HAVE_BUILTIN_CHOOSE_EXPR && HAVE_BUILTIN_TYPES_COMPATIBLE_P
	char flag;
#else
#error "Unfortunately we don't fail if no typecheck_cb support."
#endif
#else
	int flag;
#endif
	bottom_up_heapsort(argv+1, argc-1, cmp, &flag);
	return 0;
}

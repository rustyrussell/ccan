#include <ccan/minmax/minmax.h>

static int function(void)
{
#ifdef FAIL
	return min(1, 1L);
#if !HAVE_TYPEOF||!HAVE_BUILTIN_CHOOSE_EXPR||!HAVE_BUILTIN_TYPES_COMPATIBLE_P
#error "Unfortunately we don't fail if the typechecks are noops."
#endif
#else
	return 0;
#endif
}

int main(int argc, char *argv[])
{
	function();
	return 0;
}

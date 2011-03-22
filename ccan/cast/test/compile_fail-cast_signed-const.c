#include <ccan/cast/cast.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	unsigned char *uc;
#ifdef FAIL
	const
#endif
	char
		*p = NULL;

	uc = cast_signed(unsigned char *, p);
	return 0;
}

#ifdef FAIL
#if !HAVE_TYPEOF||!HAVE_BUILTIN_CHOOSE_EXPR||!HAVE_BUILTIN_TYPES_COMPATIBLE_P
#error "Unfortunately we don't fail if cast_const can only use size"
#endif
#endif

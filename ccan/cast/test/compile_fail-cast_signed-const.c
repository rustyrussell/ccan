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

#include <ccan/cast/cast.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	char c;
#ifdef FAIL
	char *
#else
	long
#endif
		x = 0;

	c = cast_static(char, x);
	return 0;
}

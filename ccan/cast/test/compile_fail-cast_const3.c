#include <ccan/cast/cast.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	char ***uc;
	const 
#ifdef FAIL
		int
#else
		char
#endif
		***p = NULL;

	uc = cast_const3(char ***, p);
	return 0;
}

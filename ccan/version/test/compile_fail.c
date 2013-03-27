#include <ccan/version/version.h>
#include <ccan/tap/tap.h>

int main(void)
{
#ifdef FAIL
	struct version a;
	a = 0; /* no direct assignment */
#endif
	return 0;
}

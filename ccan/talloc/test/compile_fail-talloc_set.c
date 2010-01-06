#include <ccan/talloc/talloc.c>

int main(void)
{
	int *p;

	talloc_set(
#ifdef FAIL
		p
#else
		&p
#endif
		, NULL);
	return 0;
}

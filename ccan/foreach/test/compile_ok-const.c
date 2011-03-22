#include <ccan/foreach/foreach.h>
#include <ccan/foreach/foreach.c>

/* Iterating over const pointers should work fine. */
int main(int argc, char *argv[])
{
	const char *s1 = "hello", *s2 = "world", *p;
	unsigned int i = 0;

	foreach_ptr(p, s1, s2)
		i++;

	return i == 2 ? 0 : 1;
}

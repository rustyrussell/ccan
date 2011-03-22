#include <ccan/foreach/foreach.h>
#include <ccan/foreach/foreach.c>

/* Iterating const over non-const pointers should work fine. */
int main(int argc, char *argv[])
{
	char *p;
	unsigned int i = 0;

	foreach_ptr(p, argv[0], argv[1])
		i++;

	return i == 2 ? 0 : 1;
}

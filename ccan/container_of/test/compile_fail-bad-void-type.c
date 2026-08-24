#include <ccan/container_of/container_of.h>
#include <stdlib.h>

struct foo {
	int *a;
	char b;
};

int main(void)
{
	struct foo foo = { .a = NULL, .b = 2 };
	void *voidp = &foo.a;
	const char *ccharp = &foo.b;
	char *charp = &foo.b;
	struct foo *p;

#ifdef FAIL
	/* voidp is a void * but b is an int */
	p = container_of(voidp, struct foo, a);
#else
	p = voidp;
	p = container_of(ccharp, struct foo, b);
	p = container_of(charp, struct foo, b);
#endif
	return p == NULL;
}

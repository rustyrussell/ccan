#include <ccan/array_size/array_size.h>
#include <stdlib.h>

struct foo {
	unsigned int a, b;
};

int check_parameter(const struct foo array[4]);
int check_parameter(const struct foo array[4])
{
#ifdef FAIL
	return (ARRAY_SIZE(array) == 4);
#else
	return sizeof(array) == 4 * sizeof(struct foo);
#endif
}

int main(int argc, char *argv[])
{
	return check_parameter(NULL);
}

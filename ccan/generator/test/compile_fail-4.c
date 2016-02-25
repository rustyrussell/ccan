#include <stdio.h>
#include <stdlib.h>

#include <ccan/generator/generator.h>

#include <ccan/generator/generator.c>

#include "example-gens.h"

int main(int argc, char *argv[])
{
	generator_t(int) g = gen1();
#ifdef FAIL
	char *val;
#else
	int val;
#endif

	generator_next_val(val, g);
	printf("%d", val);

	exit(0);
}

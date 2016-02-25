#include <stdio.h>
#include <stdlib.h>

#include <ccan/generator/generator.h>

#include <ccan/generator/generator.c>

#include "example-gens.h"

int main(int argc, char *argv[])
{
#ifdef FAIL
	int *g = gen1();
#else
	generator_t(int) g = gen1();
#endif

	printf("%d", *generator_next(g));

	exit(0);
}

#include <stdio.h>
#include <stdlib.h>

#include <ccan/generator/generator.h>

#include <ccan/generator/generator.c>

#include "example-gens.h"

int main(int argc, char *argv[])
{
#ifdef FAIL
	int *g = NULL;
#else
	generator_t(int) g = gen1();
#endif

	generator_free(g);

	exit(0);
}

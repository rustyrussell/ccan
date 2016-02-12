#include <stdio.h>

#include <ccan/generator/generator.h>

#include "example-gens.h"

generator_def(gen1, int)
{
	generator_yield(1);
	generator_yield(3);
	generator_yield(17);
}

generator_def(gen2, int, int, base)
{
	generator_yield(base + 1);
	generator_yield(base + 3);
	generator_yield(base + 17);
}

generator_def(gen3, const char *, const char *, str, int, count)
{
	int i;

	for (i = 0; i < count; i++)
		generator_yield(str);
}


#include <ccan/generator/generator.h>

#include "example-gens.h"

generator_def(gen1, int)
{
	generator_yield(1);
	generator_yield(3);
	generator_yield(17);
}

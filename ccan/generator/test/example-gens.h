#ifndef _EXAMPLE_GENS_H
#define _EXAMPLE_GENS_H

#include <ccan/generator/generator.h>

generator_declare(gen1, int);
generator_declare(gen2, int, int, base);
generator_declare(gen3, const char *, const char *, str, int, count);

#endif /* _EXAMPLE_GENS_H */

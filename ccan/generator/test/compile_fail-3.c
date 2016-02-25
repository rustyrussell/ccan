#include <stdio.h>
#include <stdlib.h>

#include <ccan/generator/generator.h>

#include <ccan/generator/generator.c>

generator_def_static(intgen, int)
{
#ifdef FAIL
	generator_yield("a");
#else
	generator_yield(1);
#endif
}

int main(int argc, char *argv[])
{
	generator_t(int) g = intgen();

	printf("%d", *generator_next(g));

	exit(0);
}

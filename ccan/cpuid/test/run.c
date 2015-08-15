#include "../cpuid.c"

#include <stdio.h>
#include <stdint.h>

int main(void)
{
	if (!cpuid_is_supported()) {
		printf ("CPUID instruction is not supported by this CPU\n");
		return 1;
	}

	uint32_t info = CPUID_HIGHEST_EXTENDED_FUNCTION_SUPPORTED | CPUID_EXTENDED_L2_CACHE_FEATURES | CPUID_VIRT_PHYS_ADDR_SIZES
			| CPUID_PROCINFO_AND_FEATUREBITS;
	uint32_t featureset = CPUID_FEAT_ECX_ALL | CPUID_FEAT_EDX_ALL;
	if (!cpuid_write_info(info, featureset, stdout)) {
		printf("Failed to write CPU information!\n");
		return 1;
	}

	printf("Wrote CPU information\n");
	return 0;
}


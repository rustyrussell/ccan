#include "../cpuid.c"

#include <stdio.h>
#include <stdint.h>

int main(void)
{
	if (!cpuid_is_supported()) {
		printf ("CPUID instruction is not supported by this CPU\n");
		return 1;
	}

	char cputype[12];
	if (cpuid_sprintf_cputype(cpuid_get_cpu_type(), cputype))
		printf ("Vendor ID: %s\n", cputype);

	char buf[48];
	cpuid(CPUID_PROC_BRAND_STRING, (uint32_t *)buf);
	printf ("Processor Brand: %s\n", buf);

	uint32_t procinfo[9];
	cpuid(CPUID_PROCINFO_AND_FEATUREBITS, procinfo);
	printf("Stepping: %d Model: 0x%X Family: %d extended model: %d extended family: %d\n",
		procinfo[0], procinfo[1], procinfo[2], procinfo[3], procinfo[4]);
	printf("Brand Index: %d CL Flush Line Size: %d Logical Processors: %d Initial APICID: %d\n",
		procinfo[5], procinfo[6], procinfo[7], procinfo[8]);

	printf ("Highest extended function supported: %#010x\n", cpuid_highest_ext_func_supported());

	uint32_t phys_virt[2];
	cpuid(CPUID_VIRT_PHYS_ADDR_SIZES, phys_virt);
	printf ("Physical address size: %d\nVirtual address size: %d\n", phys_virt[0], phys_virt[1]);

	uint32_t extfeatures[2];
	cpuid(CPUID_EXTENDED_PROC_INFO_FEATURE_BITS, extfeatures);
	printf ("Extended processor info and feature bits: %d %d\n", extfeatures[0], extfeatures[1]);

	uint32_t l2c[3];
	cpuid(CPUID_EXTENDED_L2_CACHE_FEATURES, l2c);
	printf("L2 Line size: %u bytes\tAssociativity: %02xh\tCache Size: %u KB\n",
		l2c[0], l2c[1], l2c[2]);

	uint32_t invalid;
	cpuid(0x0ffffffUL, &invalid);
	printf ("Testing invalid: %#010x\n", invalid);
	return 0;
}

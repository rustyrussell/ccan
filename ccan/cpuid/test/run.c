#include "cpuid.h"

#include <stdio.h>

int main()
{
	if (!cpuid_is_supported()) {
		printf ("CPUID instruction is not supported by this CPU\n");
		return 1;
	}

	printf ("MMX:  %s\n", cpuid_has_mmx()  ? "Yes" : "No");
	printf ("SSE:  %s\n", cpuid_has_sse()  ? "Yes" : "No");
	printf ("SSE2: %s\n", cpuid_has_sse2() ? "Yes" : "No");
	printf ("SSE3: %s\n", cpuid_has_sse3() ? "Yes" : "No");
	printf ("x64:  %s\n", cpuid_has_x64()  ? "Yes" : "No");

	char buf[128];
	cpuid(CPU_VENDORID, buf);
	printf ("Vendor ID: %s\n", buf);

	cpuid(CPU_PROC_BRAND_STRING, buf);
	printf ("Processor Brand: %s\n", buf);

	int addr;
	cpuid(CPU_HIGHEST_EXTENDED_FUNCTION_SUPPORTED, &addr);
	printf ("Highest extended function supported: %#010x\n", addr);

	int virtphys_size;
	cpuid(CPU_VIRT_PHYS_ADDR_SIZES, &virtphys_size);
	printf ("Virtual and physical address sizes: %d\n", virtphys_size);

	int extfeatures[2];
	cpuid(CPU_EXTENDED_PROC_INFO_FEATURE_BITS, extfeatures);
	printf ("Extended processor info and feature bits: %d %d\n", extfeatures[0], extfeatures[1]);

	int l2features[3];
	cpuid(CPU_EXTENDED_L2_CACHE_FEATURES, l2features);
	printf ("L2 Cache Size: %u KB\tLine Size: %u bytes\tAssociativity: %02xh\n",
			l2features[0], l2features[1], l2features[2]);

	int invalid;
	cpuid(0x0ffffffUL, &invalid);
	printf ("Testing invalid: %#010x\n", invalid);
	return 0;
}


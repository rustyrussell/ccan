#include "cpuid.h"

#include <stdio.h>
#include <stdint.h>

int main()
{
	if (!cpuid_is_supported()) {
		printf ("CPUID instruction is not supported by this CPU\n");
		return 1;
	}

	printf ("Vendor ID: %s\n", cpuid_get_cpu_type_string (cpuid_get_cpu_type ()));

	char buf[48];
	cpuid(CPU_PROC_BRAND_STRING, buf);
	printf ("Processor Brand: %s\n", buf);

	printf ("Highest extended function supported: %#010x\n", cpuid_highest_ext_func_supported());

	union {
		struct {
			uint32_t phys_bits : 8;
			uint32_t virt_bits : 8;
			uint32_t reserved  : 16;
		};
		uint32_t w;
	} s;
	cpuid(CPU_VIRT_PHYS_ADDR_SIZES, &s.w);
	printf ("Physical address size: %d\nVirtual: %d\n", s.phys_bits, s.virt_bits);

	int extfeatures[2];
	cpuid(CPU_EXTENDED_PROC_INFO_FEATURE_BITS, extfeatures);
	printf ("Extended processor info and feature bits: %d %d\n", extfeatures[0], extfeatures[1]);

	union {
		struct {
			uint32_t line_size : 8;
			uint32_t reserved : 4;
			uint32_t assoc : 4;
			uint32_t cache_size : 16;
		};

		uint32_t w;
	} l2c;

	cpuid(CPU_EXTENDED_L2_CACHE_FEATURES, &l2c.w);
	printf ("L2 Cache Size: %ld KB\tLine Size: %ld bytes\tAssociativity: %02xh\n",
			l2c.cache_size, l2c.line_size, l2c.assoc);

	int invalid;
	cpuid(0x0ffffffUL, &invalid);
	printf ("Testing invalid: %#010x\n", invalid);
	return 0;
}

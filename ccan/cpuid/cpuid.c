/*
 * Copyright (c) 2013 Ahmed Samy  <f.fallen45@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file has been written with some help from wikipedia:
 * 	http://en.wikipedia.org/wiki/CPUID
 */
#include <stdint.h>

#include "cpuid.h"

enum {
	CPU_PROC_BRAND_STRING_INTERNAL0  		= 0x80000003,
	CPU_PROC_BRAND_STRING_INTERNAL1 		= 0x80000004
};

#ifndef _MSC_VER
static void ___cpuid(cpuid_t info, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	__asm__(
		"xchg %%ebx, %%edi\n\t" 	/* 32bit PIC: Don't clobber ebx.  */
		"cpuid\n\t"
		"xchg %%ebx, %%edi\n\t"
		: "=a"(*eax), "=D"(*ebx), "=c"(*ecx), "=d"(*edx)
		: "0" (info)
	);
}
#else
#include <intrin.h>

static void ___cpuid(cpuid_t info, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint32_t registers[4];
	__cpuid(registers, info);

	*eax = registers[0];
	*ebx = registers[1];
	*ecx = registers[2];
	*edx = registers[3];
}
#endif

int highest_ext_func_supported(void)
{
	static int highest;

	if (!highest) {
		asm volatile(
			"cpuid\n\t"
			: "=a" (highest)
			: "a" (CPU_HIGHEST_EXTENDED_FUNCTION_SUPPORTED)
		);
	}

	return highest;
}

int cpuid_test_feature(cpuid_t feature)
{
	if (feature > CPU_VIRT_PHYS_ADDR_SIZES || feature < CPU_EXTENDED_PROC_INFO_FEATURE_BITS)
		return 0;

	return (feature <= highest_ext_func_supported());
}

int cpuid_has_feature(cpufeature_t feature)
{
	uint32_t eax, ebx, ecx, edx;

	___cpuid(CPU_PROCINFO_AND_FEATUREBITS, &eax, &ebx, &ecx, &edx);
	switch (feature) {
		case CF_MMX:
		case CF_SSE:
		case CF_SSE2:
			return (edx & ((int)feature)) != 0;
		case CF_SSE3:
		case CF_SSSE3:
		case CF_SSE41:
		case CF_SSE42:
		case CF_AVX:
		case CF_FMA:
			return (ecx & ((int)feature)) != 0;
	}

	return 0;
}

int cpuid_has_ext_feature(cpuextfeature_t extfeature)
{
	uint32_t eax, ebx, ecx, edx;
	if (!cpuid_test_feature(CPU_EXTENDED_PROC_INFO_FEATURE_BITS))
		return 0;

	___cpuid(CPU_EXTENDED_PROC_INFO_FEATURE_BITS, &eax, &ebx, &ecx, &edx);
	switch (extfeature) {
		case CEF_x64:
			return (edx & ((int)extfeature)) != 0;
		case CEF_SSE4a:
		case CEF_FMA4:
		case CEF_XOP:
			return (ecx & ((int)extfeature)) != 0;
	}

	return 0;
}

void cpuid(cpuid_t info, void *buf)
{
	/* Sanity checks, make sure we're not trying to do something
	 * invalid or we are trying to get information that isn't supported
	 * by the CPU.  */
	if (info > CPU_VIRT_PHYS_ADDR_SIZES || (info > CPU_HIGHEST_EXTENDED_FUNCTION_SUPPORTED
		&& !cpuid_test_feature(info)))
		return;

	uint32_t *ubuf = buf;
	if (info == CPU_PROC_BRAND_STRING) {
		___cpuid(CPU_PROC_BRAND_STRING,  	  &ubuf[0], &ubuf[1], &ubuf[2],  &ubuf[3]);
		___cpuid(CPU_PROC_BRAND_STRING_INTERNAL0, &ubuf[4], &ubuf[5], &ubuf[6],  &ubuf[7]);
		___cpuid(CPU_PROC_BRAND_STRING_INTERNAL1, &ubuf[8], &ubuf[9], &ubuf[10], &ubuf[11]);
		return;
	} else if (info == CPU_HIGHEST_EXTENDED_FUNCTION_SUPPORTED) {
		*ubuf = highest_ext_func_supported();
		return;
	}

	uint32_t eax, ebx, ecx, edx;
	___cpuid(info, &eax, &ebx, &ecx, &edx);

	switch (info) {
		case CPU_VENDORID:
			ubuf[0] = ebx;
			ubuf[1] = edx;
			ubuf[2] = ecx;
			break;
		case CPU_PROCINFO_AND_FEATUREBITS:
			ubuf[0] = eax; 	/* The so called "signature" of the CPU.  */
			ubuf[1] = edx; 	/* Feature flags #1.  */
			ubuf[2] = ecx; 	/* Feature flags #2.  */
			ubuf[3] = ebx; 	/* Additional feature information.  */
			break;
		case CPU_CACHE_AND_TLBD_INFO:
			ubuf[0] = eax;
			ubuf[1] = ebx;
			ubuf[2] = ecx;
			ubuf[3] = edx;
			break;
		case CPU_EXTENDED_PROC_INFO_FEATURE_BITS:
			ubuf[0] = edx;
			ubuf[1] = ecx;
			break;
		case CPU_L1_CACHE_AND_TLB_IDS:
			break;
		case CPU_EXTENDED_L2_CACHE_FEATURES:
			ubuf[0] = (ecx & 0xFF); 		/* Cache size  */
			ubuf[1] = (ecx >> 12) & 0xF; 		/* Line size  */
			ubuf[2] = (ecx >> 16) & 0xFFFF; 	/* Associativity  */
			break;
		case CPU_ADV_POWER_MGT_INFO:
			break;
		case CPU_VIRT_PHYS_ADDR_SIZES:
			*ubuf = eax;
			break;
		default:
			*ubuf = 0xbaadf00d;
			break;
	}
}


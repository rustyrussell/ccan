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

/* Only compile this file if we're on a x86 machine.  */
#if defined(__i386__) || defined(__i386) || defined(__x86_64) \
	|| defined(_M_AMD64) || defined(__M_X64)
#include "cpuid.h"

#include <string.h>
#include <stdio.h>

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

static struct {
	uint32_t feature;
	uint32_t mask;
	bool use_edx; 		/* ecx will be used if false.  */
} features[] = {
	{ CF_MMX, 		1 << 23, 	true },
	{ CF_SSE, 		1 << 25, 	true },
	{ CF_SSE2, 		1 << 26, 	true },
	{ CF_SSE3, 		1 << 9,  	false },
	{ CF_FPU, 		1 << 0,  	true },

	{ CF_TSC, 		1 << 4,  	true },
	{ CF_MSR, 		1 << 5,  	true },

	{ CF_SSSE3,     	1 << 9,  	false },
	{ CF_AVX, 		1 << 28, 	false },

	/* Extended ones.  */
	{ CEF_x64, 		1 << 30, 	true },
	{ CEF_FPU, 		1 << 0,  	true },
	{ CEF_DE, 		1 << 2,  	true },
	{ CEF_SYSCALLRET, 	1 << 11, 	true },
	{ CEF_CMOV, 		1 << 15, 	true },

	{ CEF_SSE4a, 		1 << 6, 	false },
	{ CEF_FMA4, 		1 << 16, 	false },
	{ CEF_XOP, 		1 << 11, 	false }
};

bool cpuid_is_supported(void)
{
	int ret = 0;
#if defined(__GNUC__) || defined(__clang__)
	/* The following assembly code uses EAX as the return value,
	 * but we store the value of EAX into ret since GCC uses EAX
	 * as the return register for every C function.  That's a double
	 * operation, but there's no other way to do this unless doing this
	 * function entirely in assembly.
	 *
	 * The following assembly code has been shamelessly stolen from:
	 * 	http://wiki.osdev.org/CPUID
	 * and converted to work with AT&T syntax.
	 *
	 * This check is to make sure that the compiler is actually compiling
	 * for 64-bit.
	 *
	 * The compiler can be 32-bit and the system 64-bit so the 
	 * following would be true:
	 * 	#if defined(__x86_64) ...
	 */

#if UINTPTR_MAX == 0xffffffffffffffff
#define ASM_PUSHF 	"pushfq\n\t"
#define ASM_POPF	"popfq\n\t"
#define ASM_PUSHEAX 	"pushq %%rax\n\t"
#define ASM_POPEAX 	"popq %%rax\n\t"
#define ASM_PUSHECX 	"pushq %%rcx\n\t"
#elif UINTPTR_MAX == 0xffffffff
#define ASM_PUSHF 	"pushfl\n\t"
#define ASM_POPF	"popfl\n\t"
#define ASM_PUSHEAX 	"pushl %%eax\n\t"
#define ASM_POPEAX 	"popl %%eax\n\t"
#define ASM_PUSHECX 	"pushl %%ecx\n\t"
#endif

	asm volatile(
		ASM_PUSHF
		ASM_POPEAX
		"movl %%eax, %%ecx\n\t"
		"xorl $0x200000, %%eax\n\t"
		ASM_PUSHEAX
		ASM_POPF
		ASM_PUSHF
		ASM_POPEAX
		"xorl %%ecx, %%eax\n\t"
		"shrl $21, %%eax\n\t"
		"andl $1, %%eax\n\t"
		ASM_PUSHECX
		ASM_POPF
		: "=a" (ret)
	);

#undef ASM_PUSHF
#undef ASM_POPF
#undef ASM_PUSHEAX
#undef ASM_POPEAX
#undef ASM_PUSHECX
#elif defined _MSC_VER
	__asm {
		pushfd
		pop eax
		mov ecx, eax
		xor eax, 0x200000
		push eax
		popfd

		pushfd
		pop eax
		xor eax, ecx
		shr eax, 0x21
		and eax, 0x1
		push ecx
		popfd

		mov eax, ret
	};
#endif
	return !!ret;
}

bool cpuid_test_feature(cpuid_t feature)
{
	if (feature > CPU_VIRT_PHYS_ADDR_SIZES || feature < CPU_EXTENDED_PROC_INFO_FEATURE_BITS)
		return false;

	return (feature <= cpuid_highest_ext_func_supported());
}

bool cpuid_has_feature(int feature, bool extended)
{
	uint32_t eax, ebx, ecx, edx, i;

	if (!extended)
		___cpuid(CPU_PROCINFO_AND_FEATUREBITS, &eax, &ebx, &ecx, &edx);
	else
		___cpuid(CPU_EXTENDED_PROC_INFO_FEATURE_BITS, &eax, &ebx, &ecx, &edx);

	for (i = 0; i < sizeof(features) / sizeof(features[0]); ++i) {
		if (features[i].feature == feature) {
			if (features[i].use_edx)
				return (edx & features[i].mask);
			else
				return (ecx & features[i].mask);
		}
	}
	return false;
}

static const char *const cpuids[] = {
	"Nooooooooone",
	"AMDisbetter!",
	"AuthenticAMD",
	"CentaurHauls",
	"CyrixInstead",
	"GenuineIntel",
	"TransmetaCPU",
	"GeniuneTMx86",
	"Geode by NSC",
	"NexGenDriven",
	"RiseRiseRise",
	"SiS SiS SiS ",
	"UMC UMC UMC ",
	"VIA VIA VIA ",
	"Vortex86 SoC",
	"KVMKVMKVMKVM"
};

cputype_t cpuid_get_cpu_type(void)
{
	static cputype_t cputype;
	if (cputype == CT_NONE) {
		union {
			char buf[12];
			uint32_t bufu32[3];
		} u;
		uint32_t i;

		___cpuid(CPU_VENDORID, &i, &u.bufu32[0], &u.bufu32[2], &u.bufu32[1]);
		for (i = 0; i < sizeof(cpuids) / sizeof(cpuids[0]); ++i) {
			if (strncmp(cpuids[i], u.buf, 12) == 0) {
				cputype = (cputype_t)i;
				break;
			}
		}
	}

	return cputype;
}

bool cpuid_sprintf_cputype(const cputype_t cputype, char *buf)
{
	if (cputype == CT_NONE)
		return false;

	memcpy(buf, cpuids[(int)cputype], 12);
	buf[12] = '\0';
	return true;
}

uint32_t cpuid_highest_ext_func_supported(void)
{
	static uint32_t highest;

	if (!highest) {
#if defined(__GNUC__) || defined(__clang__)
		asm volatile(
			"cpuid\n\t"
			: "=a" (highest)
			: "a" (CPU_HIGHEST_EXTENDED_FUNCTION_SUPPORTED)
		);
#elif defined _MSC_VER
		__asm {
			mov eax, CPU_HIGHEST_EXTENDED_FUNCTION_SUPPORTED
			cpuid
			mov highest, eax
		};
#endif
	}

	return highest;
}

void cpuid(cpuid_t info, uint32_t *buf)
{
	/* Sanity checks, make sure we're not trying to do something
	 * invalid or we are trying to get information that isn't supported
	 * by the CPU.  */
	if (info > CPU_VIRT_PHYS_ADDR_SIZES || (info > CPU_HIGHEST_EXTENDED_FUNCTION_SUPPORTED
		&& !cpuid_test_feature(info)))
		return;

	if (info == CPU_PROC_BRAND_STRING) {
		static char cached[48] = { 0 };
		if (cached[0] == '\0') {
			___cpuid(CPU_PROC_BRAND_STRING,		  &buf[0], &buf[1], &buf[2],  &buf[3]);
			___cpuid(CPU_PROC_BRAND_STRING_INTERNAL0, &buf[4], &buf[5], &buf[6],  &buf[7]);
			___cpuid(CPU_PROC_BRAND_STRING_INTERNAL1, &buf[8], &buf[9], &buf[10], &buf[11]);

			memcpy(cached, buf, sizeof cached);
		} else
			buf = (uint32_t *)cached;

		return;
	} else if (info == CPU_HIGHEST_EXTENDED_FUNCTION_SUPPORTED) {
		*buf = cpuid_highest_ext_func_supported();
		return;
	}

	uint32_t eax, ebx, ecx, edx;
	___cpuid(info, &eax, &ebx, &ecx, &edx);

	switch (info) {
		case CPU_VENDORID:
			buf[0] = ebx;
			buf[1] = edx;
			buf[2] = ecx;
			break;
		case CPU_PROCINFO_AND_FEATUREBITS:
			buf[0] = (eax & 0x0F);		/* Stepping  */
			buf[1] = (eax >> 4)  & 0x0F; 	/* Model  */
			buf[2] = (eax >> 8)  & 0x0F; 	/* Family  */
			buf[3] = (eax >> 16) & 0x0F; 	/* Extended Model.  */
			buf[4] = (eax >> 24) & 0x0F; 	/* Extended Family.  */

			buf[5] = edx; 			/* Feature flags #1.  */
			buf[6] = ecx; 			/* Feature flags #2.  */
			buf[7] = ebx; 			/* Additional feature information.  */
			break;
		case CPU_CACHE_AND_TLBD_INFO:
			buf[0] = eax;
			buf[1] = ebx;
			buf[2] = ecx;
			buf[3] = edx;
			break;
		case CPU_EXTENDED_PROC_INFO_FEATURE_BITS:
			buf[0] = edx;
			buf[1] = ecx;
			break;
		case CPU_L1_CACHE_AND_TLB_IDS:
			buf[0] = eax;
			buf[1] = ebx;
			buf[2] = ecx;
			buf[3] = edx;
			break;
		case CPU_EXTENDED_L2_CACHE_FEATURES:
			buf[0] = ecx & 0xFF; 		/* Line size.  */
			buf[1] = (ecx >> 12) & 0xFF; 	/* Associativity.  */
			buf[2] = ecx >> 16; 		/* Cache size.  */
			break;
		case CPU_ADV_POWER_MGT_INFO:
			*buf = edx;
			break;
		case CPU_VIRT_PHYS_ADDR_SIZES:
			buf[0] = eax & 0xFF; 		/* physical.  */
			buf[1] = (eax >> 8) & 0xFF; 	/* virtual.  */
			break;
		default:
			*buf = 0xbaadf00d;
			break;
	}
}

#endif

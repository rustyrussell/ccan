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
	CPUID_PROC_BRAND_STRING_INTERNAL0  		= 0x80000003,
	CPUID_PROC_BRAND_STRING_INTERNAL1 		= 0x80000004
};

#ifndef _MSC_VER
static void get_cpuid(cpuid_t info, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
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

static void get_cpuid(cpuid_t info, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint32_t registers[4];
	__cpuid(registers, info);

	*eax = registers[0];
	*ebx = registers[1];
	*ecx = registers[2];
	*edx = registers[3];
}
#endif

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
		shr eax, 21
		and eax, 1
		push ecx
		popfd

		mov eax, ret
	};
#endif
	return !!ret;
}

bool cpuid_test_feature(cpuid_t feature)
{
	if (feature > CPUID_VIRT_PHYS_ADDR_SIZES || feature < CPUID_EXTENDED_PROC_INFO_FEATURE_BITS)
		return false;

	return (feature <= cpuid_highest_ext_func_supported());
}

#if defined(__GNUC__) || defined(__clang__)
static uint32_t fetch_ecx(uint32_t what)
{
	static uint32_t ecx;
	if (ecx == 0) {
		asm volatile(
			"cpuid\n\t"
			: "=c" (ecx)
			: "a" (what)
		);
	}

	return ecx;
}

static uint32_t fetch_edx(uint32_t what)
{
	static uint32_t edx;
	if (edx == 0) {
		asm volatile(
			"cpuid\n\t"
			: "=d" (edx)
			: "a" (what)
		);
	}

	return edx;
}
#elif defined(_MSC_VER)
static uint32_t fetch_ecx(uint32_t what)
{
	static uint32_t _ecx;
	if (_ecx == 0) {
		__asm {
			mov eax, what
			cpuid
			mov _ecx, ecx
		};
	}

	return _ecx;
}

static uint32_t fetch_edx(uint32_t what)
{
	static uint32_t _edx;
	if (_edx == 0) {
		__asm {
			mov eax, what
			cpuid
			mov _edx, edx
		};
	}

	return _edx;
}
#endif

#define DEFINE_FEATURE_FUNC(NAME, REGISTER, TYPE) \
	bool cpuid_has_##NAME(int feature) \
	{ \
		static uint32_t REGISTER; \
		if (REGISTER == 0) \
			REGISTER = fetch_##REGISTER(TYPE); \
		return !!(REGISTER & feature); \
	}

DEFINE_FEATURE_FUNC(ecxfeature, ecx, CPUID_PROCINFO_AND_FEATUREBITS)
DEFINE_FEATURE_FUNC(edxfeature, edx, CPUID_PROCINFO_AND_FEATUREBITS)

DEFINE_FEATURE_FUNC(ecxfeature_ext, ecx, CPUID_EXTENDED_PROC_INFO_FEATURE_BITS)
DEFINE_FEATURE_FUNC(edxfeature_ext, edx, CPUID_EXTENDED_PROC_INFO_FEATURE_BITS)

#undef DEFINE_FEATURE_FUNC

cputype_t cpuid_get_cpu_type(void)
{
	static cputype_t cputype;
	if (cputype == CT_NONE) {
		union {
			char buf[12];
			uint32_t bufu32[3];
		} u;
		uint32_t i;

		get_cpuid(CPUID_VENDORID, &i, &u.bufu32[0], &u.bufu32[2], &u.bufu32[1]);
		for (i = 0; i < sizeof(c_cpunames) / sizeof(c_cpunames); ++i) {
			if (strncmp(c_cpunames[i], u.buf, 12) == 0) {
				cputype = (cputype_t)i;
				break;
			}
		}
	}

	return cputype;
}

uint32_t cpuid_highest_ext_func_supported(void)
{
	static uint32_t highest;

	if (!highest) {
#if defined(__GNUC__) || defined(__clang__)
		asm volatile(
			"cpuid\n\t"
			: "=a" (highest)
			: "a" (CPUID_HIGHEST_EXTENDED_FUNCTION_SUPPORTED)
		);
#elif defined _MSC_VER
		__asm {
			mov eax, CPUID_HIGHEST_EXTENDED_FUNCTION_SUPPORTED
			cpuid
			mov highest, eax
		};
#endif
	}

	return highest;
}

void cpuid(cpuid_t request, uint32_t *buf)
{
	/* Sanity checks, make sure we're not trying to do something
	 * invalid or we are trying to get information that isn't supported
	 * by the CPU.  */
	if (request > CPUID_VIRT_PHYS_ADDR_SIZES || (request > CPUID_HIGHEST_EXTENDED_FUNCTION_SUPPORTED
		&& !cpuid_test_feature(request)))
		return;

	if (request == CPUID_PROC_BRAND_STRING) {
		static char cached[48] = { 0 };
		if (cached[0] == '\0') {
			get_cpuid(CPUID_PROC_BRAND_STRING,	    &buf[0], &buf[1], &buf[2],  &buf[3] );
			get_cpuid(CPUID_PROC_BRAND_STRING_INTERNAL0, &buf[4], &buf[5], &buf[6],  &buf[7] );
			get_cpuid(CPUID_PROC_BRAND_STRING_INTERNAL1, &buf[8], &buf[9], &buf[10], &buf[11]);

			memcpy(cached, buf, sizeof cached);
		} else
			buf = (uint32_t *)cached;

		return;
	} else if (request == CPUID_HIGHEST_EXTENDED_FUNCTION_SUPPORTED) {
		*buf = cpuid_highest_ext_func_supported();
		return;
	}

	uint32_t eax, ebx, ecx, edx;
	get_cpuid(request, &eax, &ebx, &ecx, &edx);

	switch (request) {
		case CPUID_VENDORID:
			buf[0] = ebx;
			buf[1] = edx;
			buf[2] = ecx;
			break;
		case CPUID_PROCINFO_AND_FEATUREBITS:
			buf[0] = (eax & 0x0F);		/* Stepping  */
			buf[1] = (eax >> 4)  & 0x0F; 	/* Model  */
			buf[2] = (eax >> 8)  & 0x0F; 	/* Family  */
			buf[3] = (eax >> 16) & 0x0F; 	/* Extended Model.  */
			buf[4] = (eax >> 24) & 0x0F; 	/* Extended Family.  */

			/* Additional Feature information.  */
			buf[5] = ebx & 0xFF;
			buf[6] = (ebx >> 8) & 0xFF;
			buf[7] = (ebx >> 16) & 0xFF;
			buf[8] = ebx >> 24;
			break;
		case CPUID_CACHE_AND_TLBD_INFO:
			buf[0] = eax;
			buf[1] = ebx;
			buf[2] = ecx;
			buf[3] = edx;
			break;
		case CPUID_EXTENDED_PROC_INFO_FEATURE_BITS:
			buf[0] = edx;
			buf[1] = ecx;
			break;
		case CPUID_L1_CACHE_AND_TLB_IDS:
			buf[0] = eax & 0xFF;
			buf[1] = (eax >> 8) & 0xFF;
			buf[2] = (eax >> 16) & 0xFF;
			buf[3] = eax >> 24;

			buf[4] = ebx & 0xFF;
			buf[5] = (ebx >> 8) & 0xFF;
			buf[6] = (ebx >> 16) & 0xFF;
			buf[7] = ebx >> 24;

			buf[8] = ecx & 0xFF;
			buf[9] = (ecx >> 8) & 0xFF;
			buf[10] = (ecx >> 16) & 0xFF;
			buf[11] = ecx >> 24;

			buf[12] = edx & 0xFF;
			buf[13] = (edx >> 8) & 0xFF;
			buf[14] = (edx >> 16) & 0xFF;
			buf[15] = edx >> 24;
			break;
		case CPUID_EXTENDED_L2_CACHE_FEATURES:
			buf[0] = ecx & 0xFF; 		/* Line size.  */
			buf[1] = (ecx >> 12) & 0xFF; 	/* Associativity.  */
			buf[2] = ecx >> 16; 		/* Cache size.  */
			break;
		case CPUID_ADV_POWER_MGT_INFO:
			*buf = edx;
			break;
		case CPUID_VIRT_PHYS_ADDR_SIZES:
			buf[0] = eax & 0xFF; 		/* physical.  */
			buf[1] = (eax >> 8) & 0xFF; 	/* virtual.  */
			break;
		default:
			*buf = 0xbaadf00d;
			break;
	}
}

bool cpuid_write_info(uint32_t info, uint32_t featureset, FILE *file)
{
	char brand[48];
	cpuid(CPUID_PROC_BRAND_STRING, (uint32_t *)brand);

	fprintf(file, "-- CPU Information for: %s_%s --\n\n", cpuid_get_name(), brand);
	if (info & CPUID_HIGHEST_EXTENDED_FUNCTION_SUPPORTED)
		fprintf(file, "Highest extended function supported: %#010x\n\n", cpuid_highest_ext_func_supported());

	if (info & CPUID_EXTENDED_L2_CACHE_FEATURES) {
		uint32_t l2c[3];
		cpuid(CPUID_EXTENDED_L2_CACHE_FEATURES, l2c);

		fprintf(file, "-- Extended L2 Cache features --\nL2 Line size: %u bytes\nAssociativity: %02xh\nCache Size: %u KB\n\n",
			l2c[0], l2c[1], l2c[2]);
	}

	if (info & CPUID_VIRT_PHYS_ADDR_SIZES) {
		uint32_t phys_virt[2];
		cpuid(CPUID_VIRT_PHYS_ADDR_SIZES, phys_virt);

		fprintf(file, "-- Virtual and Physical address sizes --\n"
				"Physical address size: %d\nVirtual address size: %d\n\n", phys_virt[0], phys_virt[1]);
	}

	if (info & CPUID_PROCINFO_AND_FEATUREBITS) {
		uint32_t procinfo[9];
		cpuid(CPUID_PROCINFO_AND_FEATUREBITS, procinfo);

		fputs("-- Processor information and feature bits --\n", file	);
		fprintf(file, "Stepping: %d\nModel: 0x%X\nFamily: %d\nExtended model: %d\nExtended family: %d\n",
			procinfo[0], procinfo[1], procinfo[2], procinfo[3], procinfo[4]);
		fprintf(file, "\nBrand Index: %d\nCL Flush Line Size: %d\nLogical Processors: %d\nInitial APICID: %d\n\n",
			procinfo[5], procinfo[6], procinfo[7], procinfo[8]);
	}

	if (featureset != 0)
		fputs("-- CPU FEATURES --\n\n", file);

	bool
		sse3    = cpuid_has_ecxfeature(CPUID_FEAT_ECX_SSE3),
		pclmul  = cpuid_has_ecxfeature(CPUID_FEAT_ECX_PCLMUL),
		dtes64  = cpuid_has_ecxfeature(CPUID_FEAT_ECX_DTES64),
		monitor = cpuid_has_ecxfeature(CPUID_FEAT_ECX_MONITOR),
		ds_cpl  = cpuid_has_ecxfeature(CPUID_FEAT_ECX_DS_CPL),
		vmx     = cpuid_has_ecxfeature(CPUID_FEAT_ECX_VMX),
		smx     = cpuid_has_ecxfeature(CPUID_FEAT_ECX_SMX),
		est     = cpuid_has_ecxfeature(CPUID_FEAT_ECX_EST),
		tm2     = cpuid_has_ecxfeature(CPUID_FEAT_ECX_TM2),
		ssse3   = cpuid_has_ecxfeature(CPUID_FEAT_ECX_SSSE3),
		cid     = cpuid_has_ecxfeature(CPUID_FEAT_ECX_CID),
		fma     = cpuid_has_ecxfeature(CPUID_FEAT_ECX_FMA),
		cx16    = cpuid_has_ecxfeature(CPUID_FEAT_ECX_CX16),
		etprd   = cpuid_has_ecxfeature(CPUID_FEAT_ECX_ETPRD),
		pdcm    = cpuid_has_ecxfeature(CPUID_FEAT_ECX_PDCM),
		dca     = cpuid_has_ecxfeature(CPUID_FEAT_ECX_DCA),
		sse4_1  = cpuid_has_ecxfeature(CPUID_FEAT_ECX_SSE4_1),
		sse4_2  = cpuid_has_ecxfeature(CPUID_FEAT_ECX_SSE4_2),
		x2_apic = cpuid_has_ecxfeature(CPUID_FEAT_ECX_x2APIC),
		movbe   = cpuid_has_ecxfeature(CPUID_FEAT_ECX_MOVBE),
		popcnt  = cpuid_has_ecxfeature(CPUID_FEAT_ECX_POPCNT),
		aes     = cpuid_has_ecxfeature(CPUID_FEAT_ECX_AES),
		xsave   = cpuid_has_ecxfeature(CPUID_FEAT_ECX_XSAVE),
		osxsave = cpuid_has_ecxfeature(CPUID_FEAT_ECX_OSXSAVE),
		avx     = cpuid_has_ecxfeature(CPUID_FEAT_ECX_AVX);

#define YON(v)	(v) ? "Yes" : "No"
	if (featureset & CPUID_FEAT_ECX_ALL) {
		fputs("-- ECX Features --\n", file);
		fprintf(file, "SSE3:    %s\n"
			      "PCMUL:   %s\n"
			      "DTES64:  %s\n"
			      "MONITOR: %s\n"
			      "DS_CPL:  %s\n"
			      "VMX:     %s\n"
			      "SMX:     %s\n"
			      "EST:     %s\n"
			      "TM2:     %s\n"
			      "SSSE3:   %s\n"
			      "CID:     %s\n"
			      "FMA:     %s\n"
			      "CX16:    %s\n"
			      "ETPRD:   %s\n"
			      "PDCM:    %s\n"
			      "DCA:     %s\n"
			      "SSE4_1:  %s\n"
			      "SSE$_2:  %s\n"
			      "X2_APIC: %s\n"
			      "MOVBE:   %s\n"
			      "POPCNT:  %s\n"
			      "AES:     %s\n"
			      "XSAVE:   %s\n"
			      "OSXSAVE: %s\n"
			      "AVS:     %s\n\n",
			YON(sse3), YON(pclmul), YON(dtes64), YON(monitor), YON(ds_cpl),
			YON(vmx), YON(smx), YON(est), YON(tm2), YON(ssse3), YON(cid),
			YON(fma), YON(cx16), YON(etprd), YON(pdcm), YON(dca), YON(sse4_1),
			YON(sse4_2), YON(x2_apic), YON(movbe), YON(popcnt), YON(aes),
			YON(xsave), YON(osxsave), YON(avx)
		);
	} else {
		if (featureset & CPUID_FEAT_ECX_SSE3)
			fprintf(file, "SSE3:    %s\n", YON(sse3));
		if (featureset & CPUID_FEAT_ECX_PCLMUL)
			fprintf(file, "PCLMUL:    %s\n", YON(pclmul));
		if (featureset & CPUID_FEAT_ECX_DTES64)
			fprintf(file, "DTES64:    %s\n", YON(dtes64));
		if (featureset & CPUID_FEAT_ECX_MONITOR)
			fprintf(file, "Monitor:    %s\n", YON(monitor));
		if (featureset & CPUID_FEAT_ECX_DS_CPL)
			fprintf(file, "DS CPL:    %s\n", YON(ds_cpl));
		if (featureset & CPUID_FEAT_ECX_VMX)
			fprintf(file, "VMX:    %s\n", YON(vmx));
		if (featureset & CPUID_FEAT_ECX_SMX)
			fprintf(file, "SMX:    %s\n", YON(smx));
		if (featureset & CPUID_FEAT_ECX_EST)
			fprintf(file, "EST:    %s\n", YON(est));
		if (featureset & CPUID_FEAT_ECX_TM2)
			fprintf(file, "TM2:    %s\n", YON(tm2));
		if (featureset & CPUID_FEAT_ECX_SSSE3)
			fprintf(file, "SSSE3:    %s\n", YON(ssse3));
		if (featureset & CPUID_FEAT_ECX_CID)
			fprintf(file, "CID:    %s\n", YON(cid));
		if (featureset & CPUID_FEAT_ECX_FMA)
			fprintf(file, "FMA:    %s\n", YON(fma));
		if (featureset & CPUID_FEAT_ECX_CX16)
			fprintf(file, "CX16:    %s\n", YON(cx16));
		if (featureset & CPUID_FEAT_ECX_ETPRD)
			fprintf(file, "ETPRD:    %s\n", YON(etprd));
		if (featureset & CPUID_FEAT_ECX_PDCM)
			fprintf(file, "PDCM:    %s\n", YON(pdcm));
		if (featureset & CPUID_FEAT_ECX_DCA)
			fprintf(file, "DCA:    %s\n", YON(dca));
		if (featureset & CPUID_FEAT_ECX_SSE4_1)
			fprintf(file, "SSE4_1:    %s\n", YON(sse4_1));
		if (featureset & CPUID_FEAT_ECX_SSE4_2)
			fprintf(file, "SSE4_2:    %s\n", YON(sse4_2));
		if (featureset & CPUID_FEAT_ECX_x2APIC)
			fprintf(file, "x2APIC:    %s\n", YON(x2_apic));
		if (featureset & CPUID_FEAT_ECX_MOVBE)
			fprintf(file, "MOVBE:    %s\n", YON(movbe));
		if (featureset & CPUID_FEAT_ECX_POPCNT)
			fprintf(file, "POPCNT:    %s\n", YON(popcnt));
		if (featureset & CPUID_FEAT_ECX_AES)
			fprintf(file, "AES:    %s\n", YON(aes));
		if (featureset & CPUID_FEAT_ECX_XSAVE)
			fprintf(file, "XSAVE:    %s\n", YON(xsave));
		if (featureset & CPUID_FEAT_ECX_OSXSAVE)
			fprintf(file, "OSXSAVE:    %s\n", YON(osxsave));
		if (featureset & CPUID_FEAT_ECX_AVX)
			fprintf(file, "AVX:    %s\n", YON(avx));
	}

	bool
		fpu = cpuid_has_edxfeature(CPUID_FEAT_EDX_FPU),
		vme   = cpuid_has_edxfeature(CPUID_FEAT_EDX_VME),
		de    = cpuid_has_edxfeature(CPUID_FEAT_EDX_DE),
		pse   = cpuid_has_edxfeature(CPUID_FEAT_EDX_PSE),
		tsc   = cpuid_has_edxfeature(CPUID_FEAT_EDX_TSC),
		msr   = cpuid_has_edxfeature(CPUID_FEAT_EDX_MSR),
		pae   = cpuid_has_edxfeature(CPUID_FEAT_EDX_PAE),
		mce   = cpuid_has_edxfeature(CPUID_FEAT_EDX_MCE),
		cx8   = cpuid_has_edxfeature(CPUID_FEAT_EDX_CX8),
		apic  = cpuid_has_edxfeature(CPUID_FEAT_EDX_APIC),
		sep   = cpuid_has_edxfeature(CPUID_FEAT_EDX_SEP),
		mtrr  = cpuid_has_edxfeature(CPUID_FEAT_EDX_MTRR),
		pge   = cpuid_has_edxfeature(CPUID_FEAT_EDX_PGE),
		mca   = cpuid_has_edxfeature(CPUID_FEAT_EDX_MCA),
		cmov  = cpuid_has_edxfeature(CPUID_FEAT_EDX_CMOV),
		pat   = cpuid_has_edxfeature(CPUID_FEAT_EDX_PAT),
		pse36 = cpuid_has_edxfeature(CPUID_FEAT_EDX_PSE36),
		psn   = cpuid_has_edxfeature(CPUID_FEAT_EDX_PSN),
		clf   = cpuid_has_edxfeature(CPUID_FEAT_EDX_CLF),
		dtes  = cpuid_has_edxfeature(CPUID_FEAT_EDX_DTES),
		acpi  = cpuid_has_edxfeature(CPUID_FEAT_EDX_ACPI),
		mmx   = cpuid_has_edxfeature(CPUID_FEAT_EDX_MMX),
		fxsr  = cpuid_has_edxfeature(CPUID_FEAT_EDX_FXSR),
		sse   = cpuid_has_edxfeature(CPUID_FEAT_EDX_SSE),
		sse2  = cpuid_has_edxfeature(CPUID_FEAT_EDX_SSE2),
		ss    = cpuid_has_edxfeature(CPUID_FEAT_EDX_SS),
		htt   = cpuid_has_edxfeature(CPUID_FEAT_EDX_HTT),
		tm1   = cpuid_has_edxfeature(CPUID_FEAT_EDX_TM1),
		ia64  = cpuid_has_edxfeature(CPUID_FEAT_EDX_IA64),
		pbe   = cpuid_has_edxfeature(CPUID_FEAT_EDX_PBE);

	if (featureset & CPUID_FEAT_EDX_ALL) {
		fputs("-- EDX FEATURES --\n", file);
		fprintf(file, "FPU:   %s\n"
			      "VME:   %s\n"
			      "DE:    %s\n"
			      "PSE:   %s\n"
			      "TSC:   %s\n"
			      "MSR:   %s\n"
			      "PAE:   %s\n"
			      "MCE:   %s\n"
			      "CX8:   %s\n"
			      "APIC:  %s\n"
			      "SEP:   %s\n"
			      "MTRR:  %s\n"
			      "PGE:   %s\n"
			      "MCA:   %s\n"
			      "CMOV:  %s\n"
			      "PAT:   %s\n"
			      "PSE36: %s\n"
			      "PSN:   %s\n"
			      "CLF:   %s\n"
			      "DTES:  %s\n"
			      "ACPI:  %s\n"
			      "MMX:   %s\n"
			      "FXSR:  %s\n"
			      "SSE:   %s\n"
			      "SSE2:  %s\n"
			      "SS:    %s\n"
			      "HTT:   %s\n"
			      "TM1:   %s\n"
			      "IA64:  %s\n"
			      "PBE:   %s\n\n",
			YON(fpu), YON(vme), YON(de), YON(pse), YON(tsc), YON(msr),
			YON(pae), YON(mce), YON(cx8), YON(apic), YON(sep), YON(mtrr),
			YON(pge), YON(mca), YON(cmov), YON(pat), YON(pse36), YON(psn),
			YON(clf), YON(dtes), YON(acpi), YON(mmx), YON(fxsr), YON(sse),
			YON(sse2), YON(ss), YON(htt), YON(tm1), YON(ia64), YON(pbe)
		);
	} else {
		if (featureset & CPUID_FEAT_EDX_FPU)
			fprintf(file, "FPU:   %s\n", YON(fpu));
		if (featureset & CPUID_FEAT_EDX_VME)
			fprintf(file, "VME:   %s\n", YON(vme));
		if (featureset & CPUID_FEAT_EDX_DE)
			fprintf(file, "DE: %s\n", YON(de));
		if (featureset & CPUID_FEAT_EDX_PSE)
			fprintf(file, "PSE:   %s\n", YON(pse));
		if (featureset & CPUID_FEAT_EDX_TSC)
			fprintf(file, "TSC:   %s\n", YON(tsc));
		if (featureset & CPUID_FEAT_EDX_MSR)
			fprintf(file, "MSR:   %s\n", YON(msr));
		if (featureset & CPUID_FEAT_EDX_PAE)
			fprintf(file, "PAE:   %s\n", YON(pae));
		if (featureset & CPUID_FEAT_EDX_MCE)
			fprintf(file, "MCE:   %s\n", YON(mce));
		if (featureset & CPUID_FEAT_EDX_CX8)
			fprintf(file, "CX8:   %s\n", YON(cx8));
		if (featureset & CPUID_FEAT_EDX_APIC)
			fprintf(file, "APIC: %s\n", YON(apic));
		if (featureset & CPUID_FEAT_EDX_SEP)
			fprintf(file, "SEP:   %s\n", YON(sep));
		if (featureset & CPUID_FEAT_EDX_MTRR)
			fprintf(file, "MTRR: %s\n", YON(mtrr));
		if (featureset & CPUID_FEAT_EDX_PGE)
			fprintf(file, "PGE:   %s\n", YON(pge));
		if (featureset & CPUID_FEAT_EDX_MCA)
			fprintf(file, "MCA:   %s\n", YON(mca));
		if (featureset & CPUID_FEAT_EDX_CMOV)
			fprintf(file, "CMOV: %s\n", YON(cmov));
		if (featureset & CPUID_FEAT_EDX_PAT)
			fprintf(file, "PAT:   %s\n", YON(pat));
		if (featureset & CPUID_FEAT_EDX_PSE36)
			fprintf(file, "PSE36: %s\n", YON(pse36));
		if (featureset & CPUID_FEAT_EDX_PSN)
			fprintf(file, "PSN:   %s\n", YON(psn));
		if (featureset & CPUID_FEAT_EDX_CLF)
			fprintf(file, "CLF:   %s\n", YON(clf));
		if (featureset & CPUID_FEAT_EDX_DTES)
			fprintf(file, "DTES:%s\n", YON(dtes));
		if (featureset & CPUID_FEAT_EDX_ACPI)
			fprintf(file, "ACPI: %s\n", YON(acpi));
		if (featureset & CPUID_FEAT_EDX_MMX)
			fprintf(file, "MMX:   %s\n", YON(mmx));
		if (featureset & CPUID_FEAT_EDX_FXSR)
			fprintf(file, "FXSR: %s\n", YON(fxsr));
		if (featureset & CPUID_FEAT_EDX_SSE)
			fprintf(file, "SSE:   %s\n", YON(sse));
		if (featureset & CPUID_FEAT_EDX_SSE2)
			fprintf(file, "SSE2: %s\n", YON(sse2));
		if (featureset & CPUID_FEAT_EDX_SS)
			fprintf(file, "SS: %s\n", YON(ss));
		if (featureset & CPUID_FEAT_EDX_HTT)
			fprintf(file, "HTT:   %s\n", YON(htt));
		if (featureset & CPUID_FEAT_EDX_TM1)
			fprintf(file, "TM1:   %s\n", YON(tm1));
		if (featureset & CPUID_FEAT_EDX_IA64)
			fprintf(file, "IA64: %s\n", YON(ia64));
		if (featureset & CPUID_FEAT_EDX_PBE)
			fprintf(file, "PBE:   %s\n", YON(pbe));
	}
#undef YON

	return true;
}

#endif

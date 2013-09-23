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
 */
#ifndef CCAN_CPUID_H
#define CCAN_CPUID_H

/**
 * enum cpuid - stuff to get information on from the CPU.
 *
 * This is used as a parameter in cpuid().
 *
 * CPU_VENDORID:
 * 	The CPU's Vendor ID.
 *
 * CPU_PROCINFO_AND_FEATUREBITS:
 * 	Processor information and feature bits (SSE, etc.).
 *
 * CPU_CACHE_AND_TLBD_INFO
 * 	Cache and TLBD Information.
 *
 * CPU_HIGHEST_EXTENDED_FUNCTION_SUPPORTED:
 * 	Highest extended function supported address.
 * 	Can be like 0x80000008.
 *
 * CPU_EXTENDED_PROC_INFO_FEATURE_BITS:
 * 	Extended processor information and feature bits (64bit etc.)
 *
 * CPU_PROC_BRAND_STRING:
 * 	The Processor's brand string.
 *
 * CPU_L1_CACHE_AND_TLB_IDS:
 * 	L1 Cache and TLB Identifications.
 *
 * CPU_EXTENDED_L2_CACHE_FEATURES:
 * 	Extended L2 Cache features.
 *
 * CPU_ADV_POWER_MGT_INFO:
 * 	Advaned power management information.
 *
 * CPU_VIRT_PHYS_ADDR_SIZES:
 * 	Virtual and physical address sizes.
 */

typedef enum cpuid {
	CPU_VENDORID 					= 0,
	CPU_PROCINFO_AND_FEATUREBITS 			= 1,
	CPU_CACHE_AND_TLBD_INFO 			= 2,

	CPU_HIGHEST_EXTENDED_FUNCTION_SUPPORTED 	= 0x80000000,
	CPU_EXTENDED_PROC_INFO_FEATURE_BITS 		= 0x80000001,
	CPU_PROC_BRAND_STRING 				= 0x80000002, 
	CPU_L1_CACHE_AND_TLB_IDS 			= 0x80000005,
	CPU_EXTENDED_L2_CACHE_FEATURES 			= 0x80000006,
	CPU_ADV_POWER_MGT_INFO 				= 0x80000007,
	CPU_VIRT_PHYS_ADDR_SIZES 			= 0x80000008
} cpuid_t;

#define CF_MMX 		0
#define CF_SSE 		1
#define CF_SSE2 	2
#define CF_SSE3 	3
#define CF_FPU 		4
#define CF_TSC 		5
#define CF_MSR 		6
#define CF_SSSE3 	7
#define CF_AVX 		8
#define CF_FMA 		9

#define CEF_x64 	10
#define CEF_FPU 	11
#define CEF_DE 		12
#define CEF_SYSCALLRET 	13
#define CEF_CMOV 	14
#define CEF_SSE4a 	15
#define CEF_FMA4 	16
#define CEF_XOP 	17

typedef enum cputype {
	CT_NONE,
	CT_AMDK5,
	CT_AMD,
	CT_CENTAUR,
	CT_CYRIX,
	CT_INTEL,
	CT_TRANSMETA,
	CT_NATIONAL_SEMICONDUCTOR,
	CT_NEXGEN,
	CT_RISE,
	CT_SIS,
	CT_UMC,
	CT_VIA,
	CT_VORTEX,
	CT_KVM
} cputype_t;

/**
 * cpuid_get_cpu_type - Get CPU Type
 *
 * Returns the CPU Type as cputype_t.
 *
 * See also: cpuid_get_cpu_type_string()
 */
cputype_t cpuid_get_cpu_type(void);

/**
 * cpuid_get_cpu_type_string - Get CPU Type string
 *
 * Returns the CPU type string based off cputype_t.
 */
const char *cpuid_get_cpu_type_string(const cputype_t cputype);

/**
 * cpuid_is_supported - test if the CPUID instruction is supported
 *
 * CPUID is not supported by old CPUS.
 *
 * Returns 1 if the cpuid instruction is supported, 0 otherwise.
 *
 * See also: cpuid()
 */
int cpuid_is_supported(void);

/**
 * cpuid_highest_ext_func_supported - Get the highest extended function supported
 *
 *
 * Returns the highest extended function supported.
 *
 * This is the same as calling:
 * 	cpuid(CPU_HIGHEST_EEXTENDED_FUNCTION_SUPPORTED, &highest);
 *
 * This is made visible to the linker because it's easier to call it
 * instead of calling cpuid with less type-checking.  cpuid calls this.
 *
 * See also: cpuid()
 */
int cpuid_highest_ext_func_supported(void);

/**
 * cpuid - Get Some information from the CPU.
 *
 * This function expects buf to be a valid pointer to a string/int/...
 * depending on the requested information.
 *
 * For CPU_VENDOR_ID:
 * 	Returns a string into buf.
 *
 * For CPU_PROCINFO_AND_FEATUREBITS:
 * 	buf[0]:
 * 		- 3:0 - Stepping
 * 		- 7:4 - Model
 * 		- 11:8 - Family
 * 		- 13:12 - Processor Type
 * 		- 19:16 - Extended Model
 * 		- 27:20 - Extended family
 * 	buf[1] and buf[2]:
 * 		Feature flags
 * 	buf[3]:
 * 		Additional feature information.
 *
 * For CPU_HIGHEST_EXTENDED_FUNCTION_SUPPORTED:
 * 	Returns the highest supported function in *buf (expects an integer ofc)
 *
 * For CPU_EXTENDED_PROC_INFO_FEATURE_BITS:
 * 	Returns them in buf[0] and buf[1].
 *
 * For CPU_VIRT_PHYS_ADDR_SIZES:
 * 	Returns it as an integer in *buf.
 *
 * For CPU_PROC_BRAND_STRING:
 * 	Have a char array with at least 48 bytes assigned to it.
 *
 * If an invalid flag has been passed a 0xbaadf00d is returned in *buf.
 */
void cpuid(cpuid_t info, void *buf);

/**
 * cpuid_test_feature - Test if @feature is available
 *
 * Returns 1 if feature is supported, 0 otherwise.
 *
 * The feature parameter must be >= CPU_EXTENDED_PROC_INFO_FEATURE_BITS
 *  and <= CPU_VIRT_PHYS_ADDR_SIZES.
 */
int cpuid_test_feature(cpuid_t feature);

/**
 * cpuid_has_feature - Test if @feature is supported
 *
 * Test if the CPU supports MMX/SSE* etc.
 * For the extended parameter, usually you want to pass it as
 * 0 if you're not passing CEF_*.
 *
 * For more information about the CPU extended features, have a look
 * at:
 * 	http://en.wikipedia.org/wiki/CPUID
 *
 * Returns 1 if the feature is available, 0 otherwise.
 */
#define cpuid_has_mmx() 	cpuid_has_feature(CF_MMX, 	0)
#define cpuid_has_sse() 	cpuid_has_feature(CF_SSE, 	0)
#define cpuid_has_sse2() 	cpuid_has_feature(CF_SSE2, 	0)
#define cpuid_has_sse3() 	cpuid_has_feature(CF_SSE3, 	0)
#define cpuid_has_ssse3() 	cpuid_has_feature(CF_SSSE3, 	0)
#define cpuid_has_avx() 	cpuid_has_feature(CF_AVX, 	0)
#define cpuid_has_fma() 	cpuid_has_feature(CF_FMA, 	0)
#define cpuid_has_x64() 	cpuid_has_feature(CEF_x64, 	1)
#define cpuid_has_sse4a() 	cpuid_has_feature(CEF_SSE4a, 	1)
#define cpuid_has_fma4() 	cpuid_has_feature(CEF_FMA4, 	1)
#define cpuid_has_xop() 	cpuid_has_feature(CEF_XOP, 	1)
int cpuid_has_feature(int feature, int extended);

#endif


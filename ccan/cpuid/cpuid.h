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

#include <stdbool.h>
#include <stdint.h>

/**
 * enum cpuid - stuff to get information about from the CPU.
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

#if defined(__i386__) || defined(__i386) || defined(__x86_64) \
	|| defined(_M_AMD64) || defined(__M_X64)

/**
 * cpuid_get_cpu_type - Get CPU Type
 *
 * Returns the CPU Type as cputype_t.
 *
 * See also: cpuid_get_cpu_type_string()
 */
#define is_intel_cpu() 	cpuid_get_cpu_type() == CT_INTEL
#define is_amd_cpu() 	cpuid_get_cpu_type() == CT_AMDK5 || cpuid_get_cpu_type() == CT_AMD
cputype_t cpuid_get_cpu_type(void);

/**
 * cpuid_sprintf_cputype - Get CPU Type string
 * @cputype: a char of atleast 12 bytes in it.
 *
 * Returns true on success, false on failure
 */
bool cpuid_sprintf_cputype(const cputype_t cputype, char *buf);

/**
 * cpuid_is_supported - test if the CPUID instruction is supported
 *
 * CPUID is not supported by old CPUS.
 *
 * Returns true if the cpuid instruction is supported, false otherwise.
 *
 * See also: cpuid()
 */
bool cpuid_is_supported(void);

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
uint32_t cpuid_highest_ext_func_supported(void);

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
 * 	buf[0]: Stepping
 * 	buf[1]: Model
 * 	buf[2]: Family
 * 	buf[3]: Extended Model
 * 	buf[4]: Extended Family
 * 	buf[5] and buf[6]:
 * 		Feature flags
 * 	buf[7]: Brand Index
 * 	buf[8]: CL Flush Line Size
 * 	buf[9]: Logical Processors
 * 	buf[10]: Initial APICID
 *
 * For CPU_L1_CACHE_AND_TLB_IDS:
 * 	buf[0]: (eax):
 * 		- 7..0 	Number of times to exec cpuid to get all descriptors.
 * 		- 15..8 Instruction TLB: 4K Pages, 4-way set associtive, 128 entries.
 * 		- 23..16 Data TLB: 4k Pages, 4-way set associtive, 128 entries.
 * 		- 24..31 Instruction TLB: 4K Pages, 4-way set associtive, 2 entries.
 * 	buf[1]: (ebx):
 * 		- 7..0 64-byte prefetching
 * 		- 8..31 Null descriptor
 * 	buf[2]: (ecx):
 * 		- 0..31 Null descriptor
 * 	buf[3]: (edx):
 * 		- 7..0 2nd-level cache, 2M, 8-way set associtive, 64-byte line size
 * 		- 15..8 1st-level instruction cache: 32K, 8-way set associtive, 64 byte line size
 * 		- 16..23 Data TLB: 4M Pages, 4-way set associtive, 8 entires.
 * 		- 24..31 1st-level data cache: 32K, 8-way set associtive, 64 byte line size
 *
 * For CPU_HIGHEST_EXTENDED_FUNCTION_SUPPORTED:
 * 	Returns the highest supported function in *buf (expects an integer ofc)
 *
 * For CPU_EXTENDED_PROC_INFO_FEATURE_BITS:
 * 	Returns them in buf[0] and buf[1].
 *
 * For CPU_EXTENDED_L2_CACHE_FEATURES:
 * 	buf[0]: Line size
 * 	buf[1]: Associativity
 * 	buf[2]: Cache size.
 *
 * For CPU_VIRT_PHYS_ADDR_SIZES:
 * 	buf[0]: Physical
 * 	buf[1]: Virtual
 *
 * For CPU_PROC_BRAND_STRING:
 * 	Have a char array with at least 48 bytes assigned to it.
 *
 * Here's a page which will help you parse the data provided by this function.
 * 	http://www.flounder.com/cpuid_explorer2.htm
 *
 * If an invalid flag has been passed a 0xbaadf00d is returned in *buf.
 */
void cpuid(cpuid_t info, uint32_t *buf);

/**
 * cpuid_test_feature - Test if @feature is available
 *
 * Returns true if feature is supported, false otherwise.
 *
 * The feature parameter must be >= CPU_EXTENDED_PROC_INFO_FEATURE_BITS
 *  and <= CPU_VIRT_PHYS_ADDR_SIZES.
 */
bool cpuid_test_feature(cpuid_t feature);

/**
 * cpuid_has_feature - Test if @feature is supported
 *
 * Test if the CPU supports MMX/SSE* etc.
 * For the extended parameter, usually you want to pass it as
 * false if you're not passing CEF_*.
 *
 * For more information about the CPU extended features, have a look
 * at:
 * 	http://en.wikipedia.org/wiki/CPUID
 *
 * Returns true if the feature is available, false otherwise.
 */
#define cpuid_has_mmx() 	cpuid_has_feature(CF_MMX, 	false)
#define cpuid_has_sse() 	cpuid_has_feature(CF_SSE, 	false)
#define cpuid_has_sse2() 	cpuid_has_feature(CF_SSE2, 	false)
#define cpuid_has_sse3() 	cpuid_has_feature(CF_SSE3, 	false)
#define cpuid_has_ssse3() 	cpuid_has_feature(CF_SSSE3, 	false)
#define cpuid_has_avx() 	cpuid_has_feature(CF_AVX, 	false)
#define cpuid_has_fma() 	cpuid_has_feature(CF_FMA, 	false)
#define cpuid_has_x64() 	cpuid_has_feature(CEF_x64, 	true)
#define cpuid_has_sse4a() 	cpuid_has_feature(CEF_SSE4a, 	true)
#define cpuid_has_fma4() 	cpuid_has_feature(CEF_FMA4, 	true)
#define cpuid_has_xop() 	cpuid_has_feature(CEF_XOP, 	true)
bool cpuid_has_feature(int feature, bool extended);

#else
#include <ccan/build_assert/build_assert.h>

#define cpuid_get_cpu_type() 			BUILD_ASSERT_OR_ZERO(0)
#define cpuid_get_cpu_type_string() 		BUILD_ASSERT_OR_ZERO(0)

#define cpuid_is_supported() 			BUILD_ASSERT_OR_ZERO(0)
#define cpuid(info, buf) 			BUILD_ASSERT_OR_ZERO(0)

#define cpuid_highest_ext_func_supported() 	BUILD_ASSERT_OR_ZERO(0)
#define cpuid_test_feature(feature) 		BUILD_ASSERT_OR_ZERO(0)
#define cpuid_has_feature(feature, ext) 	BUILD_ASSERT_OR_ZERO(0)

#endif
#endif

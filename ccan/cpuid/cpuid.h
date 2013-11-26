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
 * %CPUID_VENDORID:
 * 	The CPU's Vendor ID.
 *
 * %CPUID_PROCINFO_AND_FEATUREBITS:
 * 	Processor information and feature bits (SSE, etc.).
 *
 * %CPUID_CACHE_AND_TLBD_INFO
 * 	Cache and TLBD Information.
 * 	For AMD: Use CPUID_EXTENDED_L2_CACHE_FEATURES
 *
 * %CPUID_HIGHEST_EXTENDED_FUNCTION_SUPPORTED:
 * 	Highest extended function supported address.
 * 	Can be like 0x80000008.
 *
 * %CPUID_EXTENDED_PROC_INFO_FEATURE_BITS:
 * 	Extended processor information and feature bits (64bit etc.)
 *
 * %CPUID_PROC_BRAND_STRING:
 * 	The Processor's brand string.
 *
 * %CPUID_L1_CACHE_AND_TLB_IDS:
 * 	L1 Cache and TLB Identifications.
 *	AMD Only.
 *
 * %CPUID_EXTENDED_L2_CACHE_FEATURES:
 * 	Extended L2 Cache features.
 *
 * %CPUID_ADV_POWER_MGT_INFO:
 * 	Advaned power management information.
 *
 * %CPUID_VIRT_PHYS_ADDR_SIZES:
 * 	Virtual and physical address sizes.
 */

typedef enum cpuid {
	CPUID_VENDORID 					= 0,
	CPUID_PROCINFO_AND_FEATUREBITS 			= 1,
	CPUID_CACHE_AND_TLBD_INFO 			= 2,

	CPUID_HIGHEST_EXTENDED_FUNCTION_SUPPORTED 	= 0x80000000,
	CPUID_EXTENDED_PROC_INFO_FEATURE_BITS 		= 0x80000001,
	CPUID_PROC_BRAND_STRING 			= 0x80000002,
	CPUID_L1_CACHE_AND_TLB_IDS 			= 0x80000005,
	CPUID_EXTENDED_L2_CACHE_FEATURES 		= 0x80000006,
	CPUID_ADV_POWER_MGT_INFO 			= 0x80000007,
	CPUID_VIRT_PHYS_ADDR_SIZES 			= 0x80000008
} cpuid_t;

enum {
	CPUID_FEAT_ECX_SSE3         = 1 << 0, 
	CPUID_FEAT_ECX_PCLMUL       = 1 << 1,
	CPUID_FEAT_ECX_DTES64       = 1 << 2,
	CPUID_FEAT_ECX_MONITOR      = 1 << 3,  
	CPUID_FEAT_ECX_DS_CPL       = 1 << 4,  
	CPUID_FEAT_ECX_VMX          = 1 << 5,  
	CPUID_FEAT_ECX_SMX          = 1 << 6,  
	CPUID_FEAT_ECX_EST          = 1 << 7,  
	CPUID_FEAT_ECX_TM2          = 1 << 8,  
	CPUID_FEAT_ECX_SSSE3        = 1 << 9,  
	CPUID_FEAT_ECX_CID          = 1 << 10,
	CPUID_FEAT_ECX_FMA          = 1 << 12,
	CPUID_FEAT_ECX_CX16         = 1 << 13, 
	CPUID_FEAT_ECX_ETPRD        = 1 << 14, 
	CPUID_FEAT_ECX_PDCM         = 1 << 15, 
	CPUID_FEAT_ECX_DCA          = 1 << 18, 
	CPUID_FEAT_ECX_SSE4_1       = 1 << 19, 
	CPUID_FEAT_ECX_SSE4_2       = 1 << 20, 
	CPUID_FEAT_ECX_x2APIC       = 1 << 21, 
	CPUID_FEAT_ECX_MOVBE        = 1 << 22, 
	CPUID_FEAT_ECX_POPCNT       = 1 << 23, 
	CPUID_FEAT_ECX_AES          = 1 << 25, 
	CPUID_FEAT_ECX_XSAVE        = 1 << 26, 
	CPUID_FEAT_ECX_OSXSAVE      = 1 << 27, 
	CPUID_FEAT_ECX_AVX          = 1 << 28,

	CPUID_FEAT_EDX_FPU          = 1 << 0,  
	CPUID_FEAT_EDX_VME          = 1 << 1,  
	CPUID_FEAT_EDX_DE           = 1 << 2,  
	CPUID_FEAT_EDX_PSE          = 1 << 3,  
	CPUID_FEAT_EDX_TSC          = 1 << 4,  
	CPUID_FEAT_EDX_MSR          = 1 << 5,  
	CPUID_FEAT_EDX_PAE          = 1 << 6,  
	CPUID_FEAT_EDX_MCE          = 1 << 7,  
	CPUID_FEAT_EDX_CX8          = 1 << 8,  
	CPUID_FEAT_EDX_APIC         = 1 << 9,  
	CPUID_FEAT_EDX_SEP          = 1 << 11, 
	CPUID_FEAT_EDX_MTRR         = 1 << 12, 
	CPUID_FEAT_EDX_PGE          = 1 << 13, 
	CPUID_FEAT_EDX_MCA          = 1 << 14, 
	CPUID_FEAT_EDX_CMOV         = 1 << 15, 
	CPUID_FEAT_EDX_PAT          = 1 << 16, 
	CPUID_FEAT_EDX_PSE36        = 1 << 17, 
	CPUID_FEAT_EDX_PSN          = 1 << 18, 
	CPUID_FEAT_EDX_CLF          = 1 << 19, 
	CPUID_FEAT_EDX_DTES         = 1 << 21, 
	CPUID_FEAT_EDX_ACPI         = 1 << 22, 
	CPUID_FEAT_EDX_MMX          = 1 << 23, 
	CPUID_FEAT_EDX_FXSR         = 1 << 24, 
	CPUID_FEAT_EDX_SSE          = 1 << 25, 
	CPUID_FEAT_EDX_SSE2         = 1 << 26, 
	CPUID_FEAT_EDX_SS           = 1 << 27, 
	CPUID_FEAT_EDX_HTT          = 1 << 28, 
	CPUID_FEAT_EDX_TM1          = 1 << 29, 
	CPUID_FEAT_EDX_IA64         = 1 << 30,
	CPUID_FEAT_EDX_PBE          = 1 << 31
};

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
 * 	cpuid(CPUID_HIGHEST_EEXTENDED_FUNCTION_SUPPORTED, &highest);
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
 * For CPUID_VENDOR_ID:
 * 	Returns a string into buf.
 *
 * For CPUID_PROCINFO_AND_FEATUREBITS:
 * 	buf[0]: Stepping
 * 	buf[1]: Model
 * 	buf[2]: Family
 * 	buf[3]: Extended Model
 * 	buf[4]: Extended Family
 * 	buf[5]: Brand Index
 * 	buf[6]: CL Flush Line Size
 * 	buf[7]: Logical Processors
 * 	buf[8]: Initial APICID
 *
 * For CPUID_L1_CACHE_AND_TLB_IDS:
 *	buf[0] to buf[3]: 2M+4M page TLB info
 * 		0: Inst count
 * 		1: Inst Assoc
 * 		2: Data Count
 * 		3: Data Assoc
 * 	buf[4] to buf[7]: 4k page TLB info
 * 		0: Inst count
 * 		1: Inst Assoc
 * 		2: Data Count
 * 		3: Data Assoc
 * 	buf[8] to buf[11]: L1 data cache information
 *		0: Line Size
 * 		1: LinesPerTag
 * 		2: Associativity
 * 		3: CacheSize
 * 	buf[12] to buf[15]: L1 instruction cache info
 * 		0: Line Size
 * 		1: LinesPerTag
 * 		2: Associativity
 * 		3: CacheSize
 *
 * For CPUID_HIGHEST_EXTENDED_FUNCTION_SUPPORTED:
 * 	Returns the highest supported function in *buf (expects an integer ofc)
 *
 * For CPUID_EXTENDED_PROC_INFO_FEATURE_BITS:
 * 	Returns them in buf[0] and buf[1].
 *
 * For CPUID_EXTENDED_L2_CACHE_FEATURES:
 * 	buf[0]: Line size
 * 	buf[1]: Associativity
 * 	buf[2]: Cache size.
 *
 * For CPUID_VIRT_PHYS_ADDR_SIZES:
 * 	buf[0]: Physical
 * 	buf[1]: Virtual
 *
 * For CPUID_PROC_BRAND_STRING:
 * 	Have a char array with at least 48 bytes assigned to it.
 *
 * If an invalid flag has been passed a 0xbaadf00d is returned in *buf.
 */
void cpuid(cpuid_t info, uint32_t *buf);

/**
 * cpuid_test_feature - Test if @feature is available
 *
 * Returns true if feature is supported, false otherwise.
 *
 * The feature parameter must be >= CPUID_EXTENDED_PROC_INFO_FEATURE_BITS
 *  and <= CPUID_VIRT_PHYS_ADDR_SIZES.
 */
bool cpuid_test_feature(cpuid_t feature);

/**
 * cpuid_has_feature - Test if @feature is supported
 *
 * Test if the CPU supports MMX/SSE* etc.
 * Use cpuid_has_ecxfeature() for *_ECX* features and
 * cpuid_has_edxfeature() for *_EDX* features.
 *
 * Returns true if the feature is available, false otherwise.
 */
bool cpuid_has_ecxfeature(int feature);
bool cpuid_has_edxfeature(int feature);

#else
#include <ccan/build_assert/build_assert.h>

#define cpuid_get_cpu_type() 			BUILD_ASSERT_OR_ZERO(0)
#define cpuid_get_cpu_type_string() 		BUILD_ASSERT_OR_ZERO(0)

#define cpuid_is_supported() 			BUILD_ASSERT_OR_ZERO(0)
#define cpuid(info, buf) 			BUILD_ASSERT_OR_ZERO(0)

#define cpuid_highest_ext_func_supported() 	BUILD_ASSERT_OR_ZERO(0)
#define cpuid_test_feature(feature) 		BUILD_ASSERT_OR_ZERO(0)
#define cpuid_has_ecxfeature(feature) 		BUILD_ASSERT_OR_ZERO(0)
#define cpuid_has_edxfeature(feature) 		BUILD_ASSERT_OR_ZERO(0)

#endif
#endif

/*
 * Copyright (c) 2013, 2015 Ahmed Samy  <f.fallen45@gmail.com>
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
#include <stdio.h>

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

	CPUID_FEAT_ECX_ALL 	    = CPUID_FEAT_ECX_SSE3 | CPUID_FEAT_ECX_PCLMUL | CPUID_FEAT_ECX_DTES64
					| CPUID_FEAT_ECX_MONITOR | CPUID_FEAT_ECX_DS_CPL | CPUID_FEAT_ECX_VMX
					| CPUID_FEAT_ECX_SMX | CPUID_FEAT_ECX_EST | CPUID_FEAT_ECX_TM2
					| CPUID_FEAT_ECX_SSSE3 | CPUID_FEAT_ECX_CID | CPUID_FEAT_ECX_FMA
					| CPUID_FEAT_ECX_CX16 | CPUID_FEAT_ECX_ETPRD | CPUID_FEAT_ECX_PDCM
					| CPUID_FEAT_ECX_DCA | CPUID_FEAT_ECX_SSE4_1 | CPUID_FEAT_ECX_SSE4_2
					| CPUID_FEAT_ECX_x2APIC | CPUID_FEAT_ECX_MOVBE | CPUID_FEAT_ECX_POPCNT
					| CPUID_FEAT_ECX_AES | CPUID_FEAT_ECX_XSAVE | CPUID_FEAT_ECX_OSXSAVE
					| CPUID_FEAT_ECX_AVX,

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
	CPUID_FEAT_EDX_PBE          = 1 << 31,

	CPUID_FEAT_EDX_ALL 	    = CPUID_FEAT_EDX_FPU | CPUID_FEAT_EDX_VME | CPUID_FEAT_EDX_DE
					| CPUID_FEAT_EDX_PSE | CPUID_FEAT_EDX_TSC | CPUID_FEAT_EDX_MSR
					| CPUID_FEAT_EDX_PAE | CPUID_FEAT_EDX_MCE | CPUID_FEAT_EDX_CX8
					| CPUID_FEAT_EDX_APIC | CPUID_FEAT_EDX_SEP | CPUID_FEAT_EDX_MTRR
					| CPUID_FEAT_EDX_PGE | CPUID_FEAT_EDX_MCA | CPUID_FEAT_EDX_CMOV
					| CPUID_FEAT_EDX_PAT | CPUID_FEAT_EDX_PSE36 | CPUID_FEAT_EDX_PSN
					| CPUID_FEAT_EDX_CLF | CPUID_FEAT_EDX_DTES | CPUID_FEAT_EDX_ACPI
					| CPUID_FEAT_EDX_MMX | CPUID_FEAT_EDX_FXSR | CPUID_FEAT_EDX_SSE
					| CPUID_FEAT_EDX_SSE2 | CPUID_FEAT_EDX_SS | CPUID_FEAT_EDX_HTT
					| CPUID_FEAT_EDX_TM1 | CPUID_FEAT_EDX_IA64 | CPUID_FEAT_EDX_PBE
};

enum {
	CPUID_EXTFEAT_ECX_LAHF_LM 		= 1 << 0,
	CPUID_EXTFEAT_ECX_CMP_LEGACY 		= 1 << 1,
	CPUID_EXTFEAT_ECX_SVM 			= 1 << 2,
	CPUID_EXTFEAT_ECX_EXTAPIC 		= 1 << 3,
	CPUID_EXTFEAT_ECX_CR8_LEGACY 		= 1 << 4,
	CPUID_EXTFEAT_ECX_ABM 			= 1 << 5,
	CPUID_EXTFEAT_ECX_SSE4A 		= 1 << 6,
	CPUID_EXTFEAT_ECX_MISALIGNSSE 		= 1 << 7,
	CPUID_EXTFEAT_ECX_3DNOWPREFETCH 	= 1 << 8,
	CPUID_EXTFEAT_ECX_OSVW 			= 1 << 9,
	CPUID_EXTFEAT_ECX_IBS 			= 1 << 10,
	CPUID_EXTFEAT_ECX_XOP			= 1 << 11,
	CPUID_EXTFEAT_ECX_SKINIT 		= 1 << 12,
	CPUID_EXTFEAT_ECX_WDT 			= 1 << 13,
	CPUID_EXTFEAT_ECX_LWP 			= 1 << 15,
	CPUID_EXTFEAT_ECX_FMA4 			= 1 << 16,
	CPUID_EXTFEAT_ECX_TCE 			= 1 << 17,
	CPUID_EXTFEAT_ECX_NODEIDE_MSR 		= 1 << 19,
	CPUID_EXTFEAT_ECX_TBM 			= 1 << 21,
	CPUID_EXTFEAT_ECX_TOPOEXT 		= 1 << 22,
	CPUID_EXTFEAT_ECX_PERFXTR_CORE 		= 1 << 23,
	CPUID_EXTFEAT_ECX_PERFCTR_NB 		= 1 << 24,

	CPUID_EXTFEAT_EDX_FPU 			= 1 << 0,
	CPUID_EXTFEAT_EDX_VME 			= 1 << 1,
	CPUID_EXTFEAT_EDX_DE 			= 1 << 2,
	CPUID_EXTFEAT_EDX_PSE 			= 1 << 3,
	CPUID_EXTFEAT_EDX_TSC 			= 1 << 4,
	CPUID_EXTFEAT_EDX_MSR 			= 1 << 5,
	CPUID_EXTFEAT_EDX_PAE 			= 1 << 6,
	CPUID_EXTFEAT_EDX_MCE 			= 1 << 7,
	CPUID_EXTFEAT_EDX_CX8 			= 1 << 8,
	CPUID_EXTFEAT_EDX_APIC 			= 1 << 9,
	CPUID_EXTFEAT_EDX_SYSCALL 		= 1 << 11,
	CPUID_EXTFEAT_EDX_MTRR 			= 1 << 12,
	CPUID_EXTFEAT_EDX_PGE 			= 1 << 13,
	CPUID_EXTFEAT_EDX_MCA 			= 1 << 14,
	CPUID_EXTFEAT_EDX_CMOV 			= 1 << 15,
	CPUID_EXTFEAT_EDX_PAT 			= 1 << 16,
	CPUID_EXTFEAT_EDX_PSE36 		= 1 << 17,
	CPUID_EXTFEAT_EDX_MP 			= 1 << 19,
	CPUID_EXTFEAT_EDX_NX 			= 1 << 20,
	CPUID_EXTFEAT_EDX_MMXEXT 		= 1 << 22,
	CPUID_EXTFEAT_EDX_MMX 			= 1 << 23,
	CPUID_EXTFEAT_EDX_FXSR 			= 1 << 24,
	CPUID_EXTFEAT_EDX_FXSR_OPT 		= 1 << 25,
	CPUID_EXTFEAT_EDX_PDPE1GB 		= 1 << 26,
	CPUID_EXTFEAT_EDX_RDTSCP		= 1 << 27,
	CPUID_EXTFEAT_EDX_LM 			= 1 << 29,
	CPUID_EXTFEAT_EDX_3DNOWEXT 		= 1 << 30,
	CPUID_EXTFEAT_EDX_3DNOW 		= 1 << 31
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

static char const *const c_cpunames[] = {
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

#if defined(__i386__) || defined(__i386) || defined(__x86_64) \
	|| defined(_M_AMD64) || defined(__M_X64)

/**
 * cpuid_get_cpu_type - Get CPU Type
 *
 * Returns the CPU Type as cputype_t.
 *
 * See also: cpuid_get_name()
 */
cputype_t cpuid_get_cpu_type(void);

static inline bool cpuid_is_intel(void)
{
	return cpuid_get_cpu_type() == CT_INTEL;
}

static inline bool cpuid_is_amd(void)
{
	return cpuid_get_cpu_type() == CT_AMDK5 || cpuid_get_cpu_type() == CT_AMD;
}

static inline const char *cpuid_get_name(void)
{
	return c_cpunames[(int)cpuid_get_cpu_type()];
}

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
 * @request: a cpuid_t
 * @buf: output
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
 * If an invalid request has been passed a 0xbaadf00d is returned in *buf.
 */
void cpuid(cpuid_t request, uint32_t *buf);

/**
 * cpuid_write_info - Write specified CPU information to a file.
 * @info: Bit set of information to write.
 * @featureset: Bit set of features to write.
 * @outfile: Output file pointer
 *
 * Returns true on success, false otherwise.
 *
 * Example usage:
 * 	if (!cpuid_write_info(CPUID_VENDORID | CPUID_PROC_BRAND_STRING,
 * 				CPUID_FEAT_ECX_SSE3 | CPUID_FEAT_EDX_FPU,
 * 				fp))
 * 		... error ...
 */
bool cpuid_write_info(uint32_t info, uint32_t featureset, FILE *outfile);

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
 * This is split into two parts:
 * 	cpuid_has_ecxfeature and
 * 	cpuid_has_edxfeature.
 * See the enum for more information.
 *
 * Returns true if the feature is available, false otherwise.
 */
bool cpuid_has_ecxfeature(int feature);
bool cpuid_has_edxfeature(int feature);

/**
 * cpuid_has_extfeature - Test if @extfeature is supported
 * @extfeature: the extended feature to test.
 *
 * This is split into two parts:
 * 	cpuid_has_ecxfeature_ext and
 * 	cpuid_has_edxfeature_ext.
 * See the enum for more information.
 *
 * Test if the CPU supports this extfeature.
 * Returns true on success, false otherwise.
 */
bool cpuid_has_ecxfeature_ext(int extfeature);
bool cpuid_has_edxfeature_ext(int extfeature);

#else
#include <ccan/build_assert/build_assert.h>

#define cpuid_get_cpu_type() 				BUILD_ASSERT_OR_ZERO(0)
#define cpuid_is_intel()				BUILD_ASSERT_OR_ZERO(0)
#define cpuid_is_amd()					BUILD_ASSERT_OR_ZERO(0)
#define cpuid_get_name()				BUILD_ASSERT_OR_ZERO(0)

#define cpuid_is_supported() 				BUILD_ASSERT_OR_ZERO(0)
#define cpuid(request, buf) 				BUILD_ASSERT_OR_ZERO(0)
#define cpuid_write_info(info, featureset, outfile)	BUILD_ASSERT_OR_ZERO(0)

#define cpuid_highest_ext_func_supported() 		BUILD_ASSERT_OR_ZERO(0)
#define cpuid_test_feature(feature) 			BUILD_ASSERT_OR_ZERO(0)
#define cpuid_has_ecxfeature(feature) 			BUILD_ASSERT_OR_ZERO(0)
#define cpuid_has_edxfeature(feature) 			BUILD_ASSERT_OR_ZERO(0)

#endif
#endif

/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_ENDIAN_H
#define CCAN_ENDIAN_H
#include <stdint.h>
#include "config.h"

#if HAVE_BYTESWAP_H
#include <byteswap.h>
#else
/**
 * bswap_16 - reverse bytes in a uint16_t value.
 * @val: value whose bytes to swap.
 *
 * Example:
 *	// Output contains "1024 is 4 as two bytes reversed"
 *	printf("1024 is %u as two bytes reversed\n", bswap_16(1024));
 */
static inline uint16_t bswap_16(uint16_t val)
{
	return ((val & (uint16_t)0x00ffU) << 8)
		| ((val & (uint16_t)0xff00U) >> 8);
}

/**
 * bswap_32 - reverse bytes in a uint32_t value.
 * @val: value whose bytes to swap.
 *
 * Example:
 *	// Output contains "1024 is 262144 as four bytes reversed"
 *	printf("1024 is %u as four bytes reversed\n", bswap_32(1024));
 */
static inline uint32_t bswap_32(uint32_t val)
{
	return ((val & (uint32_t)0x000000ffUL) << 24)
		| ((val & (uint32_t)0x0000ff00UL) <<  8)
		| ((val & (uint32_t)0x00ff0000UL) >>  8)
		| ((val & (uint32_t)0xff000000UL) >> 24);
}
#endif /* !HAVE_BYTESWAP_H */

#if !HAVE_BSWAP_64
/**
 * bswap_64 - reverse bytes in a uint64_t value.
 * @val: value whose bytes to swap.
 *
 * Example:
 *	// Output contains "1024 is 1125899906842624 as eight bytes reversed"
 *	printf("1024 is %llu as eight bytes reversed\n",
 *		(unsigned long long)bswap_64(1024));
 */
static inline uint64_t bswap_64(uint64_t val)
{
	return ((val & (uint64_t)0x00000000000000ffULL) << 56)
		| ((val & (uint64_t)0x000000000000ff00ULL) << 40)
		| ((val & (uint64_t)0x0000000000ff0000ULL) << 24)
		| ((val & (uint64_t)0x00000000ff000000ULL) <<  8)
		| ((val & (uint64_t)0x000000ff00000000ULL) >>  8)
		| ((val & (uint64_t)0x0000ff0000000000ULL) >> 24)
		| ((val & (uint64_t)0x00ff000000000000ULL) >> 40)
		| ((val & (uint64_t)0xff00000000000000ULL) >> 56);
}
#endif

/* Sanity check the defines.  We don't handle weird endianness. */
#if !HAVE_LITTLE_ENDIAN && !HAVE_BIG_ENDIAN
#error "Unknown endian"
#elif HAVE_LITTLE_ENDIAN && HAVE_BIG_ENDIAN
#error "Can't compile for both big and little endian."
#endif

/**
 * cpu_to_le64 - convert a uint64_t value to little-endian
 * @native: value to convert
 */
static inline uint64_t cpu_to_le64(uint64_t native)
{
#if HAVE_LITTLE_ENDIAN
	return native;
#else
	return bswap_64(native);
#endif
}

/**
 * cpu_to_le32 - convert a uint32_t value to little-endian
 * @native: value to convert
 */
static inline uint32_t cpu_to_le32(uint32_t native)
{
#if HAVE_LITTLE_ENDIAN
	return native;
#else
	return bswap_32(native);
#endif
}

/**
 * cpu_to_le16 - convert a uint16_t value to little-endian
 * @native: value to convert
 */
static inline uint16_t cpu_to_le16(uint16_t native)
{
#if HAVE_LITTLE_ENDIAN
	return native;
#else
	return bswap_16(native);
#endif
}

/**
 * le64_to_cpu - convert a little-endian uint64_t value
 * @le_val: little-endian value to convert
 */
static inline uint64_t le64_to_cpu(uint64_t le_val)
{
#if HAVE_LITTLE_ENDIAN
	return le_val;
#else
	return bswap_64(le_val);
#endif
}

/**
 * le32_to_cpu - convert a little-endian uint32_t value
 * @le_val: little-endian value to convert
 */
static inline uint32_t le32_to_cpu(uint32_t le_val)
{
#if HAVE_LITTLE_ENDIAN
	return le_val;
#else
	return bswap_32(le_val);
#endif
}

/**
 * le16_to_cpu - convert a little-endian uint16_t value
 * @le_val: little-endian value to convert
 */
static inline uint16_t le16_to_cpu(uint16_t le_val)
{
#if HAVE_LITTLE_ENDIAN
	return le_val;
#else
	return bswap_16(le_val);
#endif
}

/**
 * cpu_to_be64 - convert a uint64_t value to big endian.
 * @native: value to convert
 */
static inline uint64_t cpu_to_be64(uint64_t native)
{
#if HAVE_LITTLE_ENDIAN
	return bswap_64(native);
#else
	return native;
#endif
}

/**
 * cpu_to_be32 - convert a uint32_t value to big endian.
 * @native: value to convert
 */
static inline uint32_t cpu_to_be32(uint32_t native)
{
#if HAVE_LITTLE_ENDIAN
	return bswap_32(native);
#else
	return native;
#endif
}

/**
 * cpu_to_be16 - convert a uint16_t value to big endian.
 * @native: value to convert
 */
static inline uint16_t cpu_to_be16(uint16_t native)
{
#if HAVE_LITTLE_ENDIAN
	return bswap_16(native);
#else
	return native;
#endif
}

/**
 * be64_to_cpu - convert a big-endian uint64_t value
 * @be_val: big-endian value to convert
 */
static inline uint64_t be64_to_cpu(uint64_t be_val)
{
#if HAVE_LITTLE_ENDIAN
	return bswap_64(be_val);
#else
	return be_val;
#endif
}

/**
 * be32_to_cpu - convert a big-endian uint32_t value
 * @be_val: big-endian value to convert
 */
static inline uint32_t be32_to_cpu(uint32_t be_val)
{
#if HAVE_LITTLE_ENDIAN
	return bswap_32(be_val);
#else
	return be_val;
#endif
}

/**
 * be16_to_cpu - convert a big-endian uint16_t value
 * @be_val: big-endian value to convert
 */
static inline uint16_t be16_to_cpu(uint16_t be_val)
{
#if HAVE_LITTLE_ENDIAN
	return bswap_16(be_val);
#else
	return be_val;
#endif
}

#endif /* CCAN_ENDIAN_H */

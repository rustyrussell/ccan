#if !defined(_ilog_H)
# define _ilog_H (1)
# include "config.h"
# include <stdint.h>
# include <ccan/compiler/compiler.h>
# include <limits.h>
/*Note the casts to (int) below: this prevents CLZ{32|64}_OFFS from "upgrading"
   the type of an entire expression to an (unsigned) size_t.*/
# if HAVE_BUILTIN_CLZ && INT_MAX>=2147483647
#  define CLZ32_OFFS ((int)sizeof(unsigned)*CHAR_BIT)
#  define CLZ32(_x) (__builtin_clz(_x))
# elif HAVE_BUILTIN_CLZL && LONG_MAX>=2147483647L
#  define CLZ32_OFFS ((int)sizeof(unsigned long)*CHAR_BIT)
#  define CLZ32(_x) (__builtin_clzl(_x))
# endif

# if HAVE_BUILTIN_CLZ && INT_MAX>=9223372036854775807LL
#  define CLZ64_OFFS ((int)sizeof(unsigned)*CHAR_BIT)
#  define CLZ64(_x) (__builtin_clz(_x))
# elif HAVE_BUILTIN_CLZL && LONG_MAX>=9223372036854775807LL
#  define CLZ64_OFFS ((int)sizeof(unsigned long)*CHAR_BIT)
#  define CLZ64(_x) (__builtin_clzl(_x))
# elif HAVE_BUILTIN_CLZLL /* long long must be >= 64 bits according to ISO C */
#  define CLZ64_OFFS ((int)sizeof(unsigned long long)*CHAR_BIT)
#  define CLZ64(_x) (__builtin_clzll(_x))
# endif



/**
 * ilog32 - Integer binary logarithm of a 32-bit value.
 * @_v: A 32-bit value.
 * Returns floor(log2(_v))+1, or 0 if _v==0.
 * This is the number of bits that would be required to represent _v in two's
 *  complement notation with all of the leading zeros stripped.
 * The ILOG_32() or ILOGNZ_32() macros may be able to use a builtin function
 *  instead, which should be faster.
 */
int ilog32(uint32_t _v) IDEMPOTENT_ATTRIBUTE;
/**
 * ilog64 - Integer binary logarithm of a 64-bit value.
 * @_v: A 64-bit value.
 * Returns floor(log2(_v))+1, or 0 if _v==0.
 * This is the number of bits that would be required to represent _v in two's
 *  complement notation with all of the leading zeros stripped.
 * The ILOG_64() or ILOGNZ_64() macros may be able to use a builtin function
 *  instead, which should be faster.
 */
int ilog64(uint64_t _v) IDEMPOTENT_ATTRIBUTE;


# if defined(CLZ32)
/**
 * ILOGNZ_32 - Integer binary logarithm of a non-zero 32-bit value.
 * @_v: A non-zero 32-bit value.
 * Returns floor(log2(_v))+1.
 * This is the number of bits that would be required to represent _v in two's
 *  complement notation with all of the leading zeros stripped.
 * If _v is zero, the return value is undefined; use ILOG_32() instead.
 */
#  define ILOGNZ_32(_v) (CLZ32_OFFS-CLZ32(_v))
/**
 * ILOG_32 - Integer binary logarithm of a 32-bit value.
 * @_v: A 32-bit value.
 * Returns floor(log2(_v))+1, or 0 if _v==0.
 * This is the number of bits that would be required to represent _v in two's
 *  complement notation with all of the leading zeros stripped.
 */
#  define ILOG_32(_v)   (ILOGNZ_32(_v)&-!!(_v))
# else
#  define ILOGNZ_32(_v) (ilog32(_v))
#  define ILOG_32(_v)   (ilog32(_v))
# endif

# if defined(CLZ64)
/**
 * ILOGNZ_64 - Integer binary logarithm of a non-zero 64-bit value.
 * @_v: A non-zero 64-bit value.
 * Returns floor(log2(_v))+1.
 * This is the number of bits that would be required to represent _v in two's
 *  complement notation with all of the leading zeros stripped.
 * If _v is zero, the return value is undefined; use ILOG_64() instead.
 */
#  define ILOGNZ_64(_v) (CLZ64_OFFS-CLZ64(_v))
/**
 * ILOG_64 - Integer binary logarithm of a 64-bit value.
 * @_v: A 64-bit value.
 * Returns floor(log2(_v))+1, or 0 if _v==0.
 * This is the number of bits that would be required to represent _v in two's
 *  complement notation with all of the leading zeros stripped.
 */
#  define ILOG_64(_v)   (ILOGNZ_64(_v)&-!!(_v))
# else
#  define ILOGNZ_64(_v) (ilog64(_v))
#  define ILOG_64(_v)   (ilog64(_v))
# endif

# define STATIC_ILOG0(_v) (!!(_v))
# define STATIC_ILOG1(_v) (((_v)&0x2)?2:STATIC_ILOG0(_v))
# define STATIC_ILOG2(_v) (((_v)&0xC)?2+STATIC_ILOG1((_v)>>2):STATIC_ILOG1(_v))
# define STATIC_ILOG3(_v) \
 (((_v)&0xF0)?4+STATIC_ILOG2((_v)>>4):STATIC_ILOG2(_v))
# define STATIC_ILOG4(_v) \
 (((_v)&0xFF00)?8+STATIC_ILOG3((_v)>>8):STATIC_ILOG3(_v))
# define STATIC_ILOG5(_v) \
 (((_v)&0xFFFF0000)?16+STATIC_ILOG4((_v)>>16):STATIC_ILOG4(_v))
# define STATIC_ILOG6(_v) \
 (((_v)&0xFFFFFFFF00000000ULL)?32+STATIC_ILOG5((_v)>>32):STATIC_ILOG5(_v))
/**
 * STATIC_ILOG_32 - The integer logarithm of an (unsigned, 32-bit) constant.
 * @_v: A non-negative 32-bit constant.
 * Returns floor(log2(_v))+1, or 0 if _v==0.
 * This is the number of bits that would be required to represent _v in two's
 *  complement notation with all of the leading zeros stripped.
 * This macro is suitable for evaluation at compile time, but it should not be
 *  used on values that can change at runtime, as it operates via exhaustive
 *  search.
 */
# define STATIC_ILOG_32(_v) (STATIC_ILOG5((uint32_t)(_v)))
/**
 * STATIC_ILOG_64 - The integer logarithm of an (unsigned, 64-bit) constant.
 * @_v: A non-negative 64-bit constant.
 * Returns floor(log2(_v))+1, or 0 if _v==0.
 * This is the number of bits that would be required to represent _v in two's
 *  complement notation with all of the leading zeros stripped.
 * This macro is suitable for evaluation at compile time, but it should not be
 *  used on values that can change at runtime, as it operates via exhaustive
 *  search.
 */
# define STATIC_ILOG_64(_v) (STATIC_ILOG6((uint64_t)(_v)))

#endif

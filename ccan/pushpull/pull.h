/* CC0 license (public domain) - see LICENSE file for details */
#ifndef CCAN_PUSHPULL_PULL_H
#define CCAN_PUSHPULL_PULL_H
#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * pull_bytes - unmarshall bytes from push_bytes.
 * @p: pointer to bytes to unmarshal
 * @max_len: pointer to number of bytes in unmarshal buffer
 * @dst: destination to copy bytes (or NULL to discard)
 * @len: length to copy into @dst.
 *
 * If @max_len isn't long enough, @p is set to NULL, @max_len is set to
 * 0 (making chaining safe), and false is returned.  Otherwise, @len
 * bytes are copied from *@p into @dst, *@p is incremented by @len,
 * @max_len is decremented by @len, and true is returned.
 */
bool pull_bytes(const char **p, size_t *max_len, void *dst, size_t len);

/**
 * pull_u64 - unmarshall a little-endian 64-bit value.
 * @p: pointer to bytes to unmarshal
 * @max_len: pointer to number of bytes in unmarshal buffer
 * @val: the value (or NULL to discard)
 *
 * This pulls 8 bytes and converts from little-endian (if necessary for
 * this platform).  It returns false and sets @p to NULL on error (ie. not
 * enough bytes).
 *
 * Example:
 * struct foo {
 *	uint64_t vu64;
 *	uint32_t vu32;
 *	uint16_t vu16;
 *	uint8_t vu8;
 * };
 *
 * static bool pull_foo(const char **p, size_t *len, struct foo *foo)
 * {
 * 	pull_u64(p, len, &foo->vu64);
 * 	pull_u32(p, len, &foo->vu32);
 * 	pull_u16(p, len, &foo->vu16);
 * 	pull_u8(p, len, &foo->vu8);
 *
 *      // p is set to NULL on error (we could also use return codes)
 *	return p != NULL;
 * }
 */
bool pull_u64(const char **p, size_t *max_len, uint64_t *val);

/**
 * pull_u32 - unmarshall a little-endian 32-bit value.
 * @p: pointer to bytes to unmarshal
 * @max_len: pointer to number of bytes in unmarshal buffer
 * @val: the value (or NULL to discard)
 *
 * This pulls 4 bytes and converts from little-endian (if necessary for
 * this platform).
 */
bool pull_u32(const char **p, size_t *max_len, uint32_t *val);

/**
 * pull_u16 - unmarshall a little-endian 16-bit value.
 * @p: pointer to bytes to unmarshal
 * @max_len: pointer to number of bytes in unmarshal buffer
 * @val: the value (or NULL to discard)
 *
 * This pulls 2 bytes and converts from little-endian (if necessary for
 * this platform).
 */
bool pull_u16(const char **p, size_t *max_len, uint16_t *val);

/**
 * pull_u8 - unmarshall a single byte value.
 * @p: pointer to bytes to unmarshal
 * @max_len: pointer to number of bytes in unmarshal buffer
 * @val: the value (or NULL to discard)
 *
 * This pulls one byte.
 */
bool pull_u8(const char **p, size_t *max_len, uint8_t *val);
#define pull_uchar pull_u8

/**
 * pull_s64 - unmarshall a little-endian 64-bit signed value.
 * @p: pointer to bytes to unmarshal
 * @max_len: pointer to number of bytes in unmarshal buffer
 * @val: the value (or NULL to discard)
 *
 * This pulls 8 bytes and converts from little-endian (if necessary for
 * this platform).
 */
bool pull_s64(const char **p, size_t *max_len, int64_t *val);

/**
 * pull_s32 - unmarshall a little-endian 32-bit signed value.
 * @p: pointer to bytes to unmarshal
 * @max_len: pointer to number of bytes in unmarshal buffer
 * @val: the value (or NULL to discard)
 *
 * This pulls 4 bytes and converts from little-endian (if necessary for
 * this platform).
 */
bool pull_s32(const char **p, size_t *max_len, int32_t *val);

/**
 * pull_s16 - unmarshall a little-endian 16-bit signed value.
 * @p: pointer to bytes to unmarshal
 * @max_len: pointer to number of bytes in unmarshal buffer
 * @val: the value (or NULL to discard)
 *
 * This pulls 2 bytes and converts from little-endian (if necessary for
 * this platform).
 */
bool pull_s16(const char **p, size_t *max_len, int16_t *val);

/**
 * pull_s8 - unmarshall a single byte signed value.
 * @p: pointer to bytes to unmarshal
 * @max_len: pointer to number of bytes in unmarshal buffer
 * @val: the value (or NULL to discard)
 *
 * This pulls one byte.
 */
bool pull_s8(const char **p, size_t *max_len, int8_t *val);

/**
 * pull_char - unmarshall a single char value.
 * @p: pointer to bytes to unmarshal
 * @max_len: pointer to number of bytes in unmarshal buffer
 * @val: the value (or NULL to discard)
 *
 * This pulls one character.
 */
bool pull_char(const char **p, size_t *max_len, char *val);
#endif /* CCAN_PUSHPULL_PULL_H */

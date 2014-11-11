/* CC0 license (public domain) - see LICENSE file for details */
#ifndef CCAN_PUSHPULL_PUSH_H
#define CCAN_PUSHPULL_PUSH_H
#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * push_bytes - marshall bytes into buffer
 * @p: buffer of marshalled bytes
 * @len: current length of @p buffer
 * @src: source to copy bytes from (or NULL to copy zeroes)
 * @srclen: length to copy from @src.
 *
 * If realloc() fails, false is returned.  Otherwise, *@p is increased
 * by @srclen bytes, *@len is incremented by @srclen, and bytes appended
 * to *@p (from @src if non-NULL).
 */
bool push_bytes(char **p, size_t *len, const void *src, size_t srclen);

/**
 * push_u64 - marshall a 64-bit value into buffer (as little-endian)
 * @p: buffer of marshalled bytes
 * @len: current length of @p buffer
 * @val: the value to marshall.
 *
 * If realloc() fails, false is returned.
 */
bool push_u64(char **p, size_t *len, uint64_t val);

/**
 * push_u32 - marshall a 32-bit value into buffer (as little-endian)
 * @p: buffer of marshalled bytes
 * @len: current length of @p buffer
 * @val: the value to marshall.
 *
 * If realloc() fails, false is returned.
 */
bool push_u32(char **p, size_t *len, uint32_t val);

/**
 * push_u16 - marshall a 16-bit value into buffer (as little-endian)
 * @p: buffer of marshalled bytes
 * @len: current length of @p buffer
 * @val: the value to marshall.
 *
 * If realloc() fails, false is returned.
 */
bool push_u16(char **p, size_t *len, uint16_t val);

/**
 * push_u8 - marshall an 8-bit value into buffer
 * @p: buffer of marshalled bytes
 * @len: current length of @p buffer
 * @val: the value to marshall.
 *
 * If realloc() fails, false is returned.
 */
bool push_u8(char **p, size_t *len, uint8_t val);
#define push_uchar push_u8

/**
 * push_u64 - marshall a signed 64-bit value into buffer (as little-endian)
 * @p: buffer of marshalled bytes
 * @len: current length of @p buffer
 * @val: the value to marshall.
 *
 * If realloc() fails, false is returned.
 */
bool push_s64(char **p, size_t *len, int64_t val);

/**
 * push_u32 - marshall a signed 32-bit value into buffer (as little-endian)
 * @p: buffer of marshalled bytes
 * @len: current length of @p buffer
 * @val: the value to marshall.
 *
 * If realloc() fails, false is returned.
 */
bool push_s32(char **p, size_t *len, int32_t val);

/**
 * push_u16 - marshall a signed 16-bit value into buffer (as little-endian)
 * @p: buffer of marshalled bytes
 * @len: current length of @p buffer
 * @val: the value to marshall.
 *
 * If realloc() fails, false is returned.
 */
bool push_s16(char **p, size_t *len, int16_t val);

/**
 * push_u8 - marshall a signed 8-bit value into buffer
 * @p: buffer of marshalled bytes
 * @len: current length of @p buffer
 * @val: the value to marshall.
 *
 * If realloc() fails, false is returned.
 */
bool push_s8(char **p, size_t *len, int8_t val);

/**
 * push_char - marshall a character into buffer
 * @p: buffer of marshalled bytes
 * @len: current length of @p buffer
 * @val: the value to marshall.
 *
 * If realloc() fails, false is returned.
 */
bool push_char(char **p, size_t *len, char val);

/**
 * push_set_realloc - set function to use (instead of realloc).
 * @reallocfn: new reallocation function.
 *
 * This can be used, for example, to cache reallocations.
 */
void push_set_realloc(void *(reallocfn)(void *ptr, size_t size));
#endif /* CCAN_PUSHPULL_PUSH_H */

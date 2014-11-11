/* CC0 license (public domain) - see LICENSE file for details */
#include "push.h"
#include <ccan/endian/endian.h>
#include <string.h>

static void *(*push_reallocfn)(void *ptr, size_t size) = realloc;

bool push_bytes(char **p, size_t *len, const void *src, size_t srclen)
{
	char *n = push_reallocfn(*p, *len + srclen);
	if (!n)
		return false;
	*p = n;
	if (src)
		memcpy(*p + *len, src, srclen);
	else
		memset(*p + *len, 0, srclen);
	*len += srclen;
	return true;
}

bool push_u64(char **p, size_t *len, uint64_t val)
{
	leint64_t v = cpu_to_le64(val);
	return push_bytes(p, len, &v, sizeof(v));
}

bool push_u32(char **p, size_t *len, uint32_t val)
{
	leint32_t v = cpu_to_le32(val);
	return push_bytes(p, len, &v, sizeof(v));
}

bool push_u16(char **p, size_t *len, uint16_t val)
{
	leint16_t v = cpu_to_le16(val);
	return push_bytes(p, len, &v, sizeof(v));
}

bool push_u8(char **p, size_t *len, uint8_t val)
{
	return push_bytes(p, len, &val, sizeof(val));
}

bool push_s64(char **p, size_t *len, int64_t val)
{
	return push_u64(p, len, val);
}

bool push_s32(char **p, size_t *len, int32_t val)
{
	return push_u32(p, len, val);
}

bool push_s16(char **p, size_t *len, int16_t val)
{
	return push_u16(p, len, val);
}

bool push_s8(char **p, size_t *len, int8_t val)
{
	return push_u8(p, len, val);
}

bool push_char(char **p, size_t *len, char val)
{
	return push_u8(p, len, val);
}

void push_set_realloc(void *(*reallocfn)(void *ptr, size_t size))
{
	push_reallocfn = reallocfn;
}

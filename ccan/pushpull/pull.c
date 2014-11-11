/* CC0 license (public domain) - see LICENSE file for details */
#include "pull.h"
#include <ccan/endian/endian.h>
#include <string.h>

bool pull_bytes(const char **p, size_t *max_len, void *dst, size_t len)
{
	if (*max_len < len) {
		*p = NULL;
		*max_len = 0;
		return false;
	}
	if (dst)
		memcpy(dst, *p, len);
	*max_len -= len;
	*p += len;
	return true;
}

bool pull_u64(const char **p, size_t *max_len, uint64_t *val)
{
	leint64_t v;

	if (pull_bytes(p, max_len, &v, sizeof(v))) {
		if (val)
			*val = le64_to_cpu(v);
		return true;
	}
	return false;
}

bool pull_u32(const char **p, size_t *max_len, uint32_t *val)
{
	leint32_t v;

	if (pull_bytes(p, max_len, &v, sizeof(v))) {
		if (val)
			*val = le32_to_cpu(v);
		return true;
	}
	return false;
}

bool pull_u16(const char **p, size_t *max_len, uint16_t *val)
{
	leint16_t v;

	if (pull_bytes(p, max_len, &v, sizeof(v))) {
		if (val)
			*val = le16_to_cpu(v);
		return true;
	}
	return false;
}

bool pull_u8(const char **p, size_t *max_len, uint8_t *val)
{
	return pull_bytes(p, max_len, val, sizeof(*val));
}

bool pull_s64(const char **p, size_t *max_len, int64_t *val)
{
	leint64_t v;

	if (pull_bytes(p, max_len, &v, sizeof(v))) {
		if (val)
			*val = le64_to_cpu(v);
		return true;
	}
	return false;
}

bool pull_s32(const char **p, size_t *max_len, int32_t *val)
{
	leint32_t v;

	if (pull_bytes(p, max_len, &v, sizeof(v))) {
		if (val)
			*val = le32_to_cpu(v);
		return true;
	}
	return false;
}

bool pull_s16(const char **p, size_t *max_len, int16_t *val)
{
	leint16_t v;

	if (pull_bytes(p, max_len, &v, sizeof(v))) {
		if (val)
			*val = le16_to_cpu(v);
		return true;
	}
	return false;
}

bool pull_s8(const char **p, size_t *max_len, int8_t *val)
{
	return pull_bytes(p, max_len, val, sizeof(*val));
}

bool pull_char(const char **p, size_t *max_len, char *val)
{
	return pull_bytes(p, max_len, val, sizeof(*val));
}

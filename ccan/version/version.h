/*****************************************************************************
 *
 * version - simple version handling functions for major.minor version
 * types
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
 *
 ****************************************************************************/

#ifndef CCAN_VERSION_H
#define CCAN_VERSION_H

#include <stdint.h>

struct version {
	uint32_t _v; /* major << 16  | minor */
};

/**
 * version_major - return the major version of the given struct
 * @v: the version number to obtain the major number from
 */
static inline uint16_t version_major(struct version v) {
	return (v._v & 0xFFFF0000) >> 16;
}

/**
 * version_minor - return the minor version of the given struct
 * @v: the version number to obtain the minor number from
 */
static inline uint16_t version_minor(const struct version v) {
	return v._v & 0xFFFF;
}

/**
 * version - create a new version number
 * @major: major version number
 * @minor: minor version number
 */
static inline struct version version(uint16_t major, uint16_t minor)
{
	struct version v = { ._v = major << 16 | minor };
	return v;
}

/**
 * version_cmp - compare two versions
 * @a: the first version number
 * @b: the second version number
 * @return a number greater, equal, or less than 0 if a is greater, equal or
 * less than b, respectively
 *
 * Example:
 *	struct version a = version(1, 0);
 *	struct version b = version(1, 3);
 *	if (version_cmp(a, b) < 0)
 *		printf("b is smaller than b\n");
 */
static inline int version_cmp(struct version a, struct version b)
{
	return  (a._v == b._v) ? 0 : (a._v > b._v) ? 1 : - 1;
}

#endif /* CCAN_VERSION_H */

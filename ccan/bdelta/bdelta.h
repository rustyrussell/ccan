/*
 * Copyright (C) 2011 Joseph Adams <joeyadams3.14159@gmail.com>
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

#ifndef CCAN_BDELTA_H
#define CCAN_BDELTA_H

#include <stddef.h>

typedef enum {
	BDELTA_OK               = 0,  /* Operation succeeded. */

	BDELTA_MEMORY           = 1,  /* Memory allocation failed. */
	BDELTA_PATCH_INVALID    = 2,  /* Patch is malformed. */
	BDELTA_PATCH_MISMATCH   = 3,  /* Patch applied to wrong original string. */
	
	/* Internal error codes.  These will never be returned by API functions. */
	BDELTA_INTERNAL_DMAX_EXCEEDED    = -10,
	BDELTA_INTERNAL_INPUTS_TOO_LARGE = -11,
} BDELTAcode;

/*
 * bdelta_diff - Given two byte strings, generate a "patch" (also a byte string)
 * that describes how to transform the old string into the new string.
 *
 * On success, returns BDELTA_OK, and passes a malloc'd block
 * and its size through *patch_out and *patch_size_out.
 *
 * On failure, returns an error code, and clears *patch_out and *patch_size_out.
 *
 * Example:
 *	const char *old = "abcabba";
 *	const char *new_ = "cbabac";
 *	void *patch;
 *	size_t patch_size;
 *	BDELTAcode rc;
 *
 *	rc = bdelta_diff(old, strlen(old), new_, strlen(new_), &patch, &patch_size);
 *	if (rc != BDELTA_OK) {
 *		bdelta_perror("bdelta_diff", rc);
 *		return;
 *	}
 *	...
 *	free(patch);
 */
BDELTAcode bdelta_diff(
	const void  *old,       size_t  old_size,
	const void  *new_,      size_t  new_size,
	void       **patch_out, size_t *patch_size_out
);

/*
 * bdelta_patch - Apply a patch produced by bdelta_diff to the
 * old string to recover the new string.
 *
 * On success, returns BDELTA_OK, and passes a malloc'd block
 * and its size through *new_out and *new_size_out.
 *
 * On failure, returns an error code, and clears *new_out and *new_size_out.
 *
 * Example:
 *	const char *old = "abcabba";
 *	void *new_;
 *	size_t new_size;
 *	BDELTAcode rc;
 *
 *	rc = bdelta_patch(old, strlen(old), patch, patch_size, &new_, &new_size);
 *	if (rc != BDELTA_OK) {
 *		bdelta_perror("bdelta_patch", rc);
 *		return;
 *	}
 *	fwrite(new_, 1, new_size, stdout);
 *	putchar('\n');
 *	free(new_);
 */
BDELTAcode bdelta_patch(
	const void  *old,     size_t  old_size,
	const void  *patch,   size_t  patch_size,
	void       **new_out, size_t *new_size_out
);

/*
 * bdelta_strerror - Return a string describing a bdelta error code.
 */
const char *bdelta_strerror(BDELTAcode code);

/*
 * bdelta_perror - Print a bdelta error message to stderr.
 *
 * This function handles @s the same way perror does.
 */
void bdelta_perror(const char *s, BDELTAcode code);

#endif

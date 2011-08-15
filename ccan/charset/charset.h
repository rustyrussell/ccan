/*
  Copyright (C) 2011 Joseph A. Adams (joeyadams3.14159@gmail.com)
  All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef CCAN_CHARSET_H
#define CCAN_CHARSET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define REPLACEMENT_CHARACTER 0xFFFD

/*
 * Type for Unicode codepoints.
 * We need our own because wchar_t might be 16 bits.
 */
typedef uint32_t uchar_t;

/*
 * Validate the given UTF-8 string.
 * If it contains '\0' characters, it is still valid.
 */
bool utf8_validate(const char *str, size_t length);

/*
 * Validate a single UTF-8 character.
 * @s: Beginning of UTF-8 character.
 * @e: End of string.
 *
 * If it's valid, return its length (1 thru 4).
 * If it's invalid or clipped, return 0.
 */
int utf8_validate_char(const char *s, const char *e);

/*
 * Read a single UTF-8 character starting at @s,
 * returning the length, in bytes, of the character read.
 *
 * This function assumes input is valid UTF-8,
 * and that there are enough characters in front of @s.
 */
int utf8_read_char(const char *s, uchar_t *out);

/*
 * Write a single UTF-8 character to @s,
 * returning the length, in bytes, of the character written.
 *
 * @unicode should be U+0000..U+10FFFF, but not U+D800..U+DFFF.
 * If @unicode is invalid, REPLACEMENT_CHARACTER will be emitted instead.
 *
 * This function will write up to 4 bytes to @out.
 */
int utf8_write_char(uchar_t unicode, char *out);

/*
 * Compute the Unicode codepoint of a UTF-16 surrogate pair.
 *
 * @uc should be 0xD800..0xDBFF, and @lc should be 0xDC00..0xDFFF.
 * If they aren't, this function returns REPLACEMENT_CHARACTER.
 */
uchar_t from_surrogate_pair(unsigned int uc, unsigned int lc);

/*
 * Construct a UTF-16 surrogate pair given a Unicode codepoint.
 *
 * @unicode should be U+10000..U+10FFFF.
 * If it's not, this function returns false,
 * and sets *uc and *lc to REPLACEMENT_CHARACTER.
 */
bool to_surrogate_pair(uchar_t unicode, unsigned int *uc, unsigned int *lc);

#endif

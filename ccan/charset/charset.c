/*
  Copyright (C) 2010 Joseph A. Adams (joeyadams3.14159@gmail.com)
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

#include "charset.h"

bool utf8_allow_surrogates = false;

bool utf8_validate(const char *str, size_t length)
{
	const unsigned char *s = (const unsigned char*)str;
	const unsigned char *e = s + length;
	
	while (s < e) {
		unsigned char c = *s++;
		unsigned int len; /* number of bytes in sequence - 2 */
		
		/* If character is ASCII, move on. */
		if (c < 0x80)
			continue;
		
		if (s >= e)
			return false; /* Missing bytes in sequence. */
		
		if (c < 0xE0) {
			/* 2-byte sequence, U+0080 to U+07FF
			   c must be 11000010 or higher
			   s[0] must be 10xxxxxx */
			len = 0;
			if (c < 0xC2)
				return false;
		} else if (c < 0xF0) {
			/* 3-byte sequence, U+0800 to U+FFFF
			   Note that the surrogate range is U+D800 to U+DFFF
			   c must be >= 11100000 (which it is)
			   If c is 11100000, then s[0] must be >= 10100000
			   If the global parameter utf8_allow_surrogates is false:
			      If c is 11101101 and s[0] is >= 10100000,
			         then this is a surrogate and we should fail.
			   s[0] and s[1] must be 10xxxxxx */
			len = 1;
			if (c == 0xE0 && *s < 0xA0)
				return false;
			if (!utf8_allow_surrogates && c == 0xED && *s >= 0xA0)
				return false;
		} else {
			/* 4-byte sequence, U+010000 to U+10FFFF
			   c must be >= 11110000 (which it is) and <= 11110100
			   If c is 11110000, then s[0] must be >= 10010000
			   If c is 11110100, then s[0] must be < 10010000
			   s[0], s[1], and s[2] must be 10xxxxxx */
			len = 2;
			if (c > 0xF4)
				return false;
			if (c == 0xF0 && *s < 0x90)
				return false;
			if (c == 0xF4 && *s >= 0x90)
				return false;
		}
		
		if (s + len >= e)
			return false; /* Missing bytes in sequence. */
		
		do {
			if ((*s++ & 0xC0) != 0x80)
				return false;
		} while (len--);
	}
	
	return true;
}

/*
  Note to future contributors: These routines are currently all under the
    MIT license.  It would be nice to keep it that way :)
*/

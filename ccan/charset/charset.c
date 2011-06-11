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

/*
 * This function implements the syntax given in RFC3629, which is
 * the same as that given in The Unicode Standard, Version 6.0.
 *
 * It has the following properties:
 *
 *  * All codepoints U+0000..U+10FFFF may be encoded,
 *    except for U+D800..U+DFFF, which are reserved
 *    for UTF-16 surrogate pair encoding.
 *  * UTF-8 byte sequences longer than 4 bytes are not permitted,
 *    as they exceed the range of Unicode.
 *  * The sixty-six Unicode "non-characters" are permitted
 *    (namely, U+FDD0..U+FDEF, U+xxFFFE, and U+xxFFFF).
 */
bool utf8_validate(const char *str, size_t length)
{
	const unsigned char *s = (const unsigned char*)str;
	const unsigned char *e = s + length;
	
	while (s < e) {
		unsigned char c = *s++;
		unsigned char c2;
		int len_minus_two;
		
		/* Validate the first byte and determine the sequence length. */
		if (c <= 0x7F)          /* 00..7F */
			continue;
		else if (c <= 0xC1)     /* 80..C1 */
			return false;
		else if (c <= 0xDF)     /* C2..DF */
			len_minus_two = 0;
		else if (c <= 0xEF)     /* E0..EF */
			len_minus_two = 1;
		else if (c <= 0xF4)     /* F0..F4 */
			len_minus_two = 2;
		else
			return false;
		
		/* Make sure the character isn't clipped. */
		if (s + len_minus_two >= e)
			return false;
		
		c2 = *s;
		
		/* Make sure subsequent bytes are in the range 0x80..0xBF. */
		do {
			if ((*s++ & 0xC0) != 0x80)
				return false;
		} while (len_minus_two--);
		
		/* Handle special cases. */
		switch (c) {
			case 0xE0:
				/* Disallow overlong 3-byte sequence. */
				if (c2 < 0xA0)
					return false;
				break;
			case 0xED:
				/* Disallow U+D800..U+DFFF. */
				if (c2 > 0x9F)
					return false;
				break;
			case 0xF0:
				/* Disallow overlong 4-byte sequence. */
				if (c2 < 0x90)
					return false;
				break;
			case 0xF4:
				/* Disallow codepoints beyond U+10FFFF. */
				if (c2 > 0x8F)
					return false;
				break;
		}
	}
	
	return true;
}

int utf8_read_char(const char *s, uchar_t *out)
{
	const unsigned char *c = (const unsigned char*) s;

	if (c[0] <= 0x7F) {
		/* 00..7F */
		*out = c[0];
		return 1;
	} else if (c[0] <= 0xDF) {
		/* C2..DF (unless input is invalid) */
		*out = ((uchar_t)c[0] & 0x1F) << 6 |
		       ((uchar_t)c[1] & 0x3F);
		return 2;
	} else if (c[0] <= 0xEF) {
		/* E0..EF */
		*out = ((uchar_t)c[0] &  0xF) << 12 |
		       ((uchar_t)c[1] & 0x3F) << 6  |
		       ((uchar_t)c[2] & 0x3F);
		return 3;
	} else {
		/* F0..F4 (unless input is invalid) */
		*out = ((uchar_t)c[0] &  0x7) << 18 |
		       ((uchar_t)c[1] & 0x3F) << 12 |
		       ((uchar_t)c[2] & 0x3F) << 6  |
		       ((uchar_t)c[3] & 0x3F);
		return 4;
	}
}

int utf8_write_char(uchar_t unicode, char *out)
{
	unsigned char *o = (unsigned char*) out;

	if (unicode <= 0x7F) {
		/* U+0000..U+007F */
		*o++ = unicode;
		return 1;
	} else if (unicode <= 0x7FF) {
		/* U+0080..U+07FF */
		*o++ = 0xC0 | unicode >> 6;
		*o++ = 0x80 | (unicode & 0x3F);
		return 2;
	} else if (unicode <= 0xFFFF) {
		/* U+0800..U+FFFF */
		if (unicode >= 0xD800 && unicode <= 0xDFFF)
			unicode = REPLACEMENT_CHARACTER;
	three_byte_character:
		*o++ = 0xE0 | unicode >> 12;
		*o++ = 0x80 | (unicode >> 6 & 0x3F);
		*o++ = 0x80 | (unicode & 0x3F);
		return 3;
	} else if (unicode <= 0x10FFFF) {
		/* U+10000..U+10FFFF */
		*o++ = 0xF0 | unicode >> 18;
		*o++ = 0x80 | (unicode >> 12 & 0x3F);
		*o++ = 0x80 | (unicode >> 6 & 0x3F);
		*o++ = 0x80 | (unicode & 0x3F);
		return 4;
	} else {
		/* U+110000... */
		unicode = REPLACEMENT_CHARACTER;
		goto three_byte_character;
	}
}

uchar_t from_surrogate_pair(unsigned int uc, unsigned int lc)
{
	if (uc >= 0xD800 && uc <= 0xDBFF && lc >= 0xDC00 && lc <= 0xDFFF)
		return 0x10000 + ((((uchar_t)uc & 0x3FF) << 10) | (lc & 0x3FF));
	else
		return REPLACEMENT_CHARACTER;
}

bool to_surrogate_pair(uchar_t unicode, unsigned int *uc, unsigned int *lc)
{
	if (unicode >= 0x10000 && unicode <= 0x10FFFF) {
		uchar_t n = unicode - 0x10000;
		*uc = ((n >> 10) & 0x3FF) | 0xD800;
		*lc = (n & 0x3FF) | 0xDC00;
		return true;
	} else {
		*uc = *lc = REPLACEMENT_CHARACTER;
		return false;
	}
}

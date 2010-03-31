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

#ifndef CCAN_CHARSET_H
#define CCAN_CHARSET_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Validate the given UTF-8 string.  If it contains '\0' characters,
 * it is still valid.
 *
 * By default, Unicode characters U+D800 thru U+DFFF will be considered
 * invalid UTF-8.  However, if you set utf8_allow_surrogates to true,
 * they will be allowed.  Allowing the surrogate range makes it possible
 * to losslessly encode malformed UTF-16.
 */
bool utf8_validate(const char *str, size_t length);

/* Default: false */
extern bool utf8_allow_surrogates;

#endif

/* CC0 (Public domain) - see LICENSE file for details */
#include <ccan/stringbuilder/stringbuilder.h>
#include <string.h>
#include <errno.h>

int stringbuilder_args(char* str, size_t str_sz, const char* delim, ...)
{
	int res;
	va_list ap;
	va_start(ap, delim);
	res = stringbuilder_va(str, str_sz, delim, ap);
	va_end(ap);
	return res;
}

static int stringbuilder_cpy(
		char** str, size_t* str_sz, const char* s, size_t s_len)
{
	if (!s)
		return 0;

	if (*str != s) {
		if (!s_len)
			s_len = strlen(s);
		if (s_len > *str_sz)
			return EMSGSIZE;
		strcpy(*str, s);
	}
	*str += s_len;
	*str_sz -= s_len;
	return 0;
}

int stringbuilder_va(char* str, size_t str_sz, const char* delim, va_list ap)
{
	int res = 0;
	size_t delim_len = 0;
	const char* s = va_arg(ap, const char*);

	if (delim)
		delim_len = strlen(delim);

	res = stringbuilder_cpy(&str, &str_sz, s, 0);
	s = va_arg(ap, const char*);
	while(s && !res) {
		res = stringbuilder_cpy(&str, &str_sz,
				delim, delim_len);
		if (!res) {
			res = stringbuilder_cpy(&str, &str_sz,
						s, 0);
			s = va_arg(ap, const char*);
		}
	}
	return res;
}

int stringbuilder_array(char* str, size_t str_sz, const char* delim,
		size_t n_strings, const char** strings)
{
	int res = 0;
	size_t delim_len = 0;

	if (delim)
		delim_len = strlen(delim);

	res = stringbuilder_cpy(&str, &str_sz,
			*(strings++), 0);
	while(--n_strings && !res) {
		res = stringbuilder_cpy(&str, &str_sz,
				delim, delim_len);
		if (!res)
			res = stringbuilder_cpy(&str, &str_sz,
					*(strings++), 0);
	}
	return res;

}

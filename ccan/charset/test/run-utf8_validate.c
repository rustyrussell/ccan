#include <ccan/charset/charset.c>
#include <ccan/tap/tap.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

/* Make a valid or invalid Unicode character fitting in exactly @len UTF-8 bytes. */
static uchar_t utf8_randcode(int len, bool valid, bool after_clipped)
{
	uint32_t r = rand32();
	uchar_t ret;
	
	#define range(lo, hi)  ((r & 0x7FFFFFFF) % ((hi)-(lo)+1) + (lo))
	#define high_bit_set() (!!(r & 0x80000000))
	
	switch (len) {
		case 1:
			if (valid) {
				/* Generate a character U+0000..U+007F */
				return r & 0x7F;
			} else {
				/*
				 * Generate a character U+0080..U+00BF or U+00F8..U+00FF.
				 *
				 * However, don't generate U+0080..U+00BF (10xxxxxx) after a
				 * clipped character, as that can inadvertently form a valid,
				 * complete character.
				 */
				if (!after_clipped && high_bit_set())
					return range(0x80, 0xBF);
				else
					return range(0xF8, 0xFF);
			}
		case 2:
			if (valid) {
				/* Generate a character U+0080..U+07FF */
				return range(0x80, 0x7FF);
			} else {
				/* Generate a character U+0000..U+007F */
				return r & 0x7F;
			}
		case 3:
			if (valid) {
				/* Generate a character U+0800..U+FFFF, but not U+D800..U+DFFF */
				for (;;) {
					ret = range(0x800, 0xFFFF);
					if (ret >= 0xD800 && ret <= 0xDFFF) {
						r = rand32();
						continue;
					} else {
						break;
					}
				}
				return ret;
			} else {
				/* Generate a character U+0000..U+07FF or U+D800..U+DFFF */
				if (high_bit_set())
					return r & 0x7FF;
				else
					return 0xD800 + (r & 0x7FF);
			}
		case 4:
			if (valid) {
				/* Generate a character U+10000..U+10FFFF */
				return range(0x10000, 0x10FFFF);
			} else {
				/* Generate a character U+0000..0xFFFF or U+110000..U+1FFFFF */
				if (high_bit_set())
					return r & 0xFFFF;
				else
					return range(0x110000, 0x1FFFFF);
			}
		default:
			assert(false);
	}
	
	#undef range
	#undef high_bit_set
}

/* Encode @uc as UTF-8 using exactly @len characters.
   @len should be 1 thru 4. */
static void utf8_encode_raw(char *out, unsigned int uc, int len)
{
	switch (len) {
		case 1:
			assert(uc <= 0xC1 || (uc >= 0xF8 && uc <= 0xFF));
			*out++ = uc;
			break;
		case 2:
			assert(uc <= 0x7FF);
			*out++ = 0xC0 | ((uc >> 6) & 0x1F);
			*out++ = 0x80 | (uc & 0x3F);
			break;
		case 3:
			assert(uc <= 0xFFFF);
			*out++ = 0xE0 | ((uc >> 12) & 0x0F);
			*out++ = 0x80 | ((uc >> 6) & 0x3F);
			*out++ = 0x80 | (uc & 0x3F);
			break;
		case 4:
			assert(uc <= 0x1FFFFF);
			*out++ = 0xF0 | ((uc >> 18) & 0x07);
			*out++ = 0x80 | ((uc >> 12) & 0x3F);
			*out++ = 0x80 | ((uc >> 6) & 0x3F);
			*out++ = 0x80 | (uc & 0x3F);
			break;
	}
}

#if COMPUTE_AVERAGE_LENGTH
double total_averages;
#endif

/* Generate a UTF-8 string of the given byte length,
   randomly deciding if it should be valid or not.
   
   Return true if it's valid, false if it's not. */
static bool utf8_mktest(char *out, int len)
{
	double pf;
	uint32_t pu;
	int n;
	bool valid = true;
	bool v;
	bool after_clipped = false;
	
	#if COMPUTE_AVERAGE_LENGTH
	int n_total = 0;
	int count = 0;
	#endif
	
	/*
	 * Probability that, per character, it should be valid.
	 * The goal is to make utf8_mktest as a whole
	 * have a 50% chance of generating a valid string.
	 *
	 * The equation being solved is:
	 *
	 *     p^n = 0.5
	 *
	 * where p is the probability that each character is valid,
	 * and n is the number of characters in the string.
	 *
	 * 2.384 is the approximate average length of each character,
	 * so len/2.384 is about how many characters this string
	 * is expected to contain.
	 */
	pf = pow(0.5, 2.384/len);
	
	/* Convert to uint32_t to test against rand32. */
	pu = pf * 4294967295.0;
	
	for (;len > 0; len -= n, out += n) {
		v = rand32() <= pu;
		
		if (v) {
			/* Generate a valid character. */
			n = rand32() % (len < 4 ? len : 4) + 1;
			utf8_encode_raw(out, utf8_randcode(n, true, after_clipped), n);
			after_clipped = false;
		} else if (rand32() % 5) {
			/* Generate an invalid character. */
			n = rand32() % (len < 4 ? len : 4) + 1;
			utf8_encode_raw(out, utf8_randcode(n, false, after_clipped), n);
			after_clipped = false;
		} else {
			/* Generate a clipped but otherwise valid character. */
			char tmp[4];
			n = rand32() % 3 + 2;
			utf8_encode_raw(tmp, utf8_randcode(n, true, after_clipped), n);
			n -= rand32() % (n-1) + 1;
			if (n > len)
				n = len;
			assert(n >= 1 && n <= 3);
			memcpy(out, tmp, n);
			after_clipped = true;
		}
		
		if (!v)
			valid = false;
		
		#if COMPUTE_AVERAGE_LENGTH
		n_total += n;
		count++;
		#endif
	}
	
	#if COMPUTE_AVERAGE_LENGTH
	if (count > 0)
		total_averages += (double)n_total / count;
	#endif
	
	return valid;
}

static void test_utf8_validate(void)
{
	char buffer[128];
	int i;
	int len;
	bool valid;
	int passed=0, p_valid=0, p_invalid=0, total=0;
	int count;
	
	count = 100000;
	
	#if COMPUTE_AVERAGE_LENGTH
	total_averages = 0.0;
	#endif
	
	for (i=0; i<count; i++) {
		len = rand32() % (sizeof(buffer) + 1);
		valid = utf8_mktest(buffer, len);
		if (utf8_validate(buffer, len) == valid) {
			passed++;
			if (valid)
				p_valid++;
			else
				p_invalid++;
		} else {
			bool uvalid = utf8_validate(buffer, len);
			printf("Failed: generated %s string, but utf8_validate returned %s\n",
			       valid ? "valid" : "invalid",
			       uvalid ? "true" : "false");
		}
		total++;
	}
	
	if (passed == total)
		pass("%d valid tests, %d invalid tests", p_valid, p_invalid);
	else
		fail("Passed only %d out of %d tests\n", passed, total);
	
	ok(p_valid > count/10 && p_invalid > count/10,
	   "Valid and invalid should be balanced");
	
	#if COMPUTE_AVERAGE_LENGTH
	printf("Average character length: %f\n", total_averages / count);
	#endif
}

int main(void)
{
	/* This is how many tests you plan to run */
	plan_tests(2);
	
	test_utf8_validate();

	/* This exits depending on whether all tests passed */
	return exit_status();
}

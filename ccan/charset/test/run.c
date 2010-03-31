#include <ccan/charset/charset.h>
#include <ccan/charset/charset.c>
#include <ccan/tap/tap.h>

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Finds a pseudorandom 32-bit number from 0 to 2^32-1 .
 * Uses the BCPL linear congruential generator method.
 *
 * Used instead of system RNG to ensure tests are consistent.
 */
static uint32_t rand32(void)
{
	static uint32_t rand32_state = 0;
	rand32_state *= (uint32_t)0x7FF8A3ED;
	rand32_state += (uint32_t)0x2AA01D31;
	return rand32_state;
}

/*
 * Make a Unicode character requiring exactly @len UTF-8 bytes.
 *
 * Unless utf8_allow_surrogates is set,
 * do not return a value in the range U+D800 thru U+DFFF .
 *
 * If @len is not 1 thru 4, generate an out-of-range character.
 */
static unsigned int utf8_randcode(int len)
{
	uint32_t r = rand32();
	unsigned int ret;
	
	switch (len) {
		case 1: return r % 0x80;
		case 2: return r % (0x800-0x80) + 0x80;
		case 3:
			for (;;) {
				ret = r % (0x10000-0x800) + 0x800;
				if (!utf8_allow_surrogates && ret >= 0xD800 && ret <= 0xDFFF)
				{
					r = rand32();
					continue;
				} else {
					break;
				}
			}
			return ret;
		case 4: return r % (0x110000-0x10000) + 0x10000;
		default:
			while (r < 0x110000)
				r = rand32();
			return r;
	}
}

static unsigned int rand_surrogate(void)
{
	return rand32() % (0xE000 - 0xD800) + 0xD800;
}

/* Encode @uc as UTF-8 using exactly @len characters.
   @len should be 1 thru 4.
   @uc will be truncated to the bits it will go into.
   If, after bit truncation, @uc is in the wrong range for its length,
   an invalid character will be generated. */
static void utf8_encode_raw(char *out, unsigned int uc, int len)
{
	switch (len) {
		case 1:
			*out++ = uc & 0x7F;
			break;
		case 2:
			*out++ = 0xC0 | ((uc >> 6) & 0x1F);
			*out++ = 0x80 | (uc & 0x3F);
			break;
		case 3:
			*out++ = 0xE0 | ((uc >> 12) & 0x0F);
			*out++ = 0x80 | ((uc >> 6) & 0x3F);
			*out++ = 0x80 | (uc & 0x3F);
			break;
		case 4:
			*out++ = 0xF0 | ((uc >> 18) & 0x07);
			*out++ = 0x80 | ((uc >> 12) & 0x3F);
			*out++ = 0x80 | ((uc >> 6) & 0x3F);
			*out++ = 0x80 | (uc & 0x3F);
			break;
	}
}

/* Generate a UTF-8 string of the given byte length,
   randomly deciding if it should be valid or not.
   
   Return true if it's valid, false if it's not. */
static bool utf8_mktest(char *out, int len)
{
	int m, n;
	bool valid = true;
	bool v;
	double pf;
	uint32_t pu;
	
	/* Probability that, per character, it should be valid.
	   The goal is to make utf8_mktest as a whole
	   have a 50% chance of generating a valid string. */
	pf = pow(0.5, 2.5/len);
	
	/* Convert to uint32_t to test against rand32. */
	pu = pf * 4294967295.0;
	
	for (;len; len -= n) {
		v = len == 1 || rand32() <= pu;
		m = len < 4 ? len : 4;
		
		if (v) {
			/* Generate a valid character. */
			n = rand32() % m + 1;
			utf8_encode_raw(out, utf8_randcode(n), n);
		} else {
			/* Generate an invalid character. */
			assert(m >= 2);
			n = rand32() % (m-1) + 2;
			switch (n) {
				case 2:
					utf8_encode_raw(out, utf8_randcode(1), n);
					break;
				case 3:
					if (!utf8_allow_surrogates && (rand32() & 1))
						utf8_encode_raw(out, rand_surrogate(), n);
					else
						utf8_encode_raw(out, utf8_randcode(rand32() % (n-1) + 1), n);
					break;
				case 4:
					utf8_encode_raw(out, utf8_randcode(rand32() % (n-1) + 1), n);
					break;
			}
			valid = false;
		}
		out += n;
	}
	
	return valid;
}

static void test_utf8_validate(bool allow_surrogates)
{
	char buffer[1024];
	int i;
	int len;
	bool valid;
	int passed=0, p_valid=0, p_invalid=0, total=0;
	int count;
	
	count = 10000;
	
	utf8_allow_surrogates = allow_surrogates;
	
	for (i=0; i<count; i++) {
		len = rand32() % (1024 + 1);
		valid = utf8_mktest(buffer, len);
		if (utf8_validate(buffer, len) == valid) {
			passed++;
			if (valid)
				p_valid++;
			else
				p_invalid++;
		}
		total++;
	}
	
	if (passed == total) {
		printf("PASS:  %d valid tests, %d invalid tests\n",
			p_valid, p_invalid);
	} else {
		printf("FAIL:  Passed %d out of %d tests\n", passed, total);
	}
	
	ok(passed, "utf8_validate test passed%s",
		!allow_surrogates ? " (surrogates disallowed)" : "");
	
	ok(p_valid > count/10 && p_invalid > count/10,
		"   valid/invalid are balanced");
}

int main(void)
{
	/* This is how many tests you plan to run */
	plan_tests(4);
	
	test_utf8_validate(false);
	test_utf8_validate(true);

	/* This exits depending on whether all tests passed */
	return exit_status();
}

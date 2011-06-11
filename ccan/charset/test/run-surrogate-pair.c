#include <ccan/charset/charset.c>
#include <ccan/tap/tap.h>

#include <string.h>

#include "common.h"

/*
 * Testing procedure for from_surrogate_pair and to_surrogate_pair:
 *
 *  * For each Unicode code point from 0x10000 to 0x10FFFF:
 *    - Call to_surrogate_pair, and make sure that:
 *      - It returns true.
 *      - uc is 0xD800..0xDBFF
 *      - lc is 0xDC00..0xDFFF
 *    - Call from_surrogate_pair on the pair, and make sure that
 *      it returns the original character.
 *  * For various invalid arguments to to_surrogate_pair
 *    (U+0000..U+FFFF and U+110000...):
 *    - Call to_surrogate_pair, and make sure it:
 *      - Returns false.
 *      - Sets *uc and *lc to REPLACEMENT_CHARACTER.
 *  * For various invalid arguments to from_surrogate_pair
 *    (uc: not 0xD800..0xDBFF, lc: not 0xDC00..0xDFFF):
 *    - Call from_surrogate_pair, and make sure
 *      it returns REPLACEMENT_CHARACTER.
 */

#define INVALID_TRIAL_COUNT     10000

#define range(r, lo, hi)  ((r) % ((hi)-(lo)+1) + (lo))

static void test_valid(void)
{
	uchar_t unicode;
	unsigned int uc, lc;
	
	for (unicode = 0x10000; unicode <= 0x10FFFF; unicode++) {
		if (to_surrogate_pair(unicode, &uc, &lc) != true) {
			fail("to_surrogate_pair did not return true on valid input.");
			return;
		}
		if (!(uc >= 0xD800 && uc <= 0xDBFF)) {
			fail("to_surrogate_pair: uc is out of range");
			return;
		}
		if (!(lc >= 0xDC00 && lc <= 0xDFFF)) {
			fail("to_surrogate_pair: lc is out of range");
			return;
		}
		if (from_surrogate_pair(uc, lc) != unicode) {
			fail("Surrogate pair conversion did not preserve original value (U+%04lX).", (unsigned long)unicode);
			return;
		}
	}
	
	pass("to_surrogate_pair and from_surrogate_pair work for all valid arguments.");
}

static void test_invalid_to_surrogate_pair(void)
{
	long i;
	uchar_t unicode;
	unsigned int uc, lc;
	
	for (i = 1; i <= INVALID_TRIAL_COUNT; i++) {
		if (rand32() % 2) {
			unicode = range(rand32(), 0x0, 0xFFFF);
		} else {
			do {
				unicode = rand32();
			} while (unicode < 0x110000);
		}
		
		if (to_surrogate_pair(unicode, &uc, &lc) != false) {
			fail("to_surrogate_pair did not return false on invalid input.");
			return;
		}
		if (uc != REPLACEMENT_CHARACTER || lc != REPLACEMENT_CHARACTER) {
			fail("to_surrogate_pair did not set uc and lc to the replacement character on invalid input.");
			return;
		}
	}
	
	pass("to_surrogate_pair seems to handle invalid argument values properly.");
}

static void test_invalid_from_surrogate_pair(void)
{
	long i;
	unsigned int uc, lc;
	
	for (i = 1; i <= INVALID_TRIAL_COUNT; i++) {
		switch (rand32() % 3) {
			case 0:
				uc = range(rand32(), 0x0, 0xD7FF);
				break;
			case 1:
				uc = range(rand32(), 0xDC00, 0xDFFF);
				break;
			default:
				uc = range(rand32(), 0xE000, 0xFFFF);
				break;
		}
		switch (rand32() % 3) {
			case 0:
				lc = range(rand32(), 0x0, 0xD7FF);
				break;
			case 1:
				lc = range(rand32(), 0xD800, 0xDBFF);
				break;
			default:
				lc = range(rand32(), 0xE000, 0xFFFF);
				break;
		}
		
		if (from_surrogate_pair(uc, lc) != REPLACEMENT_CHARACTER) {
			fail("from_surrogate_pair(0x%04X, 0x%04X) did not return the replacement character", uc, lc);
			return;
		}
	}
	
	pass("from_surrogate_pair seems to handle invalid arguments properly.");
}

int main(void)
{
	plan_tests(3);
	
	test_valid();
	test_invalid_to_surrogate_pair();
	test_invalid_from_surrogate_pair();
	
	return exit_status();
}

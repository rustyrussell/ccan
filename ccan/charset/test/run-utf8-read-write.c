#include <ccan/charset/charset.c>
#include <ccan/tap/tap.h>

#include <string.h>

#include "common.h"

/*
 * Testing procedure for utf8_read_char and utf8_write_char:
 *
 *  * Generate N valid and invalid Unicode code points.
 *  * Encode them with utf8_write_char.
 *  * Copy the resulting string into a buffer sized exactly as big as
 *    the string produced.  This way, Valgrind can catch buffer overflows
 *    by utf8_validate and utf8_read_char.
 *  * Validate the string with utf8_validate.
 *  * Decode the string, ensuring that:
 *    - Valid codepoints are read back.
 *    - Invalid characters are read back, but replaced
 *      with REPLACEMENT_CHARACTER.
 *    - No extra characters are read back.
 */

#define TRIAL_COUNT             1000
#define MAX_CHARS_PER_TRIAL     100

#define range(r, lo, hi)  ((r) % ((hi)-(lo)+1) + (lo))

int main(void)
{
	int trial;
	
	plan_tests(TRIAL_COUNT);
	
	for (trial = 1; trial <= TRIAL_COUNT; trial++) {
		int i, count;
		uchar_t codepoints[MAX_CHARS_PER_TRIAL];
		uchar_t c;
		bool c_valid;
		
		char write_buffer[MAX_CHARS_PER_TRIAL * 4];
		char *o = write_buffer;
		char *oe = write_buffer + sizeof(write_buffer);
		
		char *string;
		const char *s;
		const char *e;
		
		int len;
		
		count = rand32() % MAX_CHARS_PER_TRIAL + 1;
		
		for (i = 0; i < count; i++) {
			if (o >= oe) {
				fail("utf8_write_char: Buffer overflow (1)");
				goto next_trial;
			}
			
			switch (rand32() % 7) {
				case 0:
					c = range(rand32(), 0x0, 0x7F);
					c_valid = true;
					break;
				case 1:
					c = range(rand32(), 0x80, 0x7FF);
					c_valid = true;
					break;
				case 2:
					c = range(rand32(), 0x800, 0xD7FF);
					c_valid = true;
					break;
				case 3:
					c = range(rand32(), 0xD800, 0xDFFF);
					c_valid = false;
					break;
				case 4:
					c = range(rand32(), 0xE000, 0xFFFF);
					c_valid = true;
					break;
				case 5:
					c = range(rand32(), 0x10000, 0x10FFFF);
					c_valid = true;
					break;
				default:
					do {
						c = rand32();
					} while (c < 0x110000);
					c_valid = false;
					break;
			}
			
			codepoints[i] = c_valid ? c : REPLACEMENT_CHARACTER;
			
			len = utf8_write_char(c, o);
			if (len < 1 || len > 4) {
				fail("utf8_write_char: Return value is not 1 thru 4.");
				goto next_trial;
			}
			o += len;
		}
		if (o > oe) {
			fail("utf8_write_char: Buffer overflow (2)");
			goto next_trial;
		}
		
		string = malloc(o - write_buffer);
		memcpy(string, write_buffer, o - write_buffer);
		s = string;
		e = string + (o - write_buffer);
		
		if (!utf8_validate(s, e - s)) {
			fail("Invalid string produced by utf8_write_char.");
			goto next_trial_free_string;
		}
		
		for (i = 0; i < count; i++) {
			if (s >= e) {
				fail("utf8_read_char: Buffer overflow (1)");
				goto next_trial_free_string;
			}
			
			len = utf8_read_char(s, &c);
			if (len < 1 || len > 4) {
				fail("utf8_read_char: Return value is not 1 thru 4.");
				goto next_trial_free_string;
			}
			if (c != codepoints[i]) {
				fail("utf8_read_char: Character read differs from that written.");
				goto next_trial_free_string;
			}
			s += len;
		}
		if (s > e) {
			fail("utf8_read_char: Buffer overflow (2)");
			goto next_trial_free_string;
		}
		if (s < e) {
			fail("utf8_read_char: Did not reach end of string.");
			goto next_trial_free_string;
		}
		
		pass("Trial %d: %d characters", trial, count);
		
	next_trial_free_string:
		free(string);
	next_trial:;
	}
	
	return exit_status();
}

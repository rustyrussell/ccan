#include "common.h"

#define is_digit(c) ((c) >= '0' && (c) <= '9')

static uint32_t parse_int(const char **sp)
{
	const char *s = *sp;
	uint32_t n = 0;
	
	while (is_digit(*s)) {
		n *= 10;
		n += *s++ - '0';
	}
	
	*sp = s;
	return n;
}

static const char *op_name(int op)
{
	switch (op) {
		case OP_COPY:
			return "copy";
		case OP_SKIP:
			return "skip";
		case OP_INSERT:
			return "insert";
		
		default:
			return "<invalid opcode>";
	}
}

static const char *verify_csi32(
	const unsigned char *patch_start, const unsigned char *patch_end,
	const char *expected)
{
	const unsigned char *p = patch_start;
	
	if (p >= patch_end)
		return "Patch type byte missing";
	if (*p++ != PT_CSI32)
		return "Patch type byte is not PT_CSI32";
	
	for (;;) {
		int patch_op;
		uint32_t patch_size;
		int expected_op;
		uint32_t expected_size;
		
		while (*expected == ' ')
			expected++;
		if (*expected == '\0')
			break;
		
		if (!csi32_parse_op(&p, patch_end, &patch_op, &patch_size)) {
			if (p == patch_end)
				return "Patch shorter than expected.";
			else
				return "Patch contains invalid CSI-32";
		}
		
		switch (*expected) {
			case 'c':
				expected_op = OP_COPY;
				break;
			case 's':
				expected_op = OP_SKIP;
				break;
			case 'i':
				expected_op = OP_INSERT;
				break;
			
			default:
				diag("verify_csi32: Unexpected character %c", *expected);
				return "Syntax error in expected difference";
		}
		expected++;
		
		while (*expected == ' ')
			expected++;
		
		if (patch_op != expected_op) {
			diag("verify_csi32: Expected %s, but next op is %s %lu",
			     op_name(expected_op), op_name(patch_op), (unsigned long)patch_size);
			return "Operation mismatch";
		}
		
		if (expected_op == OP_COPY || expected_op == OP_SKIP) {
			if (!is_digit(*expected)) {
				diag("verify_csi32: Expected size after %s instruction",
				     op_name(expected_op));
				return "Syntax error in expected difference";
			}
			expected_size = parse_int(&expected);
			
			if (patch_size != expected_size) {
				diag("verify_csi32: Expected %s %lu, but patch says %s %lu",
				     op_name(expected_op), (unsigned long)expected_size,
				     op_name(expected_op), (unsigned long)patch_size);
				return "Operation size mismatch";
			}
		} else {
			if (*expected++ != '\'') {
				diag("verify_csi32: Expected single-quoted string after insert instruction");
				return "Syntax error in expected difference";
			}
			
			for (expected_size = 0; ; expected_size++) {
				unsigned char c;
				
				if (*expected == '\'' && *(expected + 1) == '\'') {
					c = '\'';
					expected += 2;
				} else if (*expected == '\'') {
					expected++;
					break;
				} else if (*expected == '\0') {
					diag("verify_csi32: Missing end quote");
					return "Syntax error in expected difference";
				} else {
					c = *expected++;
				}
				
				if (!(p < patch_end && *p++ == c))
					return "Insertion string mismatch";
			}
			
			if (patch_size != expected_size)
				return "Insertion string mismatch";
		}
	}
	
	if (p != patch_end)
		return "Patch longer than expected.";
	
	return NULL;
}

static void test_myers(const char *old, const char *new_, const char *expected_difference)
{
	SB patch;
	BDELTAcode rc;
	const char *verify_msg;
	
	if (sb_init(&patch) != 0) {
		fail("Out of memory");
		return;
	}
	
	rc = diff_myers(old, strlen(old), new_, strlen(new_), &patch);
	if (rc != BDELTA_OK) {
		fail("test_myers(%s, %s, %s): diff_myers failed: %s", old, new_, expected_difference, bdelta_strerror(rc));
		sb_discard(&patch, NULL, NULL);
		return;
	}
	
	verify_msg = verify_csi32(patch.start, patch.cur, expected_difference);
	sb_discard(&patch, NULL, NULL);
	
	if (verify_msg != NULL) {
		fail("test_myers(%s, %s, %s): %s", old, new_, expected_difference, verify_msg);
		return;
	}
	
	pass("test_myers(%s, %s, %s)", old, new_, expected_difference);
}

int main(void)
{
	(void) random_string_pair;
	
	plan_tests(17);
	
	test_myers("abcabba", "cbabac",   "s2 c1 i'b' c2 s1 c1 i'c'");
	test_myers("abcdefg", "abcdefg",  "c7");
	test_myers("abcdefg", "abcde",    "c5");
	test_myers("abcdefg", "abcdefga", "c7 i'a'");
	test_myers("abbbbbb", "bbbbbb",   "s1 c6");
	test_myers("abbbbbb", "bbbbbbb",  "s1 c6 i'b'");
	test_myers("bbbbbb",  "abbbbbb",  "i'a' c6");
	test_myers("bbbbbbb", "abbbbbb",  "i'a' c6");
	test_myers("bbbbbb",  "abbbbbbb", "i'a' c6 i'b'");
	
	test_myers("ac",   "ba",   "i'b' c1");
	test_myers("ac",   "bc",   "s1 i'b' c1");
	test_myers("abcd", "cabd", "i'c' c2 s1 c1");
	test_myers("",     "abc",  "i'abc'");
	test_myers("abc",  "",     "");
	test_myers("abcd", "d",    "s3 c1");
	test_myers("", "", "");
	
	/*
	 * In the event the strings have no characters in common, diff_myers will
	 * emit a skip of all the characters in the old string.  Although it is
	 * unnecessary, it is tricky to get rid of.  bdelta_diff will discard the
	 * patch anyway, because it's bigger than a literal.
	 */
	test_myers("aaaaaaa", "bbbbbbb",  "s7 i'bbbbbbb'");
	
	return exit_status();
}

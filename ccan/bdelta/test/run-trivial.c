#include <ccan/bdelta/bdelta.h>
#include <ccan/bdelta/bdelta.c>
#include <ccan/tap/tap.h>

static int test_trivial(const char *old, const char *new_)
{
	void *patch;
	size_t patch_size;
	BDELTAcode rc;
	
	void *new2;
	size_t new2_size;
	
	rc = bdelta_diff(old, strlen(old), new_, strlen(new_), &patch, &patch_size);
	if (rc != BDELTA_OK) {
		bdelta_perror("bdelta_diff", rc);
		return 0;
	}
	
	if (patch_size > strlen(new_) + 1) {
		fprintf(stderr, "bdelta_diff produced a patch larger than a simple literal emitting the new string.\n");
		return 0;
	}
	
	rc = bdelta_patch(old, strlen(old), patch, patch_size, &new2, &new2_size);
	if (rc != BDELTA_OK) {
		bdelta_perror("bdelta_patch", rc);
		return 0;
	}
	
	if (new2_size != strlen(new_) || strcmp(new2, new_) != 0) {
		fprintf(stderr, "patch(old, diff(old, new)) != new\n");
		return 0;
	}
	
	/* Make sure bdelta_diff properly discards unwanted return values. */
	rc = bdelta_diff(old, strlen(old), new_, strlen(new_), NULL, NULL);
	if (rc != BDELTA_OK) {
		bdelta_perror("bdelta_diff (second time)", rc);
		return 0;
	}
	
	free(new2);
	free(patch);
	return 1;
}

int main(void)
{
	plan_tests(13);
	
	ok1(test_trivial("abcabba", "cbabac"));
	ok1(test_trivial("aaabbbcdaabcc", "aaabbcdaabeca"));
	ok1(test_trivial("aaaaaaaa", "bbbbbbbb"));
	ok1(test_trivial("aaaaaaaa", ""));
	ok1(test_trivial("", "bbbbbbbb"));
	ok1(test_trivial("", ""));
	ok1(test_trivial("aaaaaaaa", "aaaaaaaabbbbbbbb"));
	ok1(test_trivial("aaaaaaaa", "bbbbbbbbaaaaaaaa"));
	ok1(test_trivial("aaaaaaaabbbbbbbb", "aaaaaaaa"));
	ok1(test_trivial("aaaaaaaabbbbbbbb", "bbbbbbbb"));
	ok1(test_trivial("aaaaaaaabbbbbbbb", "bbbbbbbb"));
	ok1(test_trivial("abababababababab", "babababababababa"));
	ok1(test_trivial("aababcabcdabcde", "aababcabcdabcde"));
	
	return exit_status();
}

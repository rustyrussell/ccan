#include <ccan/md4/md4.h>
#include <ccan/tap/tap.h>
#include <stdio.h>
#include <string.h>

static void check_md4(const char *string, const char *expected)
{
	struct md4_ctx ctx;
	unsigned int i;
	char result[33];

	md4_init(&ctx);
	md4_hash(&ctx, string, strlen(string));
	md4_finish(&ctx);
	for (i = 0; i < sizeof(ctx.hash.bytes); i++)
		sprintf(result+i*2, "%02x", ctx.hash.bytes[i]);
	ok(strcmp(result, expected) == 0,
	   "Expected %s, got %s", expected, result);
}

int main(int argc, char *argv[])
{
	/* Tests taken from the RFC. */
	plan_tests(7);

	check_md4("", "31d6cfe0d16ae931b73c59d7e0c089c0");
	check_md4("a", "bde52cb31de33e46245e05fbdbd6fb24");
	check_md4("abc", "a448017aaf21d8525fc10ae87aa6729d");
	check_md4("message digest", "d9130a8164549fe818874806e1c7014b");
	check_md4("abcdefghijklmnopqrstuvwxyz",
		  "d79e1c308aa5bbcdeea8ed63df412da9");
	check_md4("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
		   "0123456789",
		  "043f8582f241db351ce627e153e7f0e4");
	check_md4("1234567890123456789012345678901234567890123456789"
		   "0123456789012345678901234567890",
		  "e33b4ddc9c38f2199c3e7b164fcc0536");
	exit(exit_status());
}

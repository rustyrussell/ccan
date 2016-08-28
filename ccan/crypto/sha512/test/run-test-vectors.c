#include <ccan/crypto/sha512/sha512.h>
#include <ccan/str/hex/hex.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Include the C files directly. */
#include <ccan/crypto/sha512/sha512.c>
#include <ccan/tap/tap.h>

/* Test vectors. */
struct test {
	const char *vector;
	size_t repetitions;
	const char *expected;
};

static const char ZEROES[] =
	"0000000000000000000000000000000000000000000000000000000000000000"
	"0000000000000000000000000000000000000000000000000000000000000000";

static struct test tests[] = {
	/* http://csrc.nist.gov/groups/STM/cavp/secure-hashing.html ShortMsg */
	{ "21", 1,
	  "3831a6a6155e509dee59a7f451eb35324d8f8f2df6e3708894740f98fdee2388"
	  "9f4de5adb0c5010dfb555cda77c8ab5dc902094c52de3278f35a75ebc25f093a" },
	{ "9083", 1,
	  "55586ebba48768aeb323655ab6f4298fc9f670964fc2e5f2731e34dfa4b0c09e"
	  "6e1e12e3d7286b3145c61c2047fb1a2a1297f36da64160b31fa4c8c2cddd2fb4" },
	{ "0a55db", 1,
	  "7952585e5330cb247d72bae696fc8a6b0f7d0804577e347d99bc1b11e52f3849"
	  "85a428449382306a89261ae143c2f3fb613804ab20b42dc097e5bf4a96ef919b" },
	{ "23be86d5", 1,
	  "76d42c8eadea35a69990c63a762f330614a4699977f058adb988f406fb0be8f2"
	  "ea3dce3a2bbd1d827b70b9b299ae6f9e5058ee97b50bd4922d6d37ddc761f8eb" },
	{ "eb0ca946c1", 1,
	  "d39ecedfe6e705a821aee4f58bfc489c3d9433eb4ac1b03a97e321a2586b40dd"
	  "0522f40fa5aef36afff591a78c916bfc6d1ca515c4983dd8695b1ec7951d723e" },
	{ "38667f39277b", 1,
	  "85708b8ff05d974d6af0801c152b95f5fa5c06af9a35230c5bea2752f031f9bd"
	  "84bd844717b3add308a70dc777f90813c20b47b16385664eefc88449f04f2131" },
	{ "b39f71aaa8a108", 1,
	  "258b8efa05b4a06b1e63c7a3f925c5ef11fa03e3d47d631bf4d474983783d8c0"
	  "b09449009e842fc9fa15de586c67cf8955a17d790b20f41dadf67ee8cdcdfce6" },
	{ "dc28484ebfd293d62ac759d5754bdf502423e4d419fa79020805134b2ce3dff7"
	  "38c7556c91d810adbad8dd210f041296b73c2185d4646c97fc0a5b69ed49ac8c"
	  "7ced0bd1cfd7e3c3cca47374d189247da6811a40b0ab097067ed4ad40ade2e47"
	  "91e39204e398b3204971445822a1be0dd93af8", 1,
	  "615115d2e8b62e345adaa4bdb95395a3b4fe27d71c4a111b86c1841463c5f03d"
	  "6b20d164a39948ab08ae060720d05c10f6022e5c8caf2fa3bca2e04d9c539ded" },
	{ "fd2203e467574e834ab07c9097ae164532f24be1eb5d88f1af7748ceff0d2c67"
	  "a21f4e4097f9d3bb4e9fbf97186e0db6db0100230a52b453d421f8ab9c9a6043"
	  "aa3295ea20d2f06a2f37470d8a99075f1b8a8336f6228cf08b5942fc1fb4299c"
	  "7d2480e8e82bce175540bdfad7752bc95b577f229515394f3ae5cec870a4b2f8",
	  1,
	  "a21b1077d52b27ac545af63b32746c6e3c51cb0cb9f281eb9f3580a6d4996d5c"
	  "9917d2a6e484627a9d5a06fa1b25327a9d710e027387fc3e07d7c4d14c6086cc" },
	/* http://www.di-mgt.com.au/sha_testvectors.html */
	{ ZEROES, 1,
	  "7be9fda48f4179e611c698a73cff09faf72869431efee6eaad14de0cb44bbf66"
	  "503f752b7a8eb17083355f3ce6eb7d2806f236b25af96a24e22b887405c20081" }
#if 0 /* This test is rather slow */
	,
	{ ZEROES, 100000,
	  "23b8521df55569c4e55c7be36d4ad106e338b0799d5e105058aaa1a95737c25d"
	  "b77af240849ae7283ea0b7cbf196f5e4bd78aca19af97eb6e364ada4d12c1178" }
#endif
};

static void *xmalloc(size_t size)
{
	char * ret;
	ret = malloc(size);
	if (ret == NULL) {
		perror("malloc");
		abort();
	}
	return ret;
}

static bool do_test(const struct test *t)
{
	struct sha512 h;
	char got[128 + 1];
	bool passed;
	size_t i, vector_len = strlen(t->vector) / 2;
	void *vector = xmalloc(vector_len);

	hex_decode(t->vector, vector_len * 2, vector, vector_len);

	for (i = 0; i < t->repetitions; i++) {
		sha512(&h, vector, vector_len);
		if (t->repetitions > 1)
			memcpy(vector, &h, sizeof(h));
	}

	hex_encode(&h, sizeof(h), got, sizeof(got));

	passed = strcmp(t->expected, got) == 0;
	free(vector);
	return passed;
}

int main(void)
{
	const size_t num_tests = sizeof(tests) / sizeof(tests[0]);
	size_t i;

	/* This is how many tests you plan to run */
	plan_tests(num_tests);

	for (i = 0; i < num_tests; i++)
		ok1(do_test(&tests[i]));

	/* This exits depending on whether all tests passed */
	return exit_status();
}

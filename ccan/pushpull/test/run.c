#include <ccan/pushpull/pushpull.h>
/* Include the C files directly. */
#include <ccan/pushpull/push.c>
#include <ccan/pushpull/pull.c>
#include <ccan/tap/tap.h>

struct foo {
	uint64_t vu64;
	uint32_t vu32;
	uint16_t vu16;
	uint8_t vu8;
	unsigned char vuchar;
	int64_t vs64;
	int32_t vs32;
	int16_t vs16;
	int8_t vs8;
	char vchar;
	char bytes[100];
};

static void *fail_reallocfn(void *ptr, size_t size)
{
	return NULL;
}

static bool push_foo(char **p, size_t *len, const struct foo *foo)
{
	return push_u64(p, len, foo->vu64) &&
		push_u32(p, len, foo->vu32) &&
		push_u16(p, len, foo->vu16) &&
		push_u8(p, len, foo->vu8) &&
		push_uchar(p, len, foo->vuchar) &&
		push_s64(p, len, foo->vs64) &&
		push_s32(p, len, foo->vs32) &&
		push_s16(p, len, foo->vs16) &&
		push_s8(p, len, foo->vs8) &&
		push_char(p, len, foo->vchar) &&
		push_bytes(p, len, foo->bytes, sizeof(foo->bytes));
}

static bool pull_foo(const char **p, size_t *len, struct foo *foo)
{
	int ret;

	ret = pull_u64(p, len, &foo->vu64) +
		pull_u32(p, len, &foo->vu32) +
		pull_u16(p, len, &foo->vu16) +
		pull_u8(p, len, &foo->vu8) +
		pull_uchar(p, len, &foo->vuchar) +
		pull_s64(p, len, &foo->vs64) +
		pull_s32(p, len, &foo->vs32) +
		pull_s16(p, len, &foo->vs16) +
		pull_s8(p, len, &foo->vs8) +
		pull_char(p, len, &foo->vchar) +
		pull_bytes(p, len, foo->bytes, sizeof(foo->bytes));

	if (ret != 11)
		ok1(len == 0 && *p == NULL);
	return ret == 11;
}

static bool foo_equal(const struct foo *f1, const struct foo *f2)
{
	return f1->vu64 == f2->vu64 &&
		f1->vu32 == f2->vu32 &&
		f1->vu16 == f2->vu16 &&
		f1->vu8 == f2->vu8 &&
		f1->vuchar == f2->vuchar &&
		f1->vs64 == f2->vs64 &&
		f1->vs32 == f2->vs32 &&
		f1->vs16 == f2->vs16 &&
		f1->vs8 == f2->vs8 &&
		f1->vchar == f2->vchar &&
		memcmp(f1->bytes, f2->bytes, sizeof(f1->bytes)) == 0;
}

int main(void)
{
	char *buffer;
	const char *p;
	size_t len, left;
	struct foo *foo, *foo2;

	/* This is how many tests you plan to run */
	plan_tests(17);

	/* Valgrind will make sure we don't read padding! */
	foo = malloc(sizeof(*foo));
	foo->vu64 = 0x01020304050607ULL;
	foo->vu32 = 0x08090a0b;
	foo->vu16 = 0x0c0d;
	foo->vu8 = 0x0e;
	foo->vuchar = 0x0f;
	foo->vs64 = -0x1011121314151617LL;
	foo->vs32 = -0x18191a1b;
	foo->vs16 = -0x1c1d;
	foo->vs8 = -0x1e;
	foo->vchar = -0x1f;
	memset(foo->bytes, 0x20, sizeof(foo->bytes));
	strcpy(foo->bytes, "This is a test");

	buffer = malloc(1);
	len = 0;
	ok1(push_foo(&buffer, &len, foo));
	ok1(len <= sizeof(*foo));

	/* Triggers valgrind's uninitialized value warning */
	ok1(!memchr(buffer, 0x21, len));

	p = buffer;
	left = len;
	foo2 = malloc(sizeof(*foo2));
	ok1(pull_foo(&p, &left, foo2));
	ok1(left == 0);
	ok1(p == buffer + len);
	ok1(foo_equal(foo, foo2));

	/* Too-small for pull, it should fail and set ptr/len to 0 */
	p = buffer;
	left = 0;
	ok1(!pull_u64(&p, &left, &foo2->vu64));
	ok1(p == NULL && left == 0);
	/* Shouldn't change field! */
	ok1(foo_equal(foo, foo2));

	left = 7;
	ok1(!pull_u64(&p, &left, &foo2->vu64));
	ok1(p == NULL && left == 0);
	/* Shouldn't change field! */
	ok1(foo_equal(foo, foo2));

	/* Discard should work. */
	left = len;
	ok1(pull_bytes(&p, &left, NULL, sizeof(foo->bytes)));
	ok1(left == len - sizeof(foo->bytes));

	/* Push failures should be clean. */
	push_set_realloc(fail_reallocfn);
	p = buffer;
	left = len;
	ok1(!push_u64(&buffer, &left, foo->vu64));
	ok1(p == buffer && left == len);

	free(buffer);
	free(foo);
	free(foo2);

	/* This exits depending on whether all tests passed */
	return exit_status();
}

#include <ccan/str_talloc/str_talloc.h>
#include <ccan/str_talloc/str_talloc.c>
#include <ccan/tap/tap.h>

int main(int argc, char *argv[])
{
	void *ctx = talloc_init("toplevel");
	unsigned int top_blocks = talloc_total_blocks(ctx);
	char *a, *b;
	/* If it accesses this, it will crash. */
	char **invalid = (char **)1L;

	plan_tests(25);
	/* Simple matching. */
	ok1(strreg(ctx, "hello world!", "hello") == true);
	ok1(strreg(ctx, "hello world!", "hi") == false);

	/* No parentheses means we don't use any extra args. */
	ok1(strreg(ctx, "hello world!", "hello", invalid) == true);
	ok1(strreg(ctx, "hello world!", "hi", invalid) == false);

	ok1(strreg(ctx, "hello world!", "[a-z]+", invalid) == true);
	ok1(strreg(ctx, "hello world!", "([a-z]+)", &a, invalid) == true);
	/* Found string */
	ok1(streq(a, "hello"));
	/* Allocated off ctx */
	ok1(talloc_find_parent_byname(a, "toplevel") == ctx);
	talloc_free(a);

	ok1(strreg(ctx, "hello world!", "([a-z]*) ([a-z]+)",
		   &a, &b, invalid) == true);
	ok1(streq(a, "hello"));
	ok1(streq(b, "world"));
	ok1(talloc_find_parent_byname(a, "toplevel") == ctx);
	ok1(talloc_find_parent_byname(b, "toplevel") == ctx);
	talloc_free(a);
	talloc_free(b);

	/* * after parentheses returns last match. */
	ok1(strreg(ctx, "hello world!", "([a-z])* ([a-z]+)",
		   &a, &b, invalid) == true);
	ok1(streq(a, "o"));
	ok1(streq(b, "world"));
	talloc_free(a);
	talloc_free(b);

	/* Nested parentheses are ordered by open brace. */
	ok1(strreg(ctx, "hello world!", "(([a-z]*) world)",
		   &a, &b, invalid) == true);
	ok1(streq(a, "hello world"));
	ok1(streq(b, "hello"));
	talloc_free(a);
	talloc_free(b);

	/* Nested parentheses are ordered by open brace. */
	ok1(strreg(ctx, "hello world!", "(([a-z]*) world)",
		   &a, &b, invalid) == true);
	ok1(streq(a, "hello world"));
	ok1(streq(b, "hello"));
	talloc_free(a);
	talloc_free(b);

	/* NULL means we're not interested. */
	ok1(strreg(ctx, "hello world!", "((hello|goodbye) world)",
		   &a, NULL, invalid) == true);
	ok1(streq(a, "hello world"));
	talloc_free(a);

	/* No leaks! */
	ok1(talloc_total_blocks(ctx) == top_blocks);
	talloc_free(ctx);
	talloc_disable_null_tracking();

	return exit_status();
}				

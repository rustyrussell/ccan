#include <ccan/tap/tap.h>
#include <ccan/opt/opt.c>
#include <ccan/opt/usage.c>
#include <ccan/opt/helpers.c>
#include <ccan/opt/parse.c>

/* Test consume_words helper. */
int main(int argc, char *argv[])
{
	unsigned int start, len;

	plan_tests(13);

	/* Every line over width. */
	len = consume_words("hello world", 1, &start);
	ok1(start == 0);
	ok1(len == strlen("hello"));
	len = consume_words(" world", 1, &start);
	ok1(start == 1);
	ok1(len == strlen("world"));
	ok1(consume_words("", 1, &start) == 0);

	/* Same with width where won't both fit. */
	len = consume_words("hello world", 5, &start);
	ok1(start == 0);
	ok1(len == strlen("hello"));
	len = consume_words(" world", 5, &start);
	ok1(start == 1);
	ok1(len == strlen("world"));
	ok1(consume_words("", 5, &start) == 0);

	len = consume_words("hello world", 11, &start);
	ok1(start == 0);
	ok1(len == strlen("hello world"));
	ok1(consume_words("", 11, &start) == 0);
	return exit_status();
}

#include <ccan/tok_itr/tok_itr.h>
#include <ccan/tap/tap.h>

#include <string.h>

int main(int argc, char *argv[]) {
	const char *arr[] = {"/bi", "/us", "/sb", "/us" };
	char val[4];
	int i;
	struct tok_itr itr;

	plan_tests(4 + 1);

	diag("++++trunc++++");
	diag("test token truncation due to small output buffer");

	i = 0;
	TOK_ITR_FOREACH(&itr, val, 4, "/bin:/usr/bin:/sbin:/usr/local/bin", ':') {
		diag("token = %s", val);
		ok1( strcmp(val, arr[i++]) == 0 );
	}	
	ok1( tok_itr_partial_val(&itr) == true );

	diag("----trunc----\n#");

	return exit_status();
}

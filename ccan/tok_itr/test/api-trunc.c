#include <ccan/tok_itr/tok_itr.h>
#include <ccan/tap/tap.h>

#include <string.h>

int main(int argc, char *argv[]) {
	const char *arr[] = {"/bi", "/us", "/sb", "/us" };
	char val[4];
	int i;
	struct tok_itr itr;

	plan_tests(4);

	diag("++++trunc++++");
	diag("test token truncation due to small output buffer");

	i = 0;
	TOK_ITR_FOREACH(val, 4, "/bin:/usr/bin:/sbin:/usr/local/bin", ':', &itr) {
		diag("token = %s", val);
		ok1( strcmp(val, arr[i++]) == 0 );
	}	

	diag("----trunc----\n#");

	return exit_status();
}

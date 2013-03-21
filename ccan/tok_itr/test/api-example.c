#include <ccan/tok_itr/tok_itr.h>
#include <ccan/tap/tap.h>

#include <string.h>

int main(int argc, char *argv[]) {
	const char *arr[] = {"/bin", "/usr/bin", "/sbin", "/usr/local/bin" };
	char val[32];
	int i;
	struct tok_itr itr;

	plan_tests(4);
	diag("++++example++++");
	diag("test token iterator use case/example");

	i = 0;
	TOK_ITR_FOREACH(val, 32, "/bin:/usr/bin:/sbin:/usr/local/bin", ':', &itr) {
		diag("token = %s", val);
		ok1( strcmp(val, arr[i++]) == 0 );
	}

	diag("----example----\n#");

	return exit_status();
}

#include <ccan/tok_itr/tok_itr.h>
#include <ccan/tap/tap.h>

#include <string.h>

int main(int argc, char *argv[]) {
	const char *arr[] = {"/bin", "/usr/bin", "/sbin", "/usr/local/bin" };
	char val[32];
	int i;
	struct tok_itr itr;

	plan_tests(2*4 + 1);
	diag("++++example++++");
	diag("test token iterator use case/example");

	i = 0;
	TOK_ITR_FOREACH(&itr, val, 32, "/bin:/usr/bin:/sbin:/usr/local/bin:", ':') {
		diag("token = %s", val);
		if(i > 3)
			continue;

		ok1( tok_itr_val_len(&itr) == strlen(arr[i]) );
		ok1( strcmp(val, arr[i++]) == 0 );
	}
	ok1( tok_itr_partial_val(&itr) == false );
	
	diag("----example----\n#");

	return exit_status();
}

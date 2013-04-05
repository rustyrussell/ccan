#include <ccan/tok_itr/tok_itr.h>
#include <ccan/tap/tap.h>

#include <string.h>

int main(int argc, char *argv[]) {
#	define TOK1 "blah"
#	define TOK2 "@#$%zlk"
#	define TOK3 "(*aksjhdn$"
#	define TOK4 "\"///\\zxkkkkkhs)(*&"
	const char *arr[] = {TOK1, TOK2, TOK3, TOK4 };
	char str[128];
	char val[32];
	int i;
	struct tok_itr itr;

	plan_tests(2*4 + 1 + 1);

	diag("++++foreach++++");
#	define TOKALL TOK1 ":" TOK2 ":" TOK3 ":" TOK4
	strncpy(str, TOKALL, 128);
	/*diag("str: %s\n", str);*/
	diag("test token iterator FOREACH basics");

	i = 0;
	TOK_ITR_FOREACH(&itr, val, 32, str, ':') {
		ok1( tok_itr_val_len(&itr) == strlen(arr[i]) );
		ok1( strcmp(val, arr[i++]) == 0 );
	}	
	ok1( tok_itr_partial_val(&itr) == true );
	
	diag("test whether iterator modified the string it iterated over");
	ok1( strcmp(str, TOKALL) == 0 );

	diag("----foreach----\n#");

	return exit_status();
}
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

	plan_tests(4 + 1);

	diag("++++foreach++++");
#	define TOKALL TOK1 ":" TOK2 ":" TOK3 ":" TOK4
	strncpy(str, TOKALL, 128);
	/*diag("str: %s\n", str);*/
	diag("test token iterator FOREACH basics");

	i = 0;
	TOK_ITR_FOREACH(val, 32, str, ':', &itr) {
		ok1( strcmp(val, arr[i++]) == 0 );
	}	
	
	diag("test whether iterator modified the string it iterated over");
	ok1( strcmp(str, TOKALL) == 0 );

	diag("----foreach----\n#");

	return exit_status();
}
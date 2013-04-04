#include <ccan/tok_itr/tok_itr.h>
#include <ccan/tap/tap.h>

#include <string.h>

int main(int argc, char *argv[]) {
	struct tok_itr itr;
	int i;

	plan_tests(2*3 + 1 + 1);

	diag("++++zero_val++++");
	diag("test tok_itr_val with zero size val buffer");

	i = 0;
	for(tok_itr_init(&itr, "ab&cd&ef", '&'); !tok_itr_end(&itr); tok_itr_next(&itr) ) {
		ok1( tok_itr_val_len(&itr) == 2 );
		ok1( tok_itr_val(&itr, NULL, 0) == 2 );	
		i++;
	}	
	ok1( tok_itr_partial_val(&itr) == true );

	ok1(i == 3);
	diag("----zero_val----");

	return exit_status();
}

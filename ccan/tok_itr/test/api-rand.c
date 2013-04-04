#include <ccan/tok_itr/tok_itr.h>
#include <ccan/tap/tap.h>

#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>


int main(int argc, char *argv[]) {
#	define NUM_DELIMS 48
#	define NUM_TOKENS (NUM_DELIMS + 1)
	char arr[NUM_TOKENS][1024];;
	char str[1024];
	char val[1024];
	struct tok_itr itr;
	int i;
	char delim = ':';
	char *cptr;

	plan_tests(3*NUM_TOKENS);

	srand(time(NULL));

	diag("++++rand++++");
	
	for(i = 0; i < 1023; i++) {
		char chr;
		do {
			chr = (rand() % 256);
		} while(chr == delim || chr == '\0');
		str[i] = chr;
	}
	str[i] = '\0';

	for(i = 0; i < NUM_DELIMS; i++) {
		int pos;
		do {
			pos = rand() % 1023; 
		} while(str[pos] == '\0');		
		str[pos] = '\0';
	}
	
	i = 0;
	for(cptr = str; cptr < (str+1024); cptr += (strlen(cptr)+1) ) {
		assert(i < NUM_TOKENS);
		strcpy(arr[i], cptr);
		/*diag("%d: %s\n", i, arr[i]);*/
		i++;
	}

	for(i = 0; i < 1023; i++)
		if(str[i] == '\0')
			str[i] = delim;

	/*diag("str: %s\n", str);*/
	diag("test token iterator with random token string");
	
	i = 0;
	for(tok_itr_init(&itr, str, delim); !tok_itr_end(&itr); tok_itr_next(&itr) ) {
		ok1( tok_itr_val_len(&itr) == strlen(arr[i]) );
		ok1( tok_itr_val(&itr, val, 1024) == strlen(arr[i]) );
		ok1( strcmp(val, arr[i++]) == 0 );
		/*diag("val: %s\n", val);*/
	}	
	

	diag("----rand----\n#");

	return exit_status();
}

#include <ccan/tok_itr/tok_itr.h>
#include <ccan/tap/tap.h>

#include <string.h>

int main(int argc, char *argv[]) {
	char tok1[64];
	char tok2[64];
	char tok3[64];
	char tok4[64];
	char tok5[64];
	const char *arr[] = {tok1, tok2, tok3, tok4, tok5, ""};
	char str[512];
	char val[32];
	struct tok_itr itr;
	int i;

	plan_tests(3*6 + 1);

	diag("++++big++++");
	
	str[0] = '\0';

#	define TOK_SET(_str, _chr, _amt) \
		memset(_str, _chr, _amt); \
		_str[_amt] = ':'; \
		_str[_amt+1] = '\0';

	TOK_SET(tok1, 'A', 29);
	TOK_SET(tok2, 'B', 30);
	TOK_SET(tok3, 'C', 31);
	TOK_SET(tok4, 'D', 32);
	TOK_SET(tok5, 'E', 33);

	for(i = 0; i < 5; i++) {
		/*diag("tok%d: %s\n", i+1, arr[i]);*/
		strcat(str, arr[i]);
	}

	/*diag("str: %s\n", str);*/
	diag("test token iterator with big tokens");

	i = 0;
	for(tok_itr_init(&itr, str, ':'); !tok_itr_end(&itr); tok_itr_next(&itr) ) {
		size_t tlen = tok_itr_val_len(&itr);
		int len = strlen(arr[i]);
		int cmplen;

		ok1(tok_itr_val(&itr, val, 32) == tlen);
		if(arr[i][len-1] == ':')
			len--;

		cmplen = len;
		if(len > 31) {
			diag("tok4 and tok5 should be too big (size > 31)");
			cmplen = 31;
		}

		ok1(tlen == len);

		/*diag("%d: '%s'\t(%s) %d\n", i, val, str, strlen(arr[i]) );*/
		ok1( strncmp(val, arr[i++], cmplen) == 0 );
	}	
	ok1( tok_itr_partial_val(&itr) == false );
	
	diag("----big----\n#");

	return exit_status();
}
